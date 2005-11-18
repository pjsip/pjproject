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
#include <pjsip/sip_uri.h>
#include <pjsip/sip_msg.h>
#include <pjsip/print_util.h>
#include <pj/string.h>
#include <pj/pool.h>
#include <pj/assert.h>

#define IS_SIPS(url)	((url)->vptr==&sips_url_vptr)

static const pj_str_t *pjsip_url_get_scheme( const pjsip_url* );
static const pj_str_t *pjsips_url_get_scheme( const pjsip_url* );
static const pj_str_t *pjsip_name_addr_get_scheme( const pjsip_name_addr * );
static void *pjsip_get_uri( pjsip_uri *uri );
static void *pjsip_name_addr_get_uri( pjsip_name_addr *name );

static pj_str_t sip_str = { "sip", 3 };
static pj_str_t sips_str = { "sips", 4 };

#ifdef __GNUC__
#  define HAPPY_FLAG	(void*)
#else
#  define HAPPY_FLAG
#endif

static pjsip_name_addr* pjsip_name_addr_clone( pj_pool_t *pool, 
					       const pjsip_name_addr *rhs);
static int pjsip_name_addr_print( pjsip_uri_context_e context,
				  const pjsip_name_addr *name, 
				  char *buf, pj_size_t size);
static int pjsip_name_addr_compare(  pjsip_uri_context_e context,
				     const pjsip_name_addr *naddr1,
				     const pjsip_name_addr *naddr2);
static int pjsip_url_print(  pjsip_uri_context_e context,
			     const pjsip_url *url, 
			     char *buf, pj_size_t size);
static int pjsip_url_compare( pjsip_uri_context_e context,
			      const pjsip_url *url1, const pjsip_url *url2);
static pjsip_url* pjsip_url_clone(pj_pool_t *pool, const pjsip_url *rhs);

static pjsip_uri_vptr sip_url_vptr = 
{
    HAPPY_FLAG &pjsip_url_get_scheme,
    HAPPY_FLAG &pjsip_get_uri,
    HAPPY_FLAG &pjsip_url_print,
    HAPPY_FLAG &pjsip_url_compare,
    HAPPY_FLAG &pjsip_url_clone
};

static pjsip_uri_vptr sips_url_vptr = 
{
    HAPPY_FLAG &pjsips_url_get_scheme,
    HAPPY_FLAG &pjsip_get_uri,
    HAPPY_FLAG &pjsip_url_print,
    HAPPY_FLAG &pjsip_url_compare,
    HAPPY_FLAG &pjsip_url_clone
};

static pjsip_uri_vptr name_addr_vptr = 
{
    HAPPY_FLAG &pjsip_name_addr_get_scheme,
    HAPPY_FLAG &pjsip_name_addr_get_uri,
    HAPPY_FLAG &pjsip_name_addr_print,
    HAPPY_FLAG &pjsip_name_addr_compare,
    HAPPY_FLAG &pjsip_name_addr_clone
};

static const pj_str_t *pjsip_url_get_scheme(const pjsip_url *url)
{
    PJ_UNUSED_ARG(url);
    return &sip_str;
}

static const pj_str_t *pjsips_url_get_scheme(const pjsip_url *url)
{
    PJ_UNUSED_ARG(url);
    return &sips_str;
}

static void *pjsip_get_uri( pjsip_uri *uri )
{
    return uri;
}

static void *pjsip_name_addr_get_uri( pjsip_name_addr *name )
{
    return name->uri;
}

PJ_DEF(void) pjsip_url_init(pjsip_url *url, int secure)
{
    pj_memset(url, 0, sizeof(*url));
    url->ttl_param = -1;
    url->vptr = secure ? &sips_url_vptr : &sip_url_vptr;
}

PJ_DEF(pjsip_url*) pjsip_url_create( pj_pool_t *pool, int secure )
{
    pjsip_url *url = pj_pool_alloc(pool, sizeof(pjsip_url));
    pjsip_url_init(url, secure);
    return url;
}

