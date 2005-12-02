/* $Header: /pjproject/pjmedia/src/pjmedia/sdp.h 9     6/17/05 11:16p Bennylp $ */
/* 
 * PJMEDIA - Multimedia over IP Stack 
 * (C)2003-2005 Benny Prijono <bennylp@bulukucing.org>
 *
 * Author:
 *  Benny Prijono <bennylp@bulukucing.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __PJSDP_SDP_H__
#define __PJSDP_SDP_H__

/**
 * @defgroup PJSDP SDP Library
 */
/**
 * @file sdp.h
 * @brief SDP header file.
 */
/**
 * @defgroup PJ_SDP_H SDP stack.
 * @ingroup PJSDP
 * @{
 *
 * This SDP module consists of SDP parser, SDP structure, and function to
 * print back the structure as SDP message.
 */

#include <pj/types.h>

PJ_BEGIN_DECL

#define PJSDP_MAX_FMT	32
#define PJSDP_MAX_ATTR	32
#define PJSDP_MAX_MEDIA	16

/**
 * This enumeration describes the attribute type.
 */
typedef enum pjsdp_attr_type_e
{
    PJSDP_ATTR_RTPMAP,
    PJSDP_ATTR_CAT,
    PJSDP_ATTR_KEYWORDS,
    PJSDP_ATTR_TOOL,
    PJSDP_ATTR_PTIME,
    PJSDP_ATTR_RECV_ONLY,
    PJSDP_ATTR_SEND_ONLY,
    PJSDP_ATTR_SEND_RECV,
    PJSDP_ATTR_ORIENT,
    PJSDP_ATTR_TYPE,
    PJSDP_ATTR_CHARSET,
    PJSDP_ATTR_SDP_LANG,
    PJSDP_ATTR_LANG,
    PJSDP_ATTR_FRAME_RATE,
    PJSDP_ATTR_QUALITY,
    PJSDP_ATTR_FMTP,
    PJSDP_ATTR_INACTIVE,
    PJSDP_ATTR_GENERIC,
    PJSDP_END_OF_ATTR,
} pjsdp_attr_type_e;


/**
 * This structure keeps the common attributes that all 'descendants' 
 * will have.
 */
typedef struct pjsdp_attr
{
    pjsdp_attr_type_e	type;	/**< Attribute type. */
} pjsdp_attr;


/**
 * This is the structure to represent generic attribute which has a 
 * string value.
 */
typedef struct pjsdp_attr_string
{
    pjsdp_attr_type_e	type;
    pj_str_t		value;
} pjsdp_attr_string;


/**
 * This is the structure to represent generic SDP attribute which has
 * a numeric value.
 */
typedef struct pjsdp_attr_num
{
    pjsdp_attr_type_e	type;
    pj_uint32_t		value;
} pjsdp_attr_num;


/**
 * SDP \a rtpmap attribute.
 */
typedef struct pjsdp_rtpmap_attr
{
    pjsdp_attr_type_e	type;
    unsigned		payload_type;
    pj_str_t		encoding_name;
    unsigned		clock_rate;
    pj_str_t		parameter;
} pjsdp_rtpmap_attr;


/**
 * SDP \a fmtp attribute.
 */
typedef struct pjsdp_fmtp_attr
{
    pjsdp_attr_type_e	type;
    pj_str_t		format;
    pj_str_t		param;
} pjsdp_fmtp_attr;


/** 
 * SDP generic attribute.
 */
typedef struct pjsdp_generic_attr
{
    pjsdp_attr_type_e	type;
    pj_str_t		name;
    pj_str_t		value;
} pjsdp_generic_attr;


/** SDP \a cat attribute. */
typedef struct pjsdp_attr_string pjsdp_cat_attr;

/** SDP \a keywds attribute. */
typedef struct pjsdp_attr_string pjsdp_keywds_attr;

/** SDP \a tool attribute. */
typedef struct pjsdp_attr_string pjsdp_tool_attr;

/** SDP \a ptime attribute. */
typedef struct pjsdp_attr_num pjsdp_ptime_attr;

/** SDP \a recvonly attribute. */
typedef struct pjsdp_attr pjsdp_recv_only_attr;

/** SDP \a sendonly attribute. */
typedef struct pjsdp_attr pjsdp_send_only_attr;

/** SDP \a sendrecv attribute. */
typedef struct pjsdp_attr pjsdp_send_recv_attr;

/** SDP \a orient attribute. */
typedef struct pjsdp_attr_string pjsdp_orient_attr;

/** SDP \a type attribute. */
typedef struct pjsdp_attr_string pjsdp_type_attr;

