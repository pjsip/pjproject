/* $Id$
 */
/* 
 * PJSIP - SIP Stack
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
 *
 */
#ifndef __PJSIP_SIP_URI_H__
#define __PJSIP_SIP_URI_H__

/**
 * @file sip_uri.h
 * @brief SIP URL Structures and Manipulations
 */

#include <pjsip/sip_types.h>
#include <pjsip/sip_config.h>

PJ_BEGIN_DECL


/**
 * @defgroup PJSIP_URL URL Structures
 * @brief SIP Url, tel: Url, and generic URI.
 * @ingroup PJSIP_MSG
 * @{
 */

/**
 * URI context.
 */
typedef enum pjsip_uri_context_e
{
    PJSIP_URI_IN_REQ_URI,	/**< The URI is in Request URI. */
    PJSIP_URI_IN_FROMTO_HDR,	/**< The URI is in From/To header. */
    PJSIP_URI_IN_CONTACT_HDR,	/**< The URI is in Contact header. */
    PJSIP_URI_IN_ROUTING_HDR,	/**< The URI is in Route/Record-Route header. */
    PJSIP_URI_IN_OTHER,		/**< Other context (web page, business card, etc.) */
} pjsip_uri_context_e;

/**
 * URI 'virtual' function table.
 * All types of URI in this library (such as sip:, sips:, tel:, and name-addr) 
 * will have pointer to this table as their first struct member. This table
 * provides polimorphic behaviour to the URI.
 */
typedef struct pjsip_uri_vptr
{
    /** 
     * Get URI scheme. 
     * @param uri the URI (self).
     * @return the URI scheme.
     */
    const pj_str_t* (*p_get_scheme)(const void *uri);

    /**
     * Get the URI object contained by this URI, or the URI itself if
     * it doesn't contain another URI.
     * @param uri the URI (self).
     */
    void* (*p_get_uri)(void *uri);

    /**
     * Print URI components to the buffer, following the rule of which 
     * components are allowed for the context.
     * @param context the context where the URI will be placed.
     * @param uri the URI (self).
     * @param buf the buffer.
     * @param size the size of the buffer.
     * @return the length printed.
     */
    int	(*p_print)(pjsip_uri_context_e context,
		   const void *uri, 
		   char *buf, pj_size_t size);

    /** 
     * Compare two URIs according to the context.
     * @param context the context.
     * @param uri1 the first URI (self).
     * @param uri2 the second URI.
     * @return zero if equal.
     */
    int	(*p_compare)(pjsip_uri_context_e context, 
		     const void *uri1, const void *uri2);

    /** 
     * Clone URI. 
     * @param pool the pool.
     * @param the URI to clone (self).
     * @return new URI.
     */
    void *(*p_clone)(pj_pool_t *pool, const void *uri);

} pjsip_uri_vptr;


/**
 * The declaration of 'base class' for all URI scheme.
 */
struct pjsip_uri
{
    /** All URIs must have URI virtual function table as their first member. */
    pjsip_uri_vptr *vptr;
};

/**
 * This macro checks that the URL is a "sip:" or "sips:" URL.
 * @param url The URL (pointer to)
 * @return non-zero if TRUE.
 */
#define PJSIP_URI_SCHEME_IS_SIP(url)	\
    (pj_strnicmp2(pjsip_uri_get_scheme(url), "sip", 3)==0)

/**
 * This macro checks that the URL is a "sips:" URL (not SIP).
 * @param url The URL (pointer to)
 * @return non-zero if TRUE.
 */
#define PJSIP_URI_SCHEME_IS_SIPS(url)	\
    (pj_strnicmp2(pjsip_uri_get_scheme(url), "sips", 4)==0)

/**
 * This macro checks that the URL is a "tel:" URL.
 * @param url The URL (pointer to)
 * @return non-zero if TRUE.
 */
#define PJSIP_URI_SCHEME_IS_TEL(url)	\
    (pj_strnicmp2(pjsip_uri_get_scheme(url), "tel", 3)==0)



/**
 * SIP and SIPS URL scheme.
 */
typedef struct pjsip_url
{
    pjsip_uri_vptr *vptr;		/**< Pointer to virtual function table.*/
    pj_str_t	    user;		/**< Optional user part. */
    pj_str_t	    passwd;		/**< Optional password part. */
    pj_str_t	    host;		/**< Host part, always exists. */
    int		    port;		/**< Optional port number, or zero. */
    pj_str_t	    user_param;		/**< Optional user parameter */
    pj_str_t	    method_param;	/**< Optional method parameter. */
    pj_str_t	    transport_param;	/**< Optional transport parameter. */
    int		    ttl_param;		/**< Optional TTL param, or -1. */
    int		    lr_param;		/**< Optional loose routing param, or zero */
    pj_str_t	    maddr_param;	/**< Optional maddr param */
    pj_str_t	    other_param;	/**< Other parameters grouped together. */
    pj_str_t	    header_param;	/**< Optional header parameter. */
} pjsip_url;


