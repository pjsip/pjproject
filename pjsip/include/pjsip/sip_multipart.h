/* $Id$ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJSIP_SIP_MULTIPART_H__
#define __PJSIP_SIP_MULTIPART_H__

/**
 * @file pjsip/sip_multipart.h
 * @brief Multipart support.
 */

#include <pjsip/sip_msg.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_MULTIPART Multipart message bodies.
 * @ingroup PJSIP_MSG
 * @brief Support for multipart message bodies.
 * @{
 */

/**
 * This structure describes the individual body part inside a multipart
 * message body. It mainly contains the message body itself and optional
 * headers.
 */
typedef struct pjsip_multipart_part
{
    /**
     * Standard list element.
     */
    PJ_DECL_LIST_MEMBER(struct pjsip_multipart_part);

    /**
     * Optional message headers.
     */
    pjsip_hdr		    hdr;

    /**
     * Pointer to the message body.
     */
    pjsip_msg_body	   *body;

} pjsip_multipart_part;

/**
 * Create an empty multipart body.
 *
 * @param pool		Memory pool to allocate memory from.
 * @param ctype		Optional MIME media type of the multipart
 * 			bodies. If not specified, "multipart/mixed"
 * 			will be used.
 * @param boundary	Optional string to be set as part boundary.
 * 			The boundary string excludes the leading
 * 			hyphens. If this parameter is NULL or empty,
 * 			a random boundary will be generated.
 *
 * @return		Multipart body instance with no part.
 */
PJ_DECL(pjsip_msg_body*) pjsip_multipart_create(pj_pool_t *pool,
						const pjsip_media_type *ctype,
						const pj_str_t *boundary);

/**
 * Create an empty multipart part.
 *
 * @param pool		The memory pool.
 *
 * @return		The multipart part.
 */
PJ_DECL(pjsip_multipart_part*) pjsip_multipart_create_part(pj_pool_t *pool);


/**
 * Perform a deep clone to a multipart part.
 *
 * @param pool		The memory pool.
 * @param part		The part to be duplicated.
 *
 * @return		Copy of the multipart part.
 */
PJ_DECL(pjsip_multipart_part*)
pjsip_multipart_clone_part(pj_pool_t *pool,
			   const pjsip_multipart_part *part);

/**
 * Add a part into multipart bodies.
 *
 * @param pool		The memory pool.
 * @param mp		The multipart bodies.
 * @param part		The part to be added into the bodies.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_multipart_add_part(pj_pool_t *pool,
					      pjsip_msg_body *mp,
					      pjsip_multipart_part *part);

/**
 * Get the first part of multipart bodies.
 *
 * @param mp		The multipart bodies.
 *
 * @return		The first part, or NULL if the multipart
 * 			bodies currently doesn't hold any elements.
 */
PJ_DECL(pjsip_multipart_part*)
pjsip_multipart_get_first_part(const pjsip_msg_body *mp);

/**
 * Get the next part after the specified part.
 *
 * @param mp		The multipart bodies.
 * @param part		The part.
 *
 * @return		The next part, or NULL if there is no other part after
 * 			the part.
 */
PJ_DECL(pjsip_multipart_part*)
pjsip_multipart_get_next_part(const pjsip_msg_body *mp,
			      pjsip_multipart_part *part);

/**
 * Find a body inside multipart bodies which has the specified content type.
 *
 * @param mp		The multipart body.
 * @param content_type	Content type to find.
 * @param start		If specified, the search will begin at
 * 			start->next. Otherwise it will begin at
 * 			the first part in the multipart bodies.
 *
 * @return		The first part with the specified content type
 * 			if found, or NULL.
 */
PJ_DECL(pjsip_multipart_part*)
pjsip_multipart_find_part( const pjsip_msg_body *mp,
			   const pjsip_media_type *content_type,
			   const pjsip_multipart_part *start);

/**
 * Find a body inside multipart bodies which has a header matching the
 * supplied one. Most useful for finding a part with a specific Content-ID.
 *
 * @param pool		Memory pool to use for temp space.
 * @param mp		The multipart body.
 * @param search_hdr	Header to search for.
 * @param start		If specified, the search will begin at
 * 			start->next part. Otherwise it will begin at
 * 			the first part in the multipart bodies.
 *
 * @return		The first part which has a header matching the
 * 			specified one, or NULL if not found.
 */
PJ_DECL(pjsip_multipart_part*)
pjsip_multipart_find_part_by_header(pj_pool_t *pool,
				    const pjsip_msg_body *mp,
				    void *search_hdr,
				    const pjsip_multipart_part *start);

/**
 * Find a body inside multipart bodies which has a header matching the
 * supplied name and value. Most useful for finding a part with a specific
 * Content-ID.
 *
 * @param pool		Memory pool to use for temp space.
 * @param mp		The multipart body.
 * @param hdr_name	Header name to search for.
 * @param hdr_value	Header value search for.
 * @param start		If specified, the search will begin at
 * 			start->next part. Otherwise it will begin at
 * 			the first part in the multipart bodies.
 *
 * @return		The first part which has a header matching the
 * 			specified one, or NULL if not found.
 */
PJ_DECL(pjsip_multipart_part*)
pjsip_multipart_find_part_by_header_str(pj_pool_t *pool,
				    const pjsip_msg_body *mp,
				    const pj_str_t *hdr_name,
				    const pj_str_t *hdr_value,
				    const pjsip_multipart_part *start);



/**
 * Find a body inside multipart bodies which has a Content-ID value matching the
 * supplied "cid" URI in pj_str form.  The "cid:" scheme will be assumed if the
 * URL doesn't start with it.  Enclosing angle brackets will also be handled
 * correctly if they exist.
 *
 * @see RFC2392 Content-ID and Message-ID Uniform Resource Locators
 *
 * @param pool	Memory pool to use for temp space.
 * @param mp	The multipart body.
 * @param cid	The "cid" URI to search for in pj_str form.
 *
 * @return		The first part which has a Content-ID header matching the
 * 			specified "cid" URI. or NULL if not found.
 */
PJ_DECL(pjsip_multipart_part*)
pjsip_multipart_find_part_by_cid_str(pj_pool_t *pool,
				 const pjsip_msg_body *mp,
				 pj_str_t *cid);

/**
 * Find a body inside multipart bodies which has a Content-ID value matching the
 * supplied "cid" URI.
 *
 * @see RFC2392 Content-ID and Message-ID Uniform Resource Locators
 *
 * @param pool	Memory pool to use for temp space.
 * @param mp	The multipart body.
 * @param cid	The "cid" URI to search for.
 *
 * @return		The first part which had a Content-ID header matching the
 * 			specified "cid" URI. or NULL if not found.
 */
PJ_DECL(pjsip_multipart_part*)
pjsip_multipart_find_part_by_cid_uri(pj_pool_t *pool,
				 const pjsip_msg_body *mp,
				 pjsip_other_uri *cid_uri);

/**
 * Parse multipart message.
 *
 * @param pool		Memory pool.
 * @param buf		Input buffer.
 * @param len		The buffer length.
 * @param ctype		Content type of the multipart body.
 * @param options	Parsing options, must be zero for now.
 *
 * @return		Multipart message body.
 */
PJ_DECL(pjsip_msg_body*) pjsip_multipart_parse(pj_pool_t *pool,
					       char *buf, pj_size_t len,
					       const pjsip_media_type *ctype,
					       unsigned options);

/**
 * Get the boundary string and the raw message body of the specified
 * multipart message body. Note that raw message body will only be available
 * if the multipart message body is generated by pjsip_multipart_parse().
 *
 * @param mp		The multipart message body.
 * @param boundary	Optional parameter to receive the boundary string.
 * @param raw_data	Optional parameter to receive the raw message body.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_multipart_get_raw(pjsip_msg_body *mp,
					     pj_str_t *boundary,
					     pj_str_t *raw_data);

/**
 * @}  PJSIP_MULTIPART
 */


PJ_END_DECL

#endif	/* __PJSIP_SIP_MULTIPART_H__ */