/** SDP \a charset attribute. */
typedef struct pjsdp_attr_string pjsdp_charset_attr;

/** SDP \a sdplang attribute. */
typedef struct pjsdp_attr_string pjsdp_sdp_lang_attr;

/** SDP \a lang attribute. */
typedef struct pjsdp_attr_string pjsdp_lang_attr;

/** SDP \a framerate attribute. */
typedef struct pjsdp_attr_string pjsdp_frame_rate_attr;

/** SDP \a quality attribute. */
typedef struct pjsdp_attr_num pjsdp_quality_attr;

/** SDP \a inactive attribute. */
typedef struct pjsdp_attr pjsdp_inactive_attr;

/** Clone attribute */
PJ_DECL(pjsdp_attr*) pjsdp_attr_clone (pj_pool_t *pool, const pjsdp_attr *rhs);

/** Find attribute */
PJ_DECL(const pjsdp_attr*) pjsdp_attr_find (int count, const pjsdp_attr *attr_array[], int type);

/**
 * SDP connection info.
 */
typedef struct pjsdp_conn_info
{
    pj_str_t	net_type;
    pj_str_t	addr_type;
    pj_str_t	addr;
} pjsdp_conn_info;

/** 
 *Clone connection info. 
 * 
 * @param pool	    Pool to allocate memory for the new connection info.
 * @param rhs	    The connection into to clone.
 *
 * @return the new connection info.
 */
PJ_DECL(pjsdp_conn_info*) pjsdp_conn_info_clone (pj_pool_t *pool, 
						 const pjsdp_conn_info *rhs);

/**
 * SDP media description.
 */
typedef struct pjsdp_media_desc
{
    struct
    {
	pj_str_t    media;
	pj_uint16_t port;
	unsigned    port_count;
	pj_str_t    transport;
	unsigned    fmt_count;
	pj_str_t    fmt[PJSDP_MAX_FMT];
    } desc;

    pjsdp_conn_info *conn;
    unsigned	    attr_count;
    pjsdp_attr	   *attr[PJSDP_MAX_ATTR];

} pjsdp_media_desc;

/** 
 * Clone SDP media description. 
 *
 * @param pool	    Pool to allocate memory for the new media description.
 * @param rhs	    The media descriptin to clone.
 *
 * @return a new media description.
 */
PJ_DECL(pjsdp_media_desc*) pjsdp_media_desc_clone (pj_pool_t *pool, 
						   const pjsdp_media_desc *rhs);

/** 
 * Check if the media description has the specified attribute.
 *
 * @param m		The SDP media description.
 * @param attr_type	The attribute type.
 *
 * @return nonzero if true.
 */
PJ_DECL(pj_bool_t) pjsdp_media_desc_has_attr (const pjsdp_media_desc *m, 
					      pjsdp_attr_type_e attr_type);

/** 
 * Find rtpmap attribute for the specified payload type. 
 *
 * @param m	    The SDP media description.
 * @param pt	    RTP payload type.
 *
 * @return the SDP rtpmap attribute for the payload type, or NULL if not found.
 */
PJ_DECL(const pjsdp_rtpmap_attr*) 
pjsdp_media_desc_find_rtpmap (const pjsdp_media_desc *m, unsigned pt);


/**
 * This structure describes SDP session description.
 */
typedef struct pjsdp_session_desc
{
    struct
    {
	pj_str_t    user;
	pj_uint32_t id;
	pj_uint32_t version;
	pj_str_t    net_type;
	pj_str_t    addr_type;
	pj_str_t    addr;
    } origin;

    pj_str_t	     name;
    pjsdp_conn_info *conn;
    
    struct
    {
	pj_uint32_t start;
	pj_uint32_t stop;
    } time;

    unsigned	    attr_count;
    pjsdp_attr	   *attr[PJSDP_MAX_ATTR];

    unsigned	      media_count;
    pjsdp_media_desc *media[PJSDP_MAX_MEDIA];

} pjsdp_session_desc;


/**
 * Parse SDP message.
 *
 * @param buf	    The message buffer.
 * @param len	    The length of the message.
 * @param pool	    The pool to allocate SDP session description.
 *
 * @return SDP session description.
 */
PJ_DECL(pjsdp_session_desc*) pjsdp_parse( char *buf, pj_size_t len, 
					  pj_pool_t *pool);

/**
 * Print SDP description to a buffer.
 *
 * @param buf	    The buffer.
 * @param size	    The buffer length.
 * @param desc	    The SDP session description.
 *
 * @return the length printed, or -1.
 */
PJ_DECL(int) pjsdp_print( const pjsdp_session_desc *desc, 
			  char *buf, pj_size_t size);


/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJSDP_SDP_H__ */

