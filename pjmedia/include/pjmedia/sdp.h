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
#ifndef __PJMEDIA_SDP_H__
#define __PJMEDIA_SDP_H__

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

#include <pjmedia/types.h>

PJ_BEGIN_DECL

#define PJSDP_MAX_FMT	32
#define PJSDP_MAX_ATTR	32
#define PJSDP_MAX_MEDIA	16


/****************************************************************************
 * SDP ATTRIBUTES
 ****************************************************************************
 */

/** 
 * SDP generic attribute.
 */
struct pjmedia_sdp_attr
{
    pj_str_t		name;	    /**< Attribute name.    */
    pj_str_t		value;	    /**< Attribute value.   */
};


/**
 * Create SDP attribute.
 *
 * @param pool		Pool to create the attribute.
 * @param name		Attribute name.
 * @param value		Optional attribute value.
 *
 * @return		The new SDP attribute.
 */
PJ_DECL(pjmedia_sdp_attr*) pjmedia_sdp_attr_create(pj_pool_t *pool,
						   const char *name,
						   const pj_str_t *value);

/** 
 * Clone attribute 
 *
 * @param pool		Pool to be used.
 * @param attr		The attribute to clone.
 *
 * @return		New attribute as cloned from the attribute.
 */
PJ_DECL(pjmedia_sdp_attr*) pjmedia_sdp_attr_clone(pj_pool_t *pool, 
						  const pjmedia_sdp_attr*attr);

/** 
 * Find the first attribute with the specified type.
 *
 * @param count		Number of attributes in the array.
 * @param attr_array	Array of attributes.
 * @param name		Attribute name to find.
 * @param fmt		Optional string to indicate which payload format
 *			to find for rtpmap and fmt attributes.
 *
 * @return		The specified attribute, or NULL if it can't be found.
 *
 * @see pjmedia_sdp_attr_find2, pjmedia_sdp_media_find_attr
 */
PJ_DECL(pjmedia_sdp_attr*) 
pjmedia_sdp_attr_find(unsigned count, 
		      const pjmedia_sdp_attr *const attr_array[],
		      const pj_str_t *name, const pj_str_t *fmt);

/** 
 * Find the first attribute with the specified type.
 *
 * @param count		Number of attributes in the array.
 * @param attr_array	Array of attributes.
 * @param name		Attribute name to find.
 * @param fmt		Optional string to indicate which payload format
 *			to find for rtpmap and fmt attributes.
 *
 * @return		The specified attribute, or NULL if it can't be found.
 *
 * @see pjmedia_sdp_attr_find, pjmedia_sdp_media_find_attr2
 */
PJ_DECL(pjmedia_sdp_attr*) 
pjmedia_sdp_attr_find2(unsigned count, 
		       const pjmedia_sdp_attr *const attr_array[],
		       const char *name, const pj_str_t *fmt);

/**
 * Add a new attribute to array of attributes.
 *
 * @param count		Number of attributes in the array.
 * @param attr_array	Array of attributes.
 * @param attr		The attribute to add.
 *
 * @return		PJ_SUCCESS or the error code.
 *
 * @see pjmedia_sdp_media_add_attr
 */
PJ_DECL(pj_status_t) pjmedia_sdp_attr_add(unsigned *count,
					  pjmedia_sdp_attr *attr_array[],
					  pjmedia_sdp_attr *attr);

/**
 * Remove all attributes with the specified name when they present.
 *
 * @param count		Number of attributes in the array.
 * @param attr_array	Array of attributes.
 * @param name		Attribute name to find.
 *
 * @return		Number of attributes removed.
 *
 * @see pjmedia_sdp_media_remove_all_attr
 */
PJ_DECL(unsigned) pjmedia_sdp_attr_remove_all(unsigned *count,
					      pjmedia_sdp_attr *attr_array[],
					      const char *name);


/**
 * Remove the specified attribute from the attribute array.
 *
 * @param count		Number of attributes in the array.
 * @param attr_array	Array of attributes.
 * @param name		Attribute name to find.
 *
 * @return		PJ_SUCCESS when attribute has been removed, or 
 *			PJ_ENOTFOUND when the attribute can not be found.
 *
 * @see pjmedia_sdp_media_remove_attr
 */
PJ_DECL(pj_status_t) pjmedia_sdp_attr_remove(unsigned *count,
					     pjmedia_sdp_attr *attr_array[],
					     pjmedia_sdp_attr *attr);


/**
 * SDP \a rtpmap attribute.
 */
struct pjmedia_sdp_rtpmap
{
    pj_str_t		pt;	    /**< Payload type.	    */
    pj_str_t		enc_name;   /**< Encoding name.	    */
    unsigned		clock_rate; /**< Clock rate.	    */
    pj_str_t		param;	    /**< Parameter.	    */
};