static int pjsip_url_print(  pjsip_uri_context_e context,
			     const pjsip_url *url, 
			     char *buf, pj_size_t size)
{
    int printed;
    pj_size_t size_required;
    char *startbuf = buf;
    const pj_str_t *scheme;
    *buf = '\0';

    /* Check the buffer length. */
    size_required = 6 + url->host.slen + 10 +
		    url->user.slen + url->passwd.slen + 2 +
		    url->user_param.slen + 6 +
		    url->method_param.slen + 8 +
		    url->transport_param.slen + 11 +
		    9 + 5 +
		    url->maddr_param.slen + 7 +
		    3 +
		    url->other_param.slen +
		    url->header_param.slen;
    if (size < size_required) {
	return -1;
    }

    /* Print scheme ("sip:" or "sips:") */
    scheme = pjsip_uri_get_scheme(url);
    copy_advance_no_check(buf, *scheme);
    *buf++ = ':';

    /* Print "user:password@", if any. */
    if (url->user.slen) {
	copy_advance_no_check(buf, url->user);
	if (url->passwd.slen) {
	    *buf++ = ':';
	    copy_advance_no_check(buf, url->passwd);
	}

	*buf++ = '@';
    }

    /* Print host. */
    pj_assert(url->host.slen != 0);
    copy_advance_no_check(buf, url->host);

    /* Only print port if it is explicitly specified. 
     * Port is not allowed in To and From header.
     */
    /* Unfortunately some UA requires us to send back the port
     * number exactly as it was sent. We don't remember whether an
     * UA has sent us port, so we'll just send the port indiscrimately
     */
    PJ_TODO(SHOULD_DISALLOW_URI_PORT_IN_FROM_TO_HEADER)
    if (url->port /*&& context != PJSIP_URI_IN_FROMTO_HDR*/) {
	*buf++ = ':';
	printed = pj_utoa(url->port, buf);
	buf += printed;
    }

    /* User param is allowed in all contexes */
    copy_advance_pair_no_check(buf, ";user=", 6, url->user_param);

    /* Method param is only allowed in external/other context. */
    if (context == PJSIP_URI_IN_OTHER) {
	copy_advance_pair_no_check(buf, ";method=", 8, url->method_param);
    }

    /* Transport is not allowed in From/To header. */
    if (context != PJSIP_URI_IN_FROMTO_HDR) {
	copy_advance_pair_no_check(buf, ";transport=", 11, url->transport_param);
    }

    /* TTL param is not allowed in From, To, Route, and Record-Route header. */
    if (url->ttl_param >= 0 && context != PJSIP_URI_IN_FROMTO_HDR &&
	context != PJSIP_URI_IN_ROUTING_HDR) 
    {
	pj_memcpy(buf, ";ttl=", 5);
	printed = pj_utoa(url->ttl_param, buf+5);
	buf += printed + 5;
    }

    /* maddr param is not allowed in From and To header. */
    if (context != PJSIP_URI_IN_FROMTO_HDR) {
	copy_advance_pair_no_check(buf, ";maddr=", 7, url->maddr_param);
    }

    /* lr param is not allowed in From, To, and Contact header. */
    if (url->lr_param && context != PJSIP_URI_IN_FROMTO_HDR &&
	context != PJSIP_URI_IN_CONTACT_HDR) 
    {
	pj_str_t lr = { ";lr", 3 };
	copy_advance_no_check(buf, lr);
    }

    /* Other param. */
    if (url->other_param.slen) {
	copy_advance_no_check(buf, url->other_param);
    }

    /* Header param. */
    if (url->header_param.slen) {
	copy_advance_no_check(buf, url->header_param);
    }

    *buf = '\0';
    return buf-startbuf;
}

static int pjsip_url_compare( pjsip_uri_context_e context,
			      const pjsip_url *url1, const pjsip_url *url2)
{
    /* The easiest (and probably the most efficient) way to compare two URLs
       are to print them, and compare them bytes per bytes. This technique
       works quite well with RFC3261, as the RFC (unlike RFC2543) defines that
       components specified in one URL does NOT match its default value if
       it is not specified in the second URL. For example, parameter "user=ip"
       does NOT match if it is omited in second URL.

       HOWEVER, THE SAME CAN NOT BE APPLIED FOR other-param NOR header-param.
       For these, each of the parameters must be compared one by one. Parameter
       that exists in one URL will match the comparison. But parameter that
       exists in both URLs and doesn't match wont match the URL comparison.

       The solution for this is to compare 'standard' URL components with
       bytes-to-bytes comparison, and compare other-param and header-param with
       more intelligent comparison.
     */
    char str_url1[PJSIP_MAX_URL_SIZE];
    char str_url2[PJSIP_MAX_URL_SIZE];
    int len1, len2;

    /* Must compare scheme first, as the second URI may not be SIP URL. */
    if (pj_stricmp(pjsip_uri_get_scheme(url1), pjsip_uri_get_scheme(url2)))
	return -1;

    len1 = pjsip_url_print(context, url1, str_url1, sizeof(str_url1));
    if (len1 < 1) {
	pj_assert(0);
	return -1;
    }
    len2 = pjsip_url_print(context, url2, str_url2, sizeof(str_url2));
    if (len2 < 1) {
	pj_assert(0);
	return -1;
    }

    if (len1 != len2) {
	/* Not equal. */
	return -1;
    }

    if (pj_native_strcmp(str_url1, str_url2)) {
	/* Not equal */
	return -1;
    }

    /* TODO: compare other-param and header-param in more intelligent manner. */
    PJ_TODO(HPARAM_AND_OTHER_PARAM_COMPARISON_IN_URL_COMPARISON)

    if (pj_strcmp(&url1->other_param, &url2->other_param)) {
	/* Not equal. */
	return -1;
    }
    if (pj_strcmp(&url1->header_param, &url2->header_param)) {
	/* Not equal. */
	return -1;
    }

    /* Seems to be equal, isn't it. */
    return 0;
    
}


