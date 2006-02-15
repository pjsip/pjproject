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
#ifndef __PJMEDIA_ERRNO_H__
#define __PJMEDIA_ERRNO_H__

#include <pj/errno.h>

PJ_BEGIN_DECL


/**
 * Start of error code relative to PJ_ERRNO_START_USER.
 */
#define PJMEDIA_ERRNO_START       (PJ_ERRNO_START_USER + PJ_ERRNO_SPACE_SIZE)


/************************************************************
 * GENERIC/GENERAL PJMEDIA ERRORS
 ***********************************************************/
/**
 * @hideinitializer
 * General/unknown PJMEDIA error.
 */
#define PJMEDIA_ERROR		    (PJMEDIA_ERRNO_START+1)	/* 220001 */


/************************************************************
 * SDP ERRORS
 ***********************************************************/
/**
 * @hideinitializer
 * Generic invalid SDP descriptor.
 */
#define PJMEDIA_SDP_EINSDP	    (PJMEDIA_ERRNO_START+20)    /* 220020 */
/**
 * @hideinitializer
 * Invalid SDP version.
 */
#define PJMEDIA_SDP_EINVER	    (PJMEDIA_ERRNO_START+21)    /* 220021 */
/**
 * @hideinitializer
 * Invalid SDP origin (o=) line.
 */
#define PJMEDIA_SDP_EINORIGIN	    (PJMEDIA_ERRNO_START+22)    /* 220022 */
/**
 * @hideinitializer
 * Invalid SDP time (t=) line.
 */
#define PJMEDIA_SDP_EINTIME	    (PJMEDIA_ERRNO_START+23)    /* 220023 */
/**
 * @hideinitializer
 * Empty SDP subject/name (s=) line.
 */
#define PJMEDIA_SDP_EINNAME	    (PJMEDIA_ERRNO_START+24)    /* 220024 */
/**
 * @hideinitializer
 * Invalid SDP connection info (c=) line.
 */
#define PJMEDIA_SDP_EINCONN	    (PJMEDIA_ERRNO_START+25)    /* 220025 */
/**
 * @hideinitializer
 * Missing SDP connection info line.
 */
#define PJMEDIA_SDP_EMISSINGCONN    (PJMEDIA_ERRNO_START+26)    /* 220026 */
/**
 * @hideinitializer
 * Invalid attribute (a=) line.
 */
#define PJMEDIA_SDP_EINATTR	    (PJMEDIA_ERRNO_START+27)    /* 220027 */
/**
 * @hideinitializer
 * Invalid rtpmap attribute.
 */
#define PJMEDIA_SDP_EINRTPMAP	    (PJMEDIA_ERRNO_START+28)    /* 220028 */
/**
 * @hideinitializer
 * rtpmap attribute is too long.
 */
#define PJMEDIA_SDP_ERTPMAPTOOLONG  (PJMEDIA_ERRNO_START+29)    /* 220029 */
/**
 * @hideinitializer
 * rtpmap is missing for dynamic payload type.
 */
#define PJMEDIA_SDP_EMISSINGRTPMAP  (PJMEDIA_ERRNO_START+30)    /* 220030 */
/**
 * @hideinitializer
 * Invalid SDP media (m=) line.
 */
#define PJMEDIA_SDP_EINMEDIA	    (PJMEDIA_ERRNO_START+31)    /* 220031 */
/**
 * @hideinitializer
 * No payload format in the media stream.
 */
#define PJMEDIA_SDP_ENOFMT	    (PJMEDIA_ERRNO_START+32)    /* 220032 */
/**
 * @hideinitializer
 * Invalid payload type in media.
 */
#define PJMEDIA_SDP_EINPT	    (PJMEDIA_ERRNO_START+33)    /* 220033 */
/**
 * @hideinitializer
 * Invalid fmtp attribute.
 */
#define PJMEDIA_SDP_EINFMTP	    (PJMEDIA_ERRNO_START+34)    /* 220034 */


/************************************************************
 * SDP NEGOTIATOR ERRORS
 ***********************************************************/
/**
 * @hideinitializer
 * Invalid state to perform the specified operation.
 */
#define PJMEDIA_SDPNEG_EINSTATE	    (PJMEDIA_ERRNO_START+40)    /* 220040 */
/**
 * @hideinitializer
 * No initial local SDP.
 */
