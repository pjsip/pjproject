/* $Id$
 *
 */

#ifndef __PJ_ADDR_RESOLV_H__
#define __PJ_ADDR_RESOLV_H__

/**
 * @file addr_resolv.h
 * @brief Address resolve (pj_gethostbyname()).
 */

#include <pj/types.h>

PJ_BEGIN_DECL

/**
 * @defgroup pj_addr_resolve Address Resolution
 * @ingroup PJ_IO
 * @{
 *
 * This module provides function to resolve Internet address of the
 * specified host name. To resolve a particular host name, application
 * can just call #pj_gethostbyname().
 *
 * Example:
 * <pre>
 *   ...
 *   pj_hostent he;
 *   pj_status_t rc;
 *   pj_str_t host = pj_str("host.example.com");
 *   
 *   rc = pj_gethostbyname( &host, &he);
 *   if (rc != PJ_SUCCESS) {
 *      char errbuf[80];
 *      pj_strerror( rc, errbuf, sizeof(errbuf));
 *      PJ_LOG(2,("sample", "Unable to resolve host, error=%s", errbuf));
 *      return rc;
 *   }
 *
 *   // process address...
 *   addr.sin_addr.s_addr = *(pj_uint32_t*)he.h_addr;
 *   ...
 * </pre>
 *
 * It's pretty simple really...
 */

/** This structure describes an Internet host address. */
typedef struct pj_hostent
{
    char    *h_name;		/**< The official name of the host. */
    char   **h_aliases;		/**< Aliases list. */
    int	     h_addrtype;	/**< Host address type. */
    int	     h_length;		/**< Length of address. */
    char   **h_addr_list;	/**< List of addresses. */
} pj_hostent;

/** Shortcut to h_addr_list[0] */
#define h_addr h_addr_list[0]

/**
 * This function fills the structure of type pj_hostent for a given host name.
 *
 * @param name	    Host name, or IPv4 or IPv6 address in standard dot notation.
 * @param he	    The pj_hostent structure to be filled.
 *
 * @return	    PJ_SUCCESS, or the appropriate error codes.
 */ 
PJ_DECL(pj_status_t) pj_gethostbyname(const pj_str_t *name, pj_hostent *he);


/** @} */

PJ_END_DECL

#endif	/* __PJ_ADDR_RESOLV_H__ */

