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
#include <pjsip/sip_errno.h>
#include <pjsip/sip_msg.h>
#include <pj/string.h>
#include <pj/errno.h>

/* PJSIP's own error codes/messages 
 * MUST KEEP THIS ARRAY SORTED!!
 */
static const struct 
{
    int code;
    const char *msg;
} err_str[] = 
{
    /* Generic SIP errors */
    { PJSIP_EBUSY,		"Object is busy" },
    { PJSIP_ETYPEEXISTS ,	"Object with the same type exists" },

    /* Messaging errors */
    { PJSIP_EINVALIDMSG,	"Invalid message/syntax error" },
    { PJSIP_EINVALIDSCHEME,	"Invalid URI scheme" },
    { PJSIP_EMSGTOOLONG,	"Message too long" },
    { PJSIP_EPARTIALMSG,	"Partial message" },
    { PJSIP_EMISSINGREQURI,	"Missing Request-URI" },
    { PJSIP_EMISSINGHDR,	"Missing required header(s)" },
    { PJSIP_EMISSINGBODY,	"Missing message body" },
    { PJSIP_EINVALIDVIA,	"Invalid Via header" },
    { PJSIP_EMULTIPLEVIA,	"Multiple Via headers in response" },
    { PJSIP_EINVALIDREQURI,	"Invalid Request URI" },
    { PJSIP_ENOTREQUESTMSG,	"Expecting request message"},
    { PJSIP_ENOTRESPONSEMSG,	"Expecting response message"},
    { PJSIP_EINVALIDHDR,	"Invalid header field"},

    /* Transport errors */
    { PJSIP_EUNSUPTRANSPORT,	"Unsupported transport"},
    { PJSIP_EPENDINGTX,		"Transmit buffer already pending"},
    { PJSIP_ERXOVERFLOW,	"Rx buffer overflow"},
    { PJSIP_EBUFDESTROYED,	"Buffer destroyed"},

    /* Transaction errors */
    { PJSIP_ETSXDESTROYED,	"Transaction has been destroyed"},
};


/*
 * pjsip_strerror()
 */
PJ_DEF(pj_str_t) pjsip_strerror( pj_status_t statcode, 
				 char *buf, pj_size_t bufsize )
{
    pj_str_t errstr;

    if (statcode >= PJSIP_ERRNO_START && statcode < PJSIP_ERRNO_START+800) 
    {
	/* Status code. */
	const pj_str_t *status_text = 
	    pjsip_get_status_text(PJSIP_ERRNO_TO_SIP_STATUS(statcode));

	errstr.ptr = buf;
	pj_strncpy_with_null(&errstr, status_text, bufsize);
	return errstr;
    }
    else if (statcode >= PJSIP_ERRNO_START_PJSIP && 
	     statcode < PJSIP_ERRNO_START_PJSIP + 1000)
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
	    msg.slen = pj_native_strlen(err_str[first].msg);

	    errstr.ptr = buf;
	    pj_strncpy_with_null(&errstr, &msg, bufsize);
	    return errstr;

	} else {
	    /* Error not found. */
	    errstr.ptr = buf;
	    errstr.slen = pj_snprintf(buf, bufsize, 
				      "Unknown error %d",
				      statcode);

	    return errstr;
	}
    }
    else {
	/* Not our code. Give it to PJLIB. */
	return pj_strerror(statcode, buf, bufsize);
    }

}

