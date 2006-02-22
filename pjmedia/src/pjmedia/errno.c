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
#include <pjmedia/errno.h>
#include <pj/string.h>



/* PJMEDIA's own error codes/messages 
 * MUST KEEP THIS ARRAY SORTED!!
 * Message must be limited to 64 chars!
 */
static const struct 
{
    int code;
    const char *msg;
} err_str[] = 
{
    /* Generic PJMEDIA errors, shouldn't be used! */
    { PJMEDIA_ERROR,		    "Unspecified PJMEDIA error" },

    /* SDP error. */
    { PJMEDIA_SDP_EINSDP,	    "Invalid SDP descriptor" },
    { PJMEDIA_SDP_EINVER,	    "Invalid SDP version line" },
    { PJMEDIA_SDP_EINORIGIN,	    "Invalid SDP origin line" },
    { PJMEDIA_SDP_EINTIME,	    "Invalid SDP time line"},
    { PJMEDIA_SDP_EINNAME,	    "SDP name/subject line is empty"},
    { PJMEDIA_SDP_EINCONN,	    "Invalid SDP connection line"},
    { PJMEDIA_SDP_EMISSINGCONN,	    "Missing SDP connection info line"},
    { PJMEDIA_SDP_EINATTR,	    "Invalid SDP attributes"},
    { PJMEDIA_SDP_EINRTPMAP,	    "Invalid SDP rtpmap attribute"},
    { PJMEDIA_SDP_ERTPMAPTOOLONG,   "SDP rtpmap attribute too long"},
    { PJMEDIA_SDP_EMISSINGRTPMAP,   "Missing SDP rtpmap for dynamic payload type"},
    { PJMEDIA_SDP_EINMEDIA,	    "Invalid SDP media line" },
    { PJMEDIA_SDP_ENOFMT,	    "No SDP payload format in the media line" },
    { PJMEDIA_SDP_EINPT,	    "Invalid SDP payload type in media line" },
    { PJMEDIA_SDP_EINFMTP,	    "Invalid SDP fmtp attribute" },

    /* SDP negotiator errors. */
    { PJMEDIA_SDPNEG_EINSTATE,	    "Invalid SDP negotiator state for operation" },
    { PJMEDIA_SDPNEG_ENOINITIAL,    "No initial local SDP in SDP negotiator" },
    { PJMEDIA_SDPNEG_ENOACTIVE,	    "No active SDP in SDP negotiator" },
    { PJMEDIA_SDPNEG_ENONEG,	    "No current local/remote offer/answer" },
    { PJMEDIA_SDPNEG_EMISMEDIA,	    "SDP media count mismatch in offer/answer" },
    { PJMEDIA_SDPNEG_EINVANSMEDIA,  "SDP media type mismatch in offer/answer" },
    { PJMEDIA_SDPNEG_EINVANSTP,	    "SDP media transport type mismatch in offer/answer" },
    { PJMEDIA_SDPNEG_EANSNOMEDIA,   "No common SDP media payload in answer" },
    { PJMEDIA_SDPNEG_ENOMEDIA,	    "No active media stream after negotiation" },

    /* SDP comparison results */
    { PJMEDIA_SDP_EMEDIANOTEQUAL,   "SDP media descriptor not equal" },
    { PJMEDIA_SDP_EPORTNOTEQUAL,    "Port in SDP media descriptor not equal" },
    { PJMEDIA_SDP_ETPORTNOTEQUAL,   "Transport in SDP media descriptor not equal" },
    { PJMEDIA_SDP_EFORMATNOTEQUAL,  "Format in SDP media descriptor not equal" },
    { PJMEDIA_SDP_ECONNNOTEQUAL,    "SDP connection line not equal" },
    { PJMEDIA_SDP_EATTRNOTEQUAL,    "SDP attributes not equal" },
    { PJMEDIA_SDP_EDIRNOTEQUAL,	    "SDP media direction not equal" },
    { PJMEDIA_SDP_EFMTPNOTEQUAL,    "SDP fmtp attribute not equal" },
    { PJMEDIA_SDP_ERTPMAPNOTEQUAL,  "SDP rtpmap attribute not equal" },
    { PJMEDIA_SDP_ESESSNOTEQUAL,    "SDP session descriptor not equal" },
    { PJMEDIA_SDP_EORIGINNOTEQUAL,  "SDP origin line not equal" },
    { PJMEDIA_SDP_ENAMENOTEQUAL,    "SDP name/subject line not equal" },
    { PJMEDIA_SDP_ETIMENOTEQUAL,    "SDP time line not equal" },

    /* Codec errors. */
    { PJMEDIA_CODEC_EUNSUP,	    "Unsupported media codec" },
    { PJMEDIA_CODEC_EFAILED,	    "Codec internal creation error" },
    { PJMEDIA_CODEC_EFRMTOOSHORT,   "Codec frame is too short" },
    { PJMEDIA_CODEC_EPCMTOOSHORT,   "PCM frame is too short" },

    /* Media errors. */
    { PJMEDIA_EINVALIDIP,	    "Invalid remote media (IP) address" },
    { PJMEDIA_EASYMCODEC,	    "Asymetric media codec is not supported" },
    { PJMEDIA_EINVALIDPT,	    "Invalid media payload type" },
    { PJMEDIA_EMISSINGRTPMAP,	    "Missing rtpmap in media description" },
    { PJMEDIA_EINVALIMEDIATYPE,	    "Invalid media type" },
    { PJMEDIA_EREMOTENODTMF,	    "Remote does not support DTMF" },
    { PJMEDIA_RTP_EINDTMF,	    "Invalid DTMF digit" },

    /* RTP session errors. */
    { PJMEDIA_RTP_EINPKT,	    "Invalid RTP packet" },
    { PJMEDIA_RTP_EINPACK,	    "Invalid RTP packing (internal error)" },
    { PJMEDIA_RTP_EINVER,	    "Invalid RTP version" },
    { PJMEDIA_RTP_EINSSRC,	    "RTP packet SSRC id mismatch" },
    { PJMEDIA_RTP_EINPT,	    "RTP packet payload type mismatch" },
    { PJMEDIA_RTP_EINLEN,	    "Invalid RTP packet length" },
    { PJMEDIA_RTP_ESESSRESTART,	    "RTP session restarted" },
    { PJMEDIA_RTP_ESESSPROBATION,   "RTP session in probation" },
    { PJMEDIA_RTP_EBADSEQ,	    "Bad sequence number in RTP packet" },
    { PJMEDIA_RTP_EBADDEST,	    "RTP media port destination is not configured" },
    { PJMEDIA_RTP_ENOCONFIG,	    "RTP is not configured" },
    
    /* Media port errors: */
    { PJMEDIA_ENOTCOMPATIBLE,	    "Media ports are not compatible" },
    { PJMEDIA_ENCCLOCKRATE,	    "Media ports have incompatible clock rate" },
    { PJMEDIA_ENCSAMPLESPFRAME,	    "Media ports have incompatible samples per frame" },
    { PJMEDIA_ENCTYPE,		    "Media ports have incompatible media type" },
    { PJMEDIA_ENCBITS,		    "Media ports have incompatible bits per sample" },
    { PJMEDIA_ENCBYTES,		    "Media ports have incompatible bytes per frame" },
};



/*
 * pjmedia_strerror()
 */
PJ_DEF(pj_str_t) pjmedia_strerror( pj_status_t statcode, 
				   char *buf, pj_size_t bufsize )
{
    pj_str_t errstr;

    if (statcode >= PJMEDIA_ERRNO_START && 
	statcode < PJMEDIA_ERRNO_START + PJ_ERRNO_SPACE_SIZE)
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
    errstr.slen = pj_snprintf(buf, bufsize, 
			      "Unknown error %d",
			      statcode);

    return errstr;
}

