/* $Id$ */
/* 
 * Copyright (C) 2003-2007 Benny Prijono <benny@prijono.org>
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
#include <pjnath/errno.h>
#include <pj/string.h>



/* PJNATH's own error codes/messages 
 * MUST KEEP THIS ARRAY SORTED!!
 * Message must be limited to 64 chars!
 */
#if defined(PJ_HAS_ERROR_STRING) && PJ_HAS_ERROR_STRING!=0
static const struct 
{
    int code;
    const char *msg;
} err_str[] = 
{
    /* STUN */
    PJ_BUILD_ERR( PJNATH_ESTUNTOOMANYATTR,  "Too many STUN attributes"),
    PJ_BUILD_ERR( PJNATH_ESTUNUNKNOWNATTR,  "Unknown STUN attribute"),
    PJ_BUILD_ERR( PJNATH_ESTUNINADDRLEN,    "Invalid STUN socket address length"),
    PJ_BUILD_ERR( PJNATH_ESTUNIPV6NOTSUPP,  "STUN IPv6 attribute not supported"),
    PJ_BUILD_ERR( PJNATH_ESTUNNOTRESPONSE,  "Expecting STUN response message"),
    PJ_BUILD_ERR( PJNATH_ESTUNINVALIDID,    "STUN transaction ID mismatch"),
    PJ_BUILD_ERR( PJNATH_ESTUNNOHANDLER,    "Unable to find STUN handler for the request"),
    PJ_BUILD_ERR( PJNATH_ESTUNMSGINTPOS,    "Found non-FINGERPRINT attr. after MESSAGE-INTEGRITY"),
    PJ_BUILD_ERR( PJNATH_ESTUNFINGERPOS,    "Found STUN attribute after FINGERPRINT"),
    PJ_BUILD_ERR( PJNATH_ESTUNNOUSERNAME,   "Missing STUN USERNAME attribute"),
    PJ_BUILD_ERR( PJNATH_ESTUNMSGINT,	    "Missing/invalid STUN MESSAGE-INTEGRITY attribute"),
    PJ_BUILD_ERR( PJNATH_ESTUNDUPATTR,	    "Found duplicate STUN attribute"),
    PJ_BUILD_ERR( PJNATH_ESTUNNOREALM,	    "Missing STUN REALM attribute"),
    PJ_BUILD_ERR( PJNATH_ESTUNNONCE,	    "Missing/stale STUN NONCE attribute value"),
    PJ_BUILD_ERR( PJNATH_ESTUNTSXFAILED,    "STUN transaction terminates with failure"),
};
#endif	/* PJ_HAS_ERROR_STRING */


/*
 * pjnath_strerror()
 */
PJ_DEF(pj_str_t) pjnath_strerror( pj_status_t statcode, 
				  char *buf, pj_size_t bufsize )
{
    pj_str_t errstr;

#if defined(PJ_HAS_ERROR_STRING) && (PJ_HAS_ERROR_STRING != 0)

    if (statcode >= PJNATH_ERRNO_START && 
	statcode < PJNATH_ERRNO_START + PJ_ERRNO_SPACE_SIZE)
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

#endif	/* PJ_HAS_ERROR_STRING */


    /* Error not found. */
    errstr.ptr = buf;
    errstr.slen = pj_ansi_snprintf(buf, bufsize, 
				   "Unknown pjlib-util error %d",
				   statcode);

    return errstr;
}


PJ_DEF(pj_status_t) pjnath_init(void)
{
    return pj_register_strerror(PJNATH_ERRNO_START, 
				PJ_ERRNO_SPACE_SIZE, 
				&pjnath_strerror);
}