/**
 * Convert generic attribute to SDP rtpmap.
 *
 * @param pool		Pool used to create the rtpmap attribute.
 * @param attr		Generic attribute to be converted to rtpmap, which
 *			name must be "rtpmap".
 * @param p_rtpmap	Pointer to receive SDP rtpmap attribute.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_sdp_attr_to_rtpmap(pj_pool_t *pool,
						const pjmedia_sdp_attr *attr,
						pjmedia_sdp_rtpmap **p_rtpmap);


/**
 * Get the rtpmap representation of the same SDP attribute.
 *
 * @param attr		Generic attribute to be converted to rtpmap, which
 *			name must be "rtpmap".
 * @param rtpmap	SDP rtpmap attribute to be initialized.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_sdp_attr_get_rtpmap(const pjmedia_sdp_attr *attr,
						 pjmedia_sdp_rtpmap *rtpmap);


/**
 * Convert rtpmap attribute to generic attribute.
 *
 * @param pool		Pool to be used.
 * @param rtpmap	The rtpmap attribute.
 * @param p_attr	Pointer to receive the generic SDP attribute.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_sdp_rtpmap_to_attr(pj_pool_t *pool,
						const pjmedia_sdp_rtpmap *rtpmap,
						pjmedia_sdp_attr **p_attr);


/**
 * SDP \a fmtp attribute.
 */
struct pjmedia_sdp_fmtp
{
    pj_str_t		fmt;	    /**< Format type.		    */
    pj_str_t		fmt_param;  /**< Format specific parameter. */
};


/**
 * Get the fmtp representation of the same SDP attribute.
 *
 * @param attr		Generic attribute to be converted to fmtp, which
 *			name must be "fmtp".
 * @param fmtp		SDP fmtp attribute to be initialized.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_sdp_attr_get_fmtp(const pjmedia_sdp_attr *attr,
					       pjmedia_sdp_fmtp *fmtp);


/****************************************************************************
 * SDP CONNECTION INFO
 ****************************************************************************
 */

/**
 * SDP connection info.
 */
struct pjmedia_sdp_conn
{
    pj_str_t	net_type;	/**< Network type ("IN").		*/
    pj_str_t	addr_type;	/**< Address type ("IP4", "IP6").	*/
    pj_str_t	addr;		/**< The address.			*/
};


/** 
 *Clone connection info. 
 * 
 * @param pool	    Pool to allocate memory for the new connection info.
 * @param rhs	    The connection into to clone.
 *
 * @return the new connection info.
 */
PJ_DECL(pjmedia_sdp_conn*) pjmedia_sdp_conn_clone(pj_pool_t *pool, 
						  const pjmedia_sdp_conn *rhs);



/****************************************************************************
 * SDP MEDIA INFO/LINE
 ****************************************************************************
 */

/**
 * SDP media description.
 */
struct pjmedia_sdp_media
{
    struct
    {
	pj_str_t    media;		/**< Media type ("audio", "video")  */
	pj_uint16_t port;		/**< Port number.		    */
	unsigned    port_count;		/**< Port count, used only when >2  */
	pj_str_t    transport;		/**< Transport ("RTP/AVP")	    */
	unsigned    fmt_count;		/**< Number of formats.		    */
	pj_str_t    fmt[PJSDP_MAX_FMT];	/**< Media formats.		    */
    } desc;

    pjmedia_sdp_conn *conn;		/**< Optional connection info.	    */
    unsigned	     attr_count;	/**< Number of attributes.	    */
    pjmedia_sdp_attr*attr[PJSDP_MAX_ATTR];  /**< Attributes.		    */

};


/** 
 * Clone SDP media description. 
 *
 * @param pool	    Pool to allocate memory for the new media description.
 * @param rhs	    The media descriptin to clone.
 *
 * @return a new media description.
 */
PJ_DECL(pjmedia_sdp_media*) 
pjmedia_sdp_media_clone( pj_pool_t *pool, 
			 const pjmedia_sdp_media *rhs);

/**
 * Find the first occurence of the specified attribute name.
 *
 * @param m		The SDP media description.
 * @param name		Attribute name to find.
 * @param fmt		Optional payload format type to find in the
 *			attribute list. The payload format type will be
 *			compared for attributes such as rtpmap and fmtp.
 *
 * @return		The first instance of the specified attribute or NULL.
 */
PJ_DECL(pjmedia_sdp_attr*) 
pjmedia_sdp_media_find_attr(const pjmedia_sdp_media *m,
			    const pj_str_t *name, const pj_str_t *fmt);


/**
 * Find the first occurence of the specified attribute name.
 *
 * @param m		The SDP media description.
 * @param name		Attribute name to find.
 * @param fmt		Optional payload format type to find in the
 *			attribute list. The payload format type will be
 *			compared for attributes such as rtpmap and fmtp.
 *
 * @return		The first instance of the specified attribute or NULL.
 */
