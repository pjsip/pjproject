/* $Header: /pjproject-0.3/pjlib/src/pj/addr_resolv_sock.c 2     10/14/05 12:26a Bennylp $ */
/* $Log: /pjproject-0.3/pjlib/src/pj/addr_resolv_sock.c $
 * 
 * 2     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 1     9/22/05 10:38a Bennylp
 * Created.
 *
 */
#include <pj/addr_resolv.h>
#include <pj/assert.h>
#include <pj/string.h>
#include <pj/compat/socket.h>
#include <pj/errno.h>


PJ_DEF(pj_status_t) pj_gethostbyname(const pj_str_t *hostname, pj_hostent *phe)
{
    struct hostent *he;
    char copy[PJ_MAX_HOSTNAME];

    pj_assert(hostname && hostname ->slen < PJ_MAX_HOSTNAME);
    
    if (hostname->slen >= PJ_MAX_HOSTNAME)
	return PJ_ENAMETOOLONG;

    pj_memcpy(copy, hostname->ptr, hostname->slen);
    copy[ hostname->slen ] = '\0';

    he = gethostbyname(copy);
    if (!he)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());

    phe->h_name = he->h_name;
    phe->h_aliases = he->h_aliases;
    phe->h_addrtype = he->h_addrtype;
    phe->h_length = he->h_length;
    phe->h_addr_list = he->h_addr_list;

    return PJ_SUCCESS;
}

