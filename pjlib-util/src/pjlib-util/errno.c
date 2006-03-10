/* $Id$ */
/* 
 * Copyright (C) 2003-2006 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <pjlib-util/errno.h>
#include <pj/string.h>



/* PJLIB_UTIL's own error codes/messages 
 * MUST KEEP THIS ARRAY SORTED!!
 * Message must be limited to 64 chars!
 */
static const struct 
{
    int code;
    const char *msg;
} err_str[] = 
{
    /* STUN errors */
    { PJLIB_UTIL_ESTUNRESOLVE,	    "Unable to resolve STUN server" },
    { PJLIB_UTIL_ESTUNINMSGTYPE,    "Unknown STUN message type" },
    { PJLIB_UTIL_ESTUNINMSGLEN,	    "Invalid STUN message length" },
    { PJLIB_UTIL_ESTUNINATTRLEN,    "STUN attribute length error" },
    { PJLIB_UTIL_ESTUNINATTRTYPE,   "Invalid STUN attribute type" },
    { PJLIB_UTIL_ESTUNININDEX,	    "Invalid STUN server/socket index" },
    { PJLIB_UTIL_ESTUNNOBINDRES,    "No STUN binding response in the message" },
    { PJLIB_UTIL_ESTUNRECVERRATTR,  "Received STUN error attribute" },
    { PJLIB_UTIL_ESTUNNOMAP,	    "No STUN mapped address attribute" },
    { PJLIB_UTIL_ESTUNNOTRESPOND,   "Received no response from STUN server" },
    { PJLIB_UTIL_ESTUNSYMMETRIC,    "Symetric NAT detected by STUN" },

    /* XML errors */
    { PJLIB_UTIL_EINXML,	    "Invalid XML message" },
};



/*
 * pjlib_util_strerror()
 */
PJ_DEF(pj_str_t) pjlib_util_strerror( pj_status_t statcode, 
				      char *buf, pj_size_t bufsize )
{
    pj_str_t errstr;

    if (statcode >= PJLIB_UTIL_ERRNO_START && 
	statcode < PJLIB_UTIL_ERRNO_START + PJ_ERRNO_SPACE_SIZE)
    {
	/* Find the error in the table.
	 * Use binary search!
	 */
	int first = 0;
	int n = PJ_ARRAY_SIZE(err_str);

	while (n > 0) {
	    int half = n/2;
	    int mid = first + half;

	    if (err_str[mid].code < statcode) {
		first = mid+1;
		n -= (half+1);
	    } else if (err_str[mid].code > statcode) {
		n = half;
	    } else {
		first = mid;
		break;
	    }
	}


	if (PJ_ARRAY_SIZE(err_str) && err_str[first].code == statcode) {
	    pj_str_t msg;
	    
	    msg.ptr = (char*)err_str[first].msg;
	    msg.slen = pj_ansi_strlen(err_str[first].msg);

	    errstr.ptr = buf;
	    pj_strncpy_with_null(&errstr, &msg, bufsize);
	    return errstr;

	} 
    }

    /* Error not found. */
    errstr.ptr = buf;
    errstr.slen = pj_ansi_snprintf(buf, bufsize, 
				   "Unknown error %d",
				   statcode);

    return errstr;
}


PJ_DEF(pj_status_t) pjlib_util_init(void)
{
    return pj_register_strerror(PJLIB_UTIL_ERRNO_START, 
				PJ_ERRNO_SPACE_SIZE, 
				&pjlib_util_strerror);
}