PJ_DECL(pjmedia_sdp_attr*) 
pjmedia_sdp_media_find_attr2(const pjmedia_sdp_media *m,
			     const char *name, const pj_str_t *fmt);

/**
 * Add new attribute to the media descriptor.
 *
 * @param m		The SDP media description.
 * @param name		Attribute to add.
 *
 * @return		PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_sdp_media_add_attr(pjmedia_sdp_media *m,
						pjmedia_sdp_attr *attr);

/**
 * Remove all attributes with the specified name.
 *
 * @param m		The SDP media description.
 * @param name		Attribute name to remove.
 *
 * @return		The number of attributes removed.
 */
PJ_DECL(unsigned) 
pjmedia_sdp_media_remove_all_attr(pjmedia_sdp_media *m,
				  const char *name);


/**
 * Remove the occurence of the specified attribute.
 */
PJ_DECL(pj_status_t)
pjmedia_sdp_media_remove_attr(pjmedia_sdp_media *m,
			      pjmedia_sdp_attr *attr);


/**
 * Compare two SDP media for equality.
 *
 * @param sd1	    The first SDP media to compare.
 * @param sd2	    The second SDP media to compare.
 * @param option    Comparison option.
 *
 * @return	    PJ_SUCCESS when both SDP medias are equal.
 */
PJ_DECL(pj_status_t) pjmedia_sdp_media_cmp(const pjmedia_sdp_media *sd1,
					   const pjmedia_sdp_media *sd2,
					   unsigned option);



/****************************************************************************
 * SDP SESSION DESCRIPTION
 ****************************************************************************
 */


/**
 * This structure describes SDP session description.
 */
struct pjmedia_sdp_session
{
    /** Origin (o= line) */
    struct
    {
	pj_str_t    user;	    /**< User 				*/
	pj_uint32_t id;		    /**< Session ID			*/
	pj_uint32_t version;	    /**< Session version		*/
	pj_str_t    net_type;	    /**< Network type ("IN")		*/
	pj_str_t    addr_type;	    /**< Address type ("IP4", "IP6")	*/
	pj_str_t    addr;	    /**< The address.			*/
    } origin;

    pj_str_t	     name;	    /**< Subject line (s=)		*/
    pjmedia_sdp_conn *conn;	    /**< Connection line (c=)		*/
    
    /** Session time (t= line)	*/
    struct
    {
	pj_uint32_t start;	    /**< Start time.			*/
	pj_uint32_t stop;	    /**< Stop time.			*/
    } time;

    unsigned	       attr_count;		/**< Number of attributes.  */
    pjmedia_sdp_attr  *attr[PJSDP_MAX_ATTR];	/**< Attributes array.	    */

    unsigned	       media_count;		/**< Number of media.	    */
    pjmedia_sdp_media *media[PJSDP_MAX_MEDIA];	/**< Media array.	    */

};


/**
 * Parse SDP message.
 *
 * @param buf	    The message buffer.
 * @param len	    The length of the message.
 * @param pool	    The pool to allocate SDP session description.
 *
 * @return SDP session description.
 */
PJ_DECL(pj_status_t) pjmedia_sdp_parse( pj_pool_t *pool,
				        char *buf, pj_size_t len, 
					pjmedia_sdp_session **p_sdp );

/**
 * Print SDP description to a buffer.
 *
 * @param buf	    The buffer.
 * @param size	    The buffer length.
 * @param desc	    The SDP session description.
 *
 * @return the length printed, or -1.
 */
PJ_DECL(int) pjmedia_sdp_print( const pjmedia_sdp_session *desc, 
				char *buf, pj_size_t size);


/**
 * Validate SDP descriptor.
 */
PJ_DECL(pj_status_t) pjmedia_sdp_validate(const pjmedia_sdp_session *sdp);


/**
 * Clone SDP session.
 *
 * @param pool	    The pool used to clone the session.
 * @param sess	    The SDP session to clone.
 *
 * @return	    New SDP session.
 */
PJ_DECL(pjmedia_sdp_session*) 
pjmedia_sdp_session_clone( pj_pool_t *pool,
			   const pjmedia_sdp_session *sess);


/**
 * Compare two SDP session for equality.
 *
 * @param sd1	    The first SDP session to compare.
 * @param sd2	    The second SDP session to compare.
 *
 * @return	    PJ_SUCCESS when both SDPs are equal.
 */
PJ_DECL(pj_status_t) pjmedia_sdp_session_cmp(const pjmedia_sdp_session *sd1,
					     const pjmedia_sdp_session *sd2,
					     unsigned option);


PJ_END_DECL

/**
 * @}
 */

#endif	/* __PJMEDIA_SDP_H__ */