PJ_DEF(void) pjsip_url_assign(pj_pool_t *pool, pjsip_url *url, 
			      const pjsip_url *rhs)
{
    pj_strdup( pool, &url->user, &rhs->user);
    pj_strdup( pool, &url->passwd, &rhs->passwd);
    pj_strdup( pool, &url->host, &rhs->host);
    url->port = rhs->port;
    pj_strdup( pool, &url->user_param, &rhs->user_param);
    pj_strdup( pool, &url->method_param, &rhs->method_param);
    pj_strdup( pool, &url->transport_param, &rhs->transport_param);
    url->ttl_param = rhs->ttl_param;
    pj_strdup( pool, &url->maddr_param, &rhs->maddr_param);
    pj_strdup( pool, &url->other_param, &rhs->other_param);
    pj_strdup( pool, &url->header_param, &rhs->header_param);
    url->lr_param = rhs->lr_param;
}

static pjsip_url* pjsip_url_clone(pj_pool_t *pool, const pjsip_url *rhs)
{
    pjsip_url *url = pj_pool_alloc(pool, sizeof(pjsip_url));
    if (!url)
	return NULL;

    pjsip_url_init(url, IS_SIPS(rhs));
    pjsip_url_assign(pool, url, rhs);
    return url;
}

static const pj_str_t *pjsip_name_addr_get_scheme(const pjsip_name_addr *name)
{
    pj_assert(name->uri != NULL);
    return pjsip_uri_get_scheme(name->uri);
}

PJ_DEF(void) pjsip_name_addr_init(pjsip_name_addr *name)
{
    name->vptr = &name_addr_vptr;
    name->uri = NULL;
    name->display.slen = 0;
}

PJ_DEF(pjsip_name_addr*) pjsip_name_addr_create(pj_pool_t *pool)
{
    pjsip_name_addr *name_addr = pj_pool_alloc(pool, sizeof(pjsip_name_addr));
    pjsip_name_addr_init(name_addr);
    return name_addr;
}

static int pjsip_name_addr_print( pjsip_uri_context_e context,
				  const pjsip_name_addr *name, 
				  char *buf, pj_size_t size)
{
    int printed;
    char *startbuf = buf;
    char *endbuf = buf + size;

    pj_assert(name->uri != NULL);

    if (context != PJSIP_URI_IN_REQ_URI) {
	copy_advance(buf, name->display);
	if (name->display.slen) {
	    *buf++ = ' ';
	}
	*buf++ = '<';
    }

    printed = pjsip_uri_print(context,name->uri, buf, size-(buf-startbuf));
    if (printed < 1)
	return -1;
    buf += printed;

    if (context != PJSIP_URI_IN_REQ_URI) {
	*buf++ = '>';
    }

    *buf = '\0';
    return buf-startbuf;
}

PJ_DEF(void) pjsip_name_addr_assign(pj_pool_t *pool, pjsip_name_addr *dst,
				    const pjsip_name_addr *src)
{
    pj_strdup( pool, &dst->display, &src->display);
    dst->uri = pjsip_uri_clone(pool, src->uri);
}

static pjsip_name_addr* pjsip_name_addr_clone( pj_pool_t *pool, 
					       const pjsip_name_addr *rhs)
{
    pjsip_name_addr *addr = pj_pool_alloc(pool, sizeof(pjsip_name_addr));
    if (!addr)
	return NULL;

    pjsip_name_addr_init(addr);
    pjsip_name_addr_assign(pool, addr, rhs);
    return addr;
}

static int pjsip_name_addr_compare(  pjsip_uri_context_e context,
				     const pjsip_name_addr *naddr1,
				     const pjsip_name_addr *naddr2)
{
    int d;

    /* I'm not sure whether display name is included in the comparison. */
    if (pj_strcmp(&naddr1->display, &naddr2->display) != 0) {
	return -1;
    }

    pj_assert( naddr1->uri != NULL );
    pj_assert( naddr2->uri != NULL );

    /* Compare name-addr as URL */
    d = pjsip_uri_cmp( context, naddr1->uri, naddr2->uri);
    if (d)
	return d;

    return 0;
}