/**
 * SIP name-addr, which typically appear in From, To, and Contact header.
 * The SIP name-addr contains a generic URI and a display name.
 */
typedef struct pjsip_name_addr
{
    /** Pointer to virtual function table. */
    pjsip_uri_vptr  *vptr;

    /** Optional display name. */
    pj_str_t	     display;

    /** URI part. */
    pjsip_uri	    *uri;

} pjsip_name_addr;


/**
 * Generic function to get the URI scheme.
 * @param uri	    the URI object.
 * @return	    the URI scheme.
 */
PJ_INLINE(const pj_str_t*) pjsip_uri_get_scheme(const void *uri)
{
    return (*((pjsip_uri*)uri)->vptr->p_get_scheme)(uri);
}

/**
 * Generic function to get the URI object contained by this URI, or the URI 
 * itself if it doesn't contain another URI.
 *
 * @param uri	    the URI.
 * @return	    the URI.
 */
PJ_INLINE(void*) pjsip_uri_get_uri(void *uri)
{
    return (*((pjsip_uri*)uri)->vptr->p_get_uri)(uri);
}

/**
 * Generic function to compare two URIs.
 *
 * @param context   Comparison context.
 * @param uri1	    The first URI.
 * @param uri2	    The second URI.
 * @return	    Zero if equal.
 */
PJ_INLINE(int) pjsip_uri_cmp(pjsip_uri_context_e context, 
			     const void *uri1, const void *uri2)
{
    return (*((const pjsip_uri*)uri1)->vptr->p_compare)(context, uri1, uri2);
}

/**
 * Generic function to print an URI object.
 *
 * @param context   Print context.
 * @param uri	    The URI to print.
 * @param buf	    The buffer.
 * @param size	    Size of the buffer.
 * @return	    Length printed.
 */
PJ_INLINE(int) pjsip_uri_print(pjsip_uri_context_e context,
			       const void *uri,
			       char *buf, pj_size_t size)
{
    return (*((const pjsip_uri*)uri)->vptr->p_print)(context, uri, buf, size);
}

/**
 * Generic function to clone an URI object.
 *
 * @param pool	    Pool.
 * @param uri	    URI to clone.
 * @return	    New URI.
 */
PJ_INLINE(void*) pjsip_uri_clone( pj_pool_t *pool, const void *uri )
{
    return (*((const pjsip_uri*)uri)->vptr->p_clone)(pool, uri);
}


/**
 * Create new SIP URL and initialize all fields with zero or NULL.
 * @param pool	    The pool.
 * @param secure    Tlag to indicate whether secure transport should be used.
 * @return SIP URL.
 */
PJ_DECL(pjsip_url*) pjsip_url_create( pj_pool_t *pool, int secure );

/**
 * Create new SIPS URL and initialize all fields with zero or NULL.
 * @param pool	    The pool.
 * @return	    SIPS URL.
 */
PJ_DECL(pjsip_url*) pjsips_url_create( pj_pool_t *pool );

/**
 * Initialize SIP URL (all fields are set to NULL or zero).
 * @param url	    The URL.
 */
PJ_DECL(void)  pjsip_url_init(pjsip_url *url, int secure);

/**
 * Perform full assignment to the SIP URL.
 * @param pool	    The pool.
 * @param url	    Destination URL.
 * @param rhs	    The source URL.
 */
PJ_DECL(void)  pjsip_url_assign(pj_pool_t *pool, pjsip_url *url, const pjsip_url *rhs);

/**
 * Create new instance of name address and initialize all fields with zero or
 * NULL.
 * @param pool	    The pool.
 * @return	    New SIP name address.
 */
PJ_DECL(pjsip_name_addr*) pjsip_name_addr_create(pj_pool_t *pool);

/**
 * Initialize with default value.
 * @param name_addr The name address.
 */
PJ_DECL(void) pjsip_name_addr_init(pjsip_name_addr *name_addr);

/**
 * Perform full assignment to the name address.
 * @param pool	    The pool.
 * @param addr	    The destination name address.
 * @param rhs	    The source name address.
 */
PJ_DECL(void)  pjsip_name_addr_assign(pj_pool_t *pool, 
				      pjsip_name_addr *addr, 
				      const pjsip_name_addr *rhs);




/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJSIP_URL_H__ */