#define PJMEDIA_SDPNEG_ENOINITIAL   (PJMEDIA_ERRNO_START+41)    /* 220041 */
/**
 * @hideinitializer
 * No currently active SDP.
 */
#define PJMEDIA_SDPNEG_ENOACTIVE    (PJMEDIA_ERRNO_START+42)    /* 220042 */
/**
 * @hideinitializer
 * No current offer or answer.
 */
#define PJMEDIA_SDPNEG_ENONEG	    (PJMEDIA_ERRNO_START+43)    /* 220043 */
/**
 * @hideinitializer
 * Media count mismatch in offer and answer.
 */
#define PJMEDIA_SDPNEG_EMISMEDIA    (PJMEDIA_ERRNO_START+44)    /* 220044 */
/**
 * @hideinitializer
 * Media type is different in the remote answer.
 */
#define PJMEDIA_SDPNEG_EINVANSMEDIA (PJMEDIA_ERRNO_START+45)    /* 220045 */
/**
 * @hideinitializer
 * Transport type is different in the remote answer.
 */
#define PJMEDIA_SDPNEG_EINVANSTP    (PJMEDIA_ERRNO_START+46)    /* 220046 */
/**
 * @hideinitializer
 * No common media payload is provided in the answer.
 */
#define PJMEDIA_SDPNEG_EANSNOMEDIA  (PJMEDIA_ERRNO_START+47)    /* 220047 */
/**
 * @hideinitializer
 * No media is active after negotiation.
 */
#define PJMEDIA_SDPNEG_ENOMEDIA	    (PJMEDIA_ERRNO_START+48)    /* 220048 */


/************************************************************
 * SDP COMPARISON STATUS
 ***********************************************************/
/**
 * @hideinitializer
 * SDP media stream not equal.
 */
#define PJMEDIA_SDP_EMEDIANOTEQUAL  (PJMEDIA_ERRNO_START+60)    /* 220060 */
/**
 * @hideinitializer
 * Port number in SDP media descriptor not equal.
 */
#define PJMEDIA_SDP_EPORTNOTEQUAL   (PJMEDIA_ERRNO_START+61)    /* 220061 */
/**
 * @hideinitializer
 * Transport in SDP media descriptor not equal.
 */
#define PJMEDIA_SDP_ETPORTNOTEQUAL  (PJMEDIA_ERRNO_START+62)    /* 220062 */
/**
 * @hideinitializer
 * Media format in SDP media descriptor not equal.
 */
#define PJMEDIA_SDP_EFORMATNOTEQUAL (PJMEDIA_ERRNO_START+63)    /* 220063 */
/**
 * @hideinitializer
 * SDP connection description not equal.
 */
#define PJMEDIA_SDP_ECONNNOTEQUAL   (PJMEDIA_ERRNO_START+64)    /* 220064 */
/**
 * @hideinitializer
 * SDP attributes not equal.
 */
#define PJMEDIA_SDP_EATTRNOTEQUAL   (PJMEDIA_ERRNO_START+65)    /* 220065 */
/**
 * @hideinitializer
 * SDP media direction not equal.
 */
#define PJMEDIA_SDP_EDIRNOTEQUAL    (PJMEDIA_ERRNO_START+66)    /* 220066 */
/**
 * @hideinitializer
 * SDP fmtp attribute not equal.
 */
#define PJMEDIA_SDP_EFMTPNOTEQUAL   (PJMEDIA_ERRNO_START+67)    /* 220067 */
/**
 * @hideinitializer
 * SDP ftpmap attribute not equal.
 */
#define PJMEDIA_SDP_ERTPMAPNOTEQUAL (PJMEDIA_ERRNO_START+68)    /* 220068 */
/**
 * @hideinitializer
 * SDP session descriptor not equal.
 */
#define PJMEDIA_SDP_ESESSNOTEQUAL   (PJMEDIA_ERRNO_START+69)    /* 220069 */
/**
 * @hideinitializer
 * SDP origin not equal.
 */
#define PJMEDIA_SDP_EORIGINNOTEQUAL (PJMEDIA_ERRNO_START+70)    /* 220070 */
/**
 * @hideinitializer
 * SDP name/subject not equal.
 */
#define PJMEDIA_SDP_ENAMENOTEQUAL   (PJMEDIA_ERRNO_START+71)    /* 220071 */
/**
 * @hideinitializer
 * SDP time not equal.
 */
#define PJMEDIA_SDP_ETIMENOTEQUAL   (PJMEDIA_ERRNO_START+72)    /* 220072 */


/************************************************************
 * CODEC
 ***********************************************************/
/**
 * @hideinitializer
 * Unsupported codec.
 */
#define PJMEDIA_CODEC_EUNSUP	    (PJMEDIA_ERRNO_START+80)    /* 220080 */
/**
 * @hideinitializer
 * Codec internal creation error.
 */
#define PJMEDIA_CODEC_EFAILED	    (PJMEDIA_ERRNO_START+81)    /* 220081 */
/**
 * @hideinitializer
 * Codec frame is too short.
 */
#define PJMEDIA_CODEC_EFRMTOOSHORT  (PJMEDIA_ERRNO_START+82)    /* 220082 */
/**
 * @hideinitializer
 * PCM buffer is too short.
 */
#define PJMEDIA_CODEC_EPCMTOOSHORT  (PJMEDIA_ERRNO_START+83)    /* 220083 */


/************************************************************
 * MEDIA
 ***********************************************************/
/**
 * @hideinitializer
 * Invalid remote IP address (in SDP).
 */
#define PJMEDIA_EINVALIDIP	    (PJMEDIA_ERRNO_START+100)    /* 220100 */
/**
 * @hideinitializer
 * Asymetric codec is not supported.
 */
#define PJMEDIA_EASYMCODEC	    (PJMEDIA_ERRNO_START+101)    /* 220101 */
/**
 * @hideinitializer
 * Invalid payload type.
 */
#define PJMEDIA_EINVALIDPT	    (PJMEDIA_ERRNO_START+102)    /* 220102 */
/**
 * @hideinitializer
 * Missing rtpmap.
 */
#define PJMEDIA_EMISSINGRTPMAP	    (PJMEDIA_ERRNO_START+103)    /* 220103 */
/**
 * @hideinitializer
 * Invalid media type.
 */
#define PJMEDIA_EINVALIMEDIATYPE    (PJMEDIA_ERRNO_START+104)    /* 220104 */



/************************************************************
 * RTP SESSION ERRORS
 ***********************************************************/
/**
 * @hideinitializer
 * General invalid RTP packet error.
 */
#define PJMEDIA_RTP_EINPKT	    (PJMEDIA_ERRNO_START+120)    /* 220120 */
/**
 * @hideinitializer
 * Invalid RTP packet packing.
 */
#define PJMEDIA_RTP_EINPACK	    (PJMEDIA_ERRNO_START+121)    /* 220121 */
/**
 * @hideinitializer
 * Invalid RTP packet version.
 */
#define PJMEDIA_RTP_EINVER	    (PJMEDIA_ERRNO_START+122)    /* 220122 */
/**
 * @hideinitializer
 * RTP SSRC id mismatch.
 */
#define PJMEDIA_RTP_EINSSRC	    (PJMEDIA_ERRNO_START+123)    /* 220123 */
/**
 * @hideinitializer
 * RTP payload type mismatch.
 */
#define PJMEDIA_RTP_EINPT	    (PJMEDIA_ERRNO_START+124)    /* 220124 */
/**
 * @hideinitializer
 * Invalid RTP packet length.
 */
#define PJMEDIA_RTP_EINLEN	    (PJMEDIA_ERRNO_START+125)    /* 220125 */
/**
 * @hideinitializer
 * RTP session restarted.
 */
#define PJMEDIA_RTP_ESESSRESTART    (PJMEDIA_ERRNO_START+130)    /* 220130 */
/**
 * @hideinitializer
 * RTP session in probation
 */
#define PJMEDIA_RTP_ESESSPROBATION  (PJMEDIA_ERRNO_START+131)    /* 220131 */
/**
 * @hideinitializer
 * Bad RTP sequence number
 */
#define PJMEDIA_RTP_EBADSEQ	    (PJMEDIA_ERRNO_START+132)    /* 220132 */


/************************************************************
 * JITTER BUFFER ERRORS
 ***********************************************************/


PJ_END_DECL

#endif	/* __PJMEDIA_ERRNO_H__ */

