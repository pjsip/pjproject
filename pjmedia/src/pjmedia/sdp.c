/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#include <pjmedia/sdp.h>
#include <pjmedia/errno.h>
#include <pjlib-util/scanner.h>
#include <pj/array.h>
#include <pj/except.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/string.h>
#include <pj/pool.h>
#include <pj/assert.h>
#include <pj/ctype.h>


enum {
    SKIP_WS = 0,
    SYNTAX_ERROR = 1,
};
// New token definition from RFC 4566 (SDP)
#define TOKEN           "!#$%&'*+-.^_`{|}~"
//#define TOKEN         "-.!%*_=`'~"
//#define TOKEN         "'`-./:?\"#$&*;=@[]^_`{|}+~!"
#define NTP_OFFSET      ((pj_uint32_t)2208988800)
#define THIS_FILE       "sdp.c"

typedef struct parse_context
{ 
    pj_status_t last_error;
} parse_context;


/*
 * Prototypes for line parser.
 */
static void parse_version(pj_scanner *scanner, volatile parse_context *ctx);
static void parse_origin(pj_scanner *scanner, pjmedia_sdp_session *ses,
                         volatile parse_context *ctx);
static void parse_time(pj_scanner *scanner, pjmedia_sdp_session *ses,
                       volatile parse_context *ctx);
static void parse_generic_line(pj_scanner *scanner, pj_str_t *str,
                               volatile parse_context *ctx);
static void parse_connection_info(pj_scanner *scanner, pjmedia_sdp_conn *conn,
                                  volatile parse_context *ctx);
static void parse_bandwidth_info(pj_scanner *scanner, pjmedia_sdp_bandw *bandw,
                                 volatile parse_context *ctx);
static pjmedia_sdp_attr *parse_attr(pj_pool_t *pool, pj_scanner *scanner,
                                    volatile parse_context *ctx);
static void parse_media(pj_scanner *scanner, pjmedia_sdp_media *med,
                        volatile parse_context *ctx);
static void on_scanner_error(pj_scanner *scanner);

/*
 * Scanner character specification.
 */
static int is_initialized;
static pj_cis_buf_t cis_buf;
static pj_cis_t cs_digit, cs_token;

static void init_sdp_parser(void)
{
    if (is_initialized != 0)
        return;

    pj_enter_critical_section();

    if (is_initialized != 0) {
        pj_leave_critical_section();
        return;
    }
    
    pj_cis_buf_init(&cis_buf);

    pj_cis_init(&cis_buf, &cs_token);
    pj_cis_add_alpha(&cs_token);
    pj_cis_add_num(&cs_token);
    pj_cis_add_str(&cs_token, TOKEN);

    pj_cis_init(&cis_buf, &cs_digit);
    pj_cis_add_num(&cs_digit);

    is_initialized = 1;
    pj_leave_critical_section();
}

PJ_DEF(pjmedia_sdp_attr*) pjmedia_sdp_attr_create( pj_pool_t *pool,
                                                   const char *name,
                                                   const pj_str_t *value)
{
    pjmedia_sdp_attr *attr;

    PJ_ASSERT_RETURN(pool && name, NULL);

    attr = PJ_POOL_ALLOC_T(pool, pjmedia_sdp_attr);
    pj_strdup2(pool, &attr->name, name);

    if (value)
        pj_strdup_with_null(pool, &attr->value, value);
    else {
        attr->value.ptr = NULL;
        attr->value.slen = 0;
    }

    return attr;
}

PJ_DEF(pjmedia_sdp_attr*) pjmedia_sdp_attr_clone(pj_pool_t *pool, 
                                                 const pjmedia_sdp_attr *rhs)
{
    pjmedia_sdp_attr *attr;
    
    PJ_ASSERT_RETURN(pool && rhs, NULL);

    attr = PJ_POOL_ALLOC_T(pool, pjmedia_sdp_attr);

    pj_strdup(pool, &attr->name, &rhs->name);
    pj_strdup_with_null(pool, &attr->value, &rhs->value);

    return attr;
}

PJ_DEF(pjmedia_sdp_attr*) pjmedia_sdp_attr_find (unsigned count, 
                                       pjmedia_sdp_attr *const attr_array[],
                                       const pj_str_t *name,
                                       const pj_str_t *c_fmt)
{
    unsigned i;
    unsigned c_pt = 0xFFFF;
    unsigned long ul;

    PJ_ASSERT_RETURN(count <= PJMEDIA_MAX_SDP_ATTR, NULL);

    if (c_fmt) {
        if (pj_strtoul3(c_fmt, &ul, 10) != PJ_SUCCESS)
            return NULL;
        c_pt = (unsigned)ul;
    }

    for (i=0; i<count; ++i) {
        if (pj_strcmp(&attr_array[i]->name, name) == 0) {
            const pjmedia_sdp_attr *a = attr_array[i];
            if (c_fmt) {
                pj_str_t endptr;
                unsigned pt = (unsigned) pj_strtoul2(&a->value, &endptr, 10);
                if (endptr.ptr != a->value.ptr && pt == c_pt) {
                    return (pjmedia_sdp_attr*)a;
                }
            } else 
                return (pjmedia_sdp_attr*)a;
        }
    }
    return NULL;
}

PJ_DEF(pjmedia_sdp_attr*) pjmedia_sdp_attr_find2(unsigned count, 
                                       pjmedia_sdp_attr *const attr_array[],
                                       const char *c_name,
                                       const pj_str_t *c_fmt)
{
    pj_str_t name;

    name.ptr = (char*)c_name;
    name.slen = pj_ansi_strlen(c_name);

    return pjmedia_sdp_attr_find(count, attr_array, &name, c_fmt);
}



PJ_DEF(pj_status_t) pjmedia_sdp_attr_add(unsigned *count,
                                         pjmedia_sdp_attr *attr_array[],
                                         pjmedia_sdp_attr *attr)
{
    PJ_ASSERT_RETURN(count && attr_array && attr, PJ_EINVAL);

    if (*count >= PJMEDIA_MAX_SDP_ATTR) {
        PJ_PERROR(2, (THIS_FILE, PJ_ETOOMANY, 
                  "Error adding SDP attribute %.*s, "
                  "attr is ignored",
                  (int)attr->name.slen, attr->name.ptr));

        return PJ_ETOOMANY;
    }

    attr_array[*count] = attr;
    (*count)++;

    return PJ_SUCCESS;
}


PJ_DEF(unsigned) pjmedia_sdp_attr_remove_all(unsigned *count,
                                             pjmedia_sdp_attr *attr_array[],
                                             const char *name)
{
    unsigned i, removed = 0;
    pj_str_t attr_name;

    PJ_ASSERT_RETURN(count && attr_array && name, PJ_EINVAL);
    PJ_ASSERT_RETURN(*count <= PJMEDIA_MAX_SDP_ATTR, PJ_ETOOMANY);

    attr_name.ptr = (char*)name;
    attr_name.slen = pj_ansi_strlen(name);

    for (i=0; i<*count; ) {
        if (pj_strcmp(&attr_array[i]->name, &attr_name)==0) {
            pj_array_erase(attr_array, sizeof(pjmedia_sdp_attr*),
                           *count, i);
            --(*count);
            ++removed;
        } else {
            ++i;
        }   
    }

    return removed;
}


PJ_DEF(pj_status_t) pjmedia_sdp_attr_remove( unsigned *count,
                                             pjmedia_sdp_attr *attr_array[],
                                             pjmedia_sdp_attr *attr )
{
    unsigned i, removed=0;

    PJ_ASSERT_RETURN(count && attr_array && attr, PJ_EINVAL);
    PJ_ASSERT_RETURN(*count <= PJMEDIA_MAX_SDP_ATTR, PJ_ETOOMANY);

    for (i=0; i<*count; ) {
        if (attr_array[i] == attr) {
            pj_array_erase(attr_array, sizeof(pjmedia_sdp_attr*),
                           *count, i);
            --(*count);
            ++removed;
        } else {
            ++i;
        }
    }

    return removed ? PJ_SUCCESS : PJ_ENOTFOUND;
}


PJ_DEF(pj_status_t) pjmedia_sdp_attr_get_rtpmap( const pjmedia_sdp_attr *attr,
                                                 pjmedia_sdp_rtpmap *rtpmap)
{
    pj_scanner scanner;
    pj_str_t token;
    pj_status_t status = -1;
    char term = 0;
    PJ_USE_EXCEPTION;

    PJ_ASSERT_RETURN(pj_strcmp2(&attr->name, "rtpmap")==0, PJ_EINVALIDOP);

    if (attr->value.slen == 0)
        return PJMEDIA_SDP_EINATTR;

    init_sdp_parser();

    /* Check if input is null terminated, and null terminate if
     * necessary. Unfortunately this may crash the application if
     * attribute was allocated from a read-only memory location.
     * But this shouldn't happen as attribute's value normally is
     * null terminated.
     */
    if (attr->value.ptr[attr->value.slen] != 0 &&
        attr->value.ptr[attr->value.slen] != '\r' &&
        attr->value.ptr[attr->value.slen] != '\n')
    {
        pj_assert(!"Shouldn't happen");
        term = attr->value.ptr[attr->value.slen];
        attr->value.ptr[attr->value.slen] = '\0';
    }

    /* The buffer passed to the scanner is not guaranteed to be NULL
     * terminated, but should be safe. See ticket #2063.
     */    
    pj_scan_init(&scanner, (char*)attr->value.ptr, attr->value.slen,
                 PJ_SCAN_AUTOSKIP_WS, &on_scanner_error);

    /* rtpmap sample:
     *  a=rtpmap:98 L16/16000/2.
     */

    /* Init */
    rtpmap->pt.slen = rtpmap->param.slen = rtpmap->enc_name.slen = 0;
    rtpmap->clock_rate = 0;

    /* Parse */
    PJ_TRY {

        /* Get payload type. */
        pj_scan_get(&scanner, &cs_token, &rtpmap->pt);


        /* Get encoding name. */
        pj_scan_get(&scanner, &cs_token, &rtpmap->enc_name);

        /* Expecting '/' after encoding name. */
        if (pj_scan_get_char(&scanner) != '/') {
            status = PJMEDIA_SDP_EINRTPMAP;
            goto on_return;
        }


        /* Get the clock rate. */
        pj_scan_get(&scanner, &cs_digit, &token);
        rtpmap->clock_rate = pj_strtoul(&token);

        /* Expecting either '/' or EOF */
        if (*scanner.curptr == '/') {
            /* Skip the '/' */
            pj_scan_get_char(&scanner);
            pj_scan_get(&scanner, &cs_token, &rtpmap->param);
        } else {
            rtpmap->param.slen = 0;
        }

        status = PJ_SUCCESS;
    }
    PJ_CATCH_ANY {
        status = PJMEDIA_SDP_EINRTPMAP;
    }
    PJ_END;


on_return:
    pj_scan_fini(&scanner);
    if (term) {
        attr->value.ptr[attr->value.slen] = term;
    }
    return status;
}

PJ_DEF(pj_status_t) pjmedia_sdp_attr_get_fmtp( const pjmedia_sdp_attr *attr,
                                               pjmedia_sdp_fmtp *fmtp)
{
    const char *p = attr->value.ptr;
    const char *end = attr->value.ptr + attr->value.slen;
    pj_str_t token;

    PJ_ASSERT_RETURN(pj_strcmp2(&attr->name, "fmtp")==0, PJ_EINVALIDOP);

    if (attr->value.slen == 0)
        return PJMEDIA_SDP_EINATTR;

    /* fmtp BNF:
     *  a=fmtp:<format> <format specific parameter>
     */

    /* Get format. */
    token.ptr = (char*)p;
    while (pj_isdigit(*p) && p!=end)
        ++p;
    token.slen = p - token.ptr;
    if (token.slen == 0)
        return PJMEDIA_SDP_EINFMTP;

    fmtp->fmt = token;

    /* Expecting space after format. */
    if (*p != ' ') return PJMEDIA_SDP_EINFMTP;

    /* Get space. */
    ++p;

    /* Set the remaining string as fmtp format parameter. */
    fmtp->fmt_param.ptr = (char*)p;
    fmtp->fmt_param.slen = end - p;

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_sdp_attr_get_rtcp(const pjmedia_sdp_attr *attr,
                                              pjmedia_sdp_rtcp_attr *rtcp)
{
    pj_scanner scanner;
    pj_str_t token;
    pj_status_t status = -1;
    PJ_USE_EXCEPTION;

    PJ_ASSERT_RETURN(pj_strcmp2(&attr->name, "rtcp")==0, PJ_EINVALIDOP);

    if (attr->value.slen == 0)
        return PJMEDIA_SDP_EINATTR;

    init_sdp_parser();

    /* fmtp BNF:
     *  a=rtcp:<port> [nettype addrtype address]
     */

    /* The buffer passed to the scanner is not guaranteed to be NULL
     * terminated, but should be safe. See ticket #2063.
     */
    pj_scan_init(&scanner, (char*)attr->value.ptr, attr->value.slen,
                 PJ_SCAN_AUTOSKIP_WS, &on_scanner_error);

    /* Init */
    rtcp->net_type.slen = rtcp->addr_type.slen = rtcp->addr.slen = 0;

    /* Parse */
    PJ_TRY {

        /* Get the port */
        pj_scan_get(&scanner, &cs_digit, &token);
        rtcp->port = pj_strtoul(&token);
        if (rtcp->port > 0xFFFF)
            PJ_THROW(PJMEDIA_SDP_EINRTCP);

        /* Have address? */
        if (!pj_scan_is_eof(&scanner)) {

            /* Get network type */
            pj_scan_get(&scanner, &cs_token, &rtcp->net_type);

            /* Get address type */
            pj_scan_get(&scanner, &cs_token, &rtcp->addr_type);

            /* Get the address */
            //pj_scan_get(&scanner, &cs_token, &rtcp->addr);
            pj_scan_get_until_chr(&scanner, "/ \t\r\n", &rtcp->addr);

        }

        status = PJ_SUCCESS;

    }
    PJ_CATCH_ANY {
        status = PJMEDIA_SDP_EINRTCP;
    }
    PJ_END;

    pj_scan_fini(&scanner);
    return status;
}


PJ_DEF(pjmedia_sdp_attr*) pjmedia_sdp_attr_create_rtcp(pj_pool_t *pool,
                                                       const pj_sockaddr *a)
{
    enum {
        ATTR_LEN = PJ_INET6_ADDRSTRLEN+16
    };
    char tmp_addr[PJ_INET6_ADDRSTRLEN];
    pjmedia_sdp_attr *attr;

    attr = PJ_POOL_ALLOC_T(pool, pjmedia_sdp_attr);
    attr->name = pj_str("rtcp");
    attr->value.ptr = (char*) pj_pool_alloc(pool, ATTR_LEN);
    if (a->addr.sa_family == pj_AF_INET()) {
        attr->value.slen = 
            pj_ansi_snprintf(attr->value.ptr, ATTR_LEN,
                            "%u IN IP4 %s",
                            pj_sockaddr_get_port(a),
                            pj_sockaddr_print(a, tmp_addr, 
                                              sizeof(tmp_addr), 0));
    } else if (a->addr.sa_family == pj_AF_INET6()) {
        attr->value.slen = 
            pj_ansi_snprintf(attr->value.ptr, ATTR_LEN,
                            "%u IN IP6 %s",
                            pj_sockaddr_get_port(a),
                            pj_sockaddr_print(a, tmp_addr, 
                                              sizeof(tmp_addr), 0));

    } else {
        pj_assert(!"Unsupported address family");
        return NULL;
    }

    return attr;
}


PJ_DEF(pj_status_t) pjmedia_sdp_attr_get_ssrc(const pjmedia_sdp_attr *attr,
                                              pjmedia_sdp_ssrc_attr *ssrc)
{
    pj_scanner scanner;
    pj_str_t token;
    pj_status_t status = -1;
    PJ_USE_EXCEPTION;

    PJ_ASSERT_RETURN(pj_strcmp2(&attr->name, "ssrc")==0, PJ_EINVALIDOP);

    if (attr->value.slen == 0)
        return PJMEDIA_SDP_EINATTR;

    init_sdp_parser();

    /* ssrc BNF:
     *  a=ssrc:<ssrc-id> <attribute>
     *  a=ssrc:<ssrc-id> <attribute>:<value>
     */

    /* The buffer passed to the scanner is not guaranteed to be NULL
     * terminated, but should be safe. See ticket #2063.
     */
    pj_scan_init(&scanner, (char*)attr->value.ptr, attr->value.slen,
                 PJ_SCAN_AUTOSKIP_WS, &on_scanner_error);

    /* Init */
    pj_bzero(ssrc, sizeof(*ssrc));

    /* Parse */
    PJ_TRY {
        pj_str_t scan_attr;

        /* Get the ssrc */
        pj_scan_get(&scanner, &cs_digit, &token);
        ssrc->ssrc = pj_strtoul(&token);

        pj_scan_get_char(&scanner);
        pj_scan_get(&scanner, &cs_token, &scan_attr);
        
        /* Get cname attribute, if any */
        if (!pj_scan_is_eof(&scanner) &&
            pj_scan_get_char(&scanner) == ':' &&
            pj_strcmp2(&scan_attr, "cname"))
        {
            pj_scan_get(&scanner, &cs_token, &ssrc->cname);
        }

        status = PJ_SUCCESS;

    }
    PJ_CATCH_ANY {
        status = PJMEDIA_SDP_EINSSRC;
    }
    PJ_END;

    pj_scan_fini(&scanner);
    return status;
}


PJ_DEF(pjmedia_sdp_attr*) pjmedia_sdp_attr_create_ssrc( pj_pool_t *pool,
                                                        pj_uint32_t ssrc,
                                                        const pj_str_t *cname)
{
    pjmedia_sdp_attr *attr;

    if (cname->slen == 0)
        return NULL;

    attr = PJ_POOL_ALLOC_T(pool, pjmedia_sdp_attr);
    attr->name = pj_str("ssrc");
    attr->value.ptr = (char*) pj_pool_alloc(pool, cname->slen+7 /* " cname:"*/
                                                  + 10 /* 32-bit integer */
                                                  + 1 /* NULL */);
    attr->value.slen = pj_ansi_snprintf(attr->value.ptr, cname->slen+18,
                                        "%u cname:%.*s", ssrc,
                                        (int)cname->slen, cname->ptr);

    return attr;
}


PJ_DEF(pj_status_t) pjmedia_sdp_attr_to_rtpmap(pj_pool_t *pool,
                                               const pjmedia_sdp_attr *attr,
                                               pjmedia_sdp_rtpmap **p_rtpmap)
{
    PJ_ASSERT_RETURN(pool && attr && p_rtpmap, PJ_EINVAL);

    *p_rtpmap = PJ_POOL_ALLOC_T(pool, pjmedia_sdp_rtpmap);
    PJ_ASSERT_RETURN(*p_rtpmap, PJ_ENOMEM);

    return pjmedia_sdp_attr_get_rtpmap(attr, *p_rtpmap);
}


PJ_DEF(pj_status_t) pjmedia_sdp_rtpmap_to_attr(pj_pool_t *pool,
                                               const pjmedia_sdp_rtpmap *rtpmap,
                                               pjmedia_sdp_attr **p_attr)
{
    pjmedia_sdp_attr *attr;
    char tempbuf[128];
    int len;

    /* Check arguments. */
    PJ_ASSERT_RETURN(pool && rtpmap && p_attr, PJ_EINVAL);

    /* Check that mandatory attributes are specified. */
    PJ_ASSERT_RETURN(rtpmap->enc_name.slen && rtpmap->clock_rate,
                     PJMEDIA_SDP_EINRTPMAP);


    attr = PJ_POOL_ALLOC_T(pool, pjmedia_sdp_attr);
    PJ_ASSERT_RETURN(attr != NULL, PJ_ENOMEM);

    attr->name.ptr = "rtpmap";
    attr->name.slen = 6;

    /* Format: ":pt enc_name/clock_rate[/param]" */
    len = pj_ansi_snprintf(tempbuf, sizeof(tempbuf), 
                           "%.*s %.*s/%u%s%.*s",
                           (int)rtpmap->pt.slen,
                           rtpmap->pt.ptr,
                           (int)rtpmap->enc_name.slen,
                           rtpmap->enc_name.ptr,
                           rtpmap->clock_rate,
                           (rtpmap->param.slen ? "/" : ""),
                           (int)rtpmap->param.slen,
                           rtpmap->param.ptr);

    if (len < 1 || len >= (int)sizeof(tempbuf))
        return PJMEDIA_SDP_ERTPMAPTOOLONG;

    attr->value.slen = len;
    attr->value.ptr = (char*) pj_pool_alloc(pool, attr->value.slen+1);
    pj_memcpy(attr->value.ptr, tempbuf, attr->value.slen+1);

    *p_attr = attr;
    return PJ_SUCCESS;
}


static int print_connection_info( pjmedia_sdp_conn *c, char *buf, int len)
{
    int printed;

    printed = pj_ansi_snprintf(buf, len, "c=%.*s %.*s %.*s\r\n",
                               (int)c->net_type.slen,
                               c->net_type.ptr,
                               (int)c->addr_type.slen,
                               c->addr_type.ptr,
                               (int)c->addr.slen,
                               c->addr.ptr);
    if (printed < 1 || printed >= len)
        return -1;

    return printed;
}


PJ_DEF(pjmedia_sdp_conn*) pjmedia_sdp_conn_clone (pj_pool_t *pool, 
                                                  const pjmedia_sdp_conn *rhs)
{
    pjmedia_sdp_conn *c = PJ_POOL_ALLOC_T(pool, pjmedia_sdp_conn);
    if (!c) return NULL;

    if (!pj_strdup (pool, &c->net_type, &rhs->net_type)) return NULL;
    if (!pj_strdup (pool, &c->addr_type, &rhs->addr_type)) return NULL;
    if (!pj_strdup (pool, &c->addr, &rhs->addr)) return NULL;

    return c;
}

PJ_DEF(pjmedia_sdp_bandw*)
pjmedia_sdp_bandw_clone (pj_pool_t *pool, 
                         const pjmedia_sdp_bandw *rhs)
{
    pjmedia_sdp_bandw *b = PJ_POOL_ALLOC_T(pool, pjmedia_sdp_bandw);
    if (!b) return NULL;

    if (!pj_strdup (pool, &b->modifier, &rhs->modifier)) return NULL;
    b->value = rhs->value;

    return b;
}

static pj_ssize_t print_bandw(const pjmedia_sdp_bandw *bandw,
                              char *buf, pj_size_t len)
{
    char *p = buf;

    if ((int)len < bandw->modifier.slen + 10 + 5)
        return -1;

    *p++ = 'b';
    *p++ = '=';
    pj_memcpy(p, bandw->modifier.ptr, bandw->modifier.slen);
    p += bandw->modifier.slen;
    *p++ = ':';
    p += pj_utoa(bandw->value, p);

    *p++ = '\r';
    *p++ = '\n';
    return p-buf;
}

static pj_ssize_t print_attr(const pjmedia_sdp_attr *attr, 
                             char *buf, pj_size_t len)
{
    char *p = buf;

    if ((int)len < attr->name.slen + attr->value.slen + 10)
        return -1;

    *p++ = 'a';
    *p++ = '=';
    pj_memcpy(p, attr->name.ptr, attr->name.slen);
    p += attr->name.slen;
    

    if (attr->value.slen) {
        *p++ = ':';
        pj_memcpy(p, attr->value.ptr, attr->value.slen);
        p += attr->value.slen;
    }

    *p++ = '\r';
    *p++ = '\n';
    return p-buf;
}

static int print_media_desc(const pjmedia_sdp_media *m, char *buf, pj_size_t len)
{
    char *p = buf;
    char *end = buf+len;
    unsigned i;
    int printed;

    /* check length for the "m=" line. */
    if (len < (pj_size_t)m->desc.media.slen+m->desc.transport.slen+12+24) {
        return -1;
    }
    *p++ = 'm';     /* m= */
    *p++ = '=';
    pj_memcpy(p, m->desc.media.ptr, m->desc.media.slen);
    p += m->desc.media.slen;
    *p++ = ' ';
    printed = pj_utoa(m->desc.port, p);
    p += printed;
    if (m->desc.port_count > 1) {
        *p++ = '/';
        printed = pj_utoa(m->desc.port_count, p);
        p += printed;
    }
    *p++ = ' ';
    pj_memcpy(p, m->desc.transport.ptr, m->desc.transport.slen);
    p += m->desc.transport.slen;
    for (i=0; i<m->desc.fmt_count; ++i) {
        if (end-p > m->desc.fmt[i].slen) {
            *p++ = ' ';
            pj_memcpy(p, m->desc.fmt[i].ptr, m->desc.fmt[i].slen);
            p += m->desc.fmt[i].slen;
        } else {
            return -1;
        }
    }

    if (end-p >= 2) {
        *p++ = '\r';
        *p++ = '\n';
    } else {
        return -1;
    }

    /* print connection info, if present. */
    if (m->conn) {
        printed = print_connection_info(m->conn, p, (int)(end-p));
        if (printed < 0) {
            return -1;
        }
        p += printed;
    }
    
    /* print optional bandwidth info. */
    for (i=0; i<m->bandw_count; ++i) {
        printed = (int)print_bandw(m->bandw[i], p, end-p);
        if (printed < 0) {
            return -1;
        }
        p += printed;
    }

    /* print attributes. */
    for (i=0; i<m->attr_count; ++i) {
        printed = (int)print_attr(m->attr[i], p, end-p);
        if (printed < 0) {
            return -1;
        }
        p += printed;
    }

    return (int)(p-buf);
}

PJ_DEF(int) pjmedia_sdp_media_print(const pjmedia_sdp_media *media,
                               char *buf, pj_size_t size)
{
    return print_media_desc(media, buf, size);
}

PJ_DEF(int) pjmedia_sdp_attr_print(const pjmedia_sdp_attr *attr,
                               char *buf, pj_size_t size)
{
    return print_attr(attr, buf, size);
}

PJ_DEF(pjmedia_sdp_media*) pjmedia_sdp_media_clone(
                                                 pj_pool_t *pool, 
                                                 const pjmedia_sdp_media *rhs)
{
    unsigned int i;
    pjmedia_sdp_media *m = PJ_POOL_ALLOC_T(pool, pjmedia_sdp_media);
    PJ_ASSERT_RETURN(m != NULL, NULL);

    pj_strdup (pool, &m->desc.media, &rhs->desc.media);
    m->desc.port = rhs->desc.port;
    m->desc.port_count = rhs->desc.port_count;
    pj_strdup (pool, &m->desc.transport, &rhs->desc.transport);
    m->desc.fmt_count = rhs->desc.fmt_count;
    for (i=0; i<rhs->desc.fmt_count; ++i)
        pj_strdup(pool, &m->desc.fmt[i], &rhs->desc.fmt[i]);

    if (rhs->conn) {
        m->conn = pjmedia_sdp_conn_clone (pool, rhs->conn);
        PJ_ASSERT_RETURN(m->conn != NULL, NULL);
    } else {
        m->conn = NULL;
    }

    m->bandw_count = rhs->bandw_count;
    for (i=0; i < rhs->bandw_count; ++i) {
        m->bandw[i] = pjmedia_sdp_bandw_clone (pool, rhs->bandw[i]);
        PJ_ASSERT_RETURN(m->bandw[i] != NULL, NULL);
    }

    m->attr_count = rhs->attr_count;
    for (i=0; i < rhs->attr_count; ++i) {
        m->attr[i] = pjmedia_sdp_attr_clone (pool, rhs->attr[i]);
        PJ_ASSERT_RETURN(m->attr[i] != NULL, NULL);
    }

    return m;
}

PJ_DEF(pjmedia_sdp_attr*) pjmedia_sdp_media_find_attr(
                                const pjmedia_sdp_media *m,
                                const pj_str_t *name, const pj_str_t *fmt)
{
    PJ_ASSERT_RETURN(m && name, NULL);
    return pjmedia_sdp_attr_find(m->attr_count, m->attr, name, fmt);
}



PJ_DEF(pjmedia_sdp_attr*) pjmedia_sdp_media_find_attr2(
                                const pjmedia_sdp_media *m,
                                const char *name, const pj_str_t *fmt)
{
    PJ_ASSERT_RETURN(m && name, NULL);
    return pjmedia_sdp_attr_find2(m->attr_count, m->attr, name, fmt);
}


PJ_DEF(pj_status_t) pjmedia_sdp_media_add_attr( pjmedia_sdp_media *m,
                                                pjmedia_sdp_attr *attr)
{
    return pjmedia_sdp_attr_add(&m->attr_count, m->attr, attr);
}

PJ_DEF(pj_status_t) pjmedia_sdp_session_add_attr(pjmedia_sdp_session *s,
                                                 pjmedia_sdp_attr *attr)
{
    return pjmedia_sdp_attr_add(&s->attr_count, s->attr, attr);
}

PJ_DEF(unsigned) pjmedia_sdp_media_remove_all_attr(pjmedia_sdp_media *m,
                                                   const char *name)
{
    return pjmedia_sdp_attr_remove_all(&m->attr_count, m->attr, name);
}

PJ_DEF(pj_status_t) pjmedia_sdp_media_remove_attr(pjmedia_sdp_media *m,
                                                  pjmedia_sdp_attr *attr)
{
    return pjmedia_sdp_attr_remove(&m->attr_count, m->attr, attr);
}

static int print_session(const pjmedia_sdp_session *ses, 
                         char *buf, pj_ssize_t len)
{
    char *p = buf;
    char *end = buf+len;
    unsigned i;
    int printed;

    /* Check length for v= and o= lines. */
    if (len < 5+ 
              2+ses->origin.user.slen+18+
              ses->origin.net_type.slen+ses->origin.addr.slen + 2)
    {
        return -1;
    }

    /* SDP version (v= line) */
    pj_memcpy(p, "v=0\r\n", 5);
    p += 5;

    /* Owner (o=) line. */
    *p++ = 'o';
    *p++ = '=';
    pj_memcpy(p, ses->origin.user.ptr, ses->origin.user.slen);
    p += ses->origin.user.slen;
    *p++ = ' ';
    printed = pj_utoa2(ses->origin.id, p);
    p += printed;
    *p++ = ' ';
    printed = pj_utoa2(ses->origin.version, p);
    p += printed;
    *p++ = ' ';
    pj_memcpy(p, ses->origin.net_type.ptr, ses->origin.net_type.slen);
    p += ses->origin.net_type.slen;
    *p++ = ' ';
    pj_memcpy(p, ses->origin.addr_type.ptr, ses->origin.addr_type.slen);
    p += ses->origin.addr_type.slen;
    *p++ = ' ';
    pj_memcpy(p, ses->origin.addr.ptr, ses->origin.addr.slen);
    p += ses->origin.addr.slen;
    *p++ = '\r';
    *p++ = '\n';

    /* Session name (s=) line. */
    if ((end-p)  < 8+ses->name.slen) {
        return -1;
    }
    *p++ = 's';
    *p++ = '=';
    pj_memcpy(p, ses->name.ptr, ses->name.slen);
    p += ses->name.slen;
    *p++ = '\r';
    *p++ = '\n';

    /* Connection line (c=) if exist. */
    if (ses->conn) {
        printed = print_connection_info(ses->conn, p, (int)(end-p));
        if (printed < 1) {
            return -1;
        }
        p += printed;
    }

    /* print optional bandwidth info. */
    for (i=0; i<ses->bandw_count; ++i) {
        printed = (int)print_bandw(ses->bandw[i], p, end-p);
        if (printed < 1) {
            return -1;
        }
        p += printed;
    }

    /* Time */
    if ((end-p) < 24) {
        return -1;
    }
    *p++ = 't';
    *p++ = '=';
    printed = pj_utoa2(ses->time.start, p);
    p += printed;
    *p++ = ' ';
    printed = pj_utoa2(ses->time.stop, p);
    p += printed;
    *p++ = '\r';
    *p++ = '\n';

    /* Print all attribute (a=) lines. */
    for (i=0; i<ses->attr_count; ++i) {
        printed = (int)print_attr(ses->attr[i], p, end-p);
        if (printed < 0) {
            return -1;
        }
        p += printed;
    }

    /* Print media (m=) lines. */
    for (i=0; i<ses->media_count; ++i) {
        printed = print_media_desc(ses->media[i], p, (int)(end-p));
        if (printed < 0) {
            return -1;
        }
        p += printed;
    }

    return (int)(p-buf);
}

/******************************************************************************
 * PARSERS
 */

static void parse_version(pj_scanner *scanner, 
                          volatile parse_context *ctx)
{
    ctx->last_error = PJMEDIA_SDP_EINVER;

    /* check equal sign */
    if (scanner->curptr+1 >= scanner->end || *(scanner->curptr+1) != '=') {
        on_scanner_error(scanner);
        return;
    }

    /* check version is 0 */
    if (scanner->curptr+2 >= scanner->end || *(scanner->curptr+2) != '0') {
        on_scanner_error(scanner);
        return;
    }

    /* We've got what we're looking for, skip anything until newline */
    pj_scan_skip_line(scanner);
}

static void parse_origin(pj_scanner *scanner, pjmedia_sdp_session *ses,
                         volatile parse_context *ctx)
{
    pj_str_t str;
    pj_uint_t ui;

    ctx->last_error = PJMEDIA_SDP_EINORIGIN;

    /* check equal sign */
    if (scanner->curptr+1 >= scanner->end || *(scanner->curptr+1) != '=') {
        on_scanner_error(scanner);
        return;
    }

    /* o= */
    pj_scan_advance_n(scanner, 2, SKIP_WS);

    /* username. */
    pj_scan_get_until_ch(scanner, ' ', &ses->origin.user);
    pj_scan_get_char(scanner);

    /* id */
    pj_scan_get_until_ch(scanner, ' ', &str);
    if (pj_strtoul4(&str, &ui, 10) != PJ_SUCCESS){
        on_scanner_error(scanner);
        return;
    }
    ses->origin.id = (pj_uint_t)ui;
    pj_scan_get_char(scanner);

    /* version */
    pj_scan_get_until_ch(scanner, ' ', &str);
    if (pj_strtoul4(&str, &ui, 10) != PJ_SUCCESS){
        on_scanner_error(scanner);
        return;
    }
    ses->origin.version = (pj_uint_t)ui;
    pj_scan_get_char(scanner);

    /* network-type */
    pj_scan_get_until_ch(scanner, ' ', &ses->origin.net_type);
    pj_scan_get_char(scanner);

    /* addr-type */
    pj_scan_get_until_ch(scanner, ' ', &ses->origin.addr_type);
    pj_scan_get_char(scanner);

    /* address */
    pj_scan_get_until_chr(scanner, " \t\r\n", &ses->origin.addr);

    /* We've got what we're looking for, skip anything until newline */
    pj_scan_skip_line(scanner);

}

static void parse_time(pj_scanner *scanner, pjmedia_sdp_session *ses,
                       volatile parse_context *ctx)
{
    pj_str_t str;
    pj_uint_t ui;

    ctx->last_error = PJMEDIA_SDP_EINTIME;

    /* check equal sign */
    if (scanner->curptr+1 >= scanner->end || *(scanner->curptr+1) != '=') {
        on_scanner_error(scanner);
        return;
    }

    /* t= */
    pj_scan_advance_n(scanner, 2, SKIP_WS);

    /* start time */
    pj_scan_get_until_ch(scanner, ' ', &str);
    if (pj_strtoul4(&str, &ui, 10) != PJ_SUCCESS){
        on_scanner_error(scanner);
        return;
    }
    ses->time.start = (pj_uint_t)ui;

    pj_scan_get_char(scanner);

    /* stop time */
    pj_scan_get_until_chr(scanner, " \t\r\n", &str);
    if (pj_strtoul4(&str, &ui, 10) != PJ_SUCCESS){
        on_scanner_error(scanner);
        return;
    }
    ses->time.stop = (pj_uint_t)ui;

    /* We've got what we're looking for, skip anything until newline */
    pj_scan_skip_line(scanner);
}

static void parse_generic_line(pj_scanner *scanner, pj_str_t *str,
                               volatile parse_context *ctx)
{
    ctx->last_error = PJMEDIA_SDP_EINSDP;

    /* check equal sign */
    if ((scanner->curptr+1 >= scanner->end) || *(scanner->curptr+1) != '=') {
        on_scanner_error(scanner);
        return;
    }

    /* x= */
    pj_scan_advance_n(scanner, 2, SKIP_WS);

    /* get anything until newline (including whitespaces). */
    pj_scan_get_until_chr(scanner, "\r\n", str);

    /* newline. */
    pj_scan_get_newline(scanner);
}

static void parse_connection_info(pj_scanner *scanner, pjmedia_sdp_conn *conn,
                                  volatile parse_context *ctx)
{
    ctx->last_error = PJMEDIA_SDP_EINCONN;

    /* c= */
    pj_scan_advance_n(scanner, 2, SKIP_WS);

    /* network-type */
    pj_scan_get_until_ch(scanner, ' ', &conn->net_type);
    pj_scan_get_char(scanner);

    /* addr-type */
    pj_scan_get_until_ch(scanner, ' ', &conn->addr_type);
    pj_scan_get_char(scanner);

    /* address. */
    pj_scan_get_until_chr(scanner, "/ \t\r\n", &conn->addr);
    /* Parse multicast details, if any. */
    if (*scanner->curptr == '/') {
        pj_str_t str;
        unsigned long ul;

        pj_scan_get_until_chr(scanner, "/ \t\r\n", &str);
        if (*scanner->curptr == '/') {
            if ((pj_strtoul3(&str, &ul, 10) != PJ_SUCCESS) || ul > 255) {
                on_scanner_error(scanner);
                return;
            }

            conn->ttl = (pj_uint8_t)ul;
        }        

        if ((pj_strtoul3(&str, &ul, 10) != PJ_SUCCESS) || ul > 255) {
            on_scanner_error(scanner);
            return;
        }
        conn->no_addr = (pj_uint8_t)ul;

    }

    /* We've got what we're looking for, skip anything until newline */
    pj_scan_skip_line(scanner);
}

static void parse_bandwidth_info(pj_scanner *scanner, pjmedia_sdp_bandw *bandw,
                                 volatile parse_context *ctx)
{
    pj_str_t str;
    unsigned long ul;

    ctx->last_error = PJMEDIA_SDP_EINBANDW;

    /* b= */
    pj_scan_advance_n(scanner, 2, SKIP_WS);

    /* modifier */
    pj_scan_get_until_ch(scanner, ':', &bandw->modifier);
    pj_scan_get_char(scanner);

    /* value */
    pj_scan_get_until_chr(scanner, " \t\r\n", &str);
    if (pj_strtoul3(&str, &ul, 10) != PJ_SUCCESS){
        on_scanner_error(scanner);
        return;
    }
    bandw->value = (pj_uint32_t)ul;

    /* We've got what we're looking for, skip anything until newline */
    pj_scan_skip_line(scanner);
}

static void parse_media(pj_scanner *scanner, pjmedia_sdp_media *med,
                        volatile parse_context *ctx)
{
    pj_str_t str;
    unsigned long num;
    pj_status_t status;

    ctx->last_error = PJMEDIA_SDP_EINMEDIA;

    /* check the equal sign */
    if (scanner->curptr+1 >= scanner->end || *(scanner->curptr+1) != '=') {
        on_scanner_error(scanner);
        return;
    }

    /* m= */
    pj_scan_advance_n(scanner, 2, SKIP_WS);

    /* type */
    pj_scan_get(scanner, &cs_token, &med->desc.media);

    if (pj_scan_get_char(scanner) != ' ') {
        on_scanner_error(scanner);
    }

    /* port */
    pj_scan_get(scanner, &cs_token, &str);
    status = pj_strtoul3(&str, &num, 10);
    if (status != PJ_SUCCESS || pj_scan_is_eof(scanner) || num > 0xFFFF) {
        on_scanner_error(scanner);
        return;
    }
    med->desc.port = (unsigned short)num;
    if (*scanner->curptr == '/') {
        /* port count */
        pj_scan_get_char(scanner);
        pj_scan_get(scanner, &cs_token, &str);
        status = pj_strtoul3(&str, &num, 10);
        if (status != PJ_SUCCESS) {
            on_scanner_error(scanner);
            return;
        }
        med->desc.port_count = (unsigned)num;
    } else {
        med->desc.port_count = 0;
    }

    if (pj_scan_get_char(scanner) != ' ') {
        on_scanner_error(scanner);
    }

    /* transport */
    pj_scan_get_until_chr(scanner, " \t\r\n", &med->desc.transport);

    /* format list */
    med->desc.fmt_count = 0;
    while (scanner->curptr < scanner->end && *scanner->curptr == ' ') {
        pj_str_t fmt;

        pj_scan_get_char(scanner);

        /* Check again for the end of the line */
        if ((*scanner->curptr == '\r') || (*scanner->curptr == '\n'))
                break;

        pj_scan_get(scanner, &cs_token, &fmt);
        if (med->desc.fmt_count < PJMEDIA_MAX_SDP_FMT)
            med->desc.fmt[med->desc.fmt_count++] = fmt;
        else
            PJ_PERROR(2,(THIS_FILE, PJ_ETOOMANY, 
                         "Error adding SDP media format %.*s, "
                         "format is ignored",
                         (int)fmt.slen, fmt.ptr));
    }

    /* We've got what we're looking for, skip anything until newline */
    pj_scan_skip_line(scanner);
}

static void on_scanner_error(pj_scanner *scanner)
{
    PJ_UNUSED_ARG(scanner);

    PJ_THROW(SYNTAX_ERROR);
}

static pjmedia_sdp_attr *parse_attr( pj_pool_t *pool, pj_scanner *scanner,
                                    volatile parse_context *ctx)
{
    pjmedia_sdp_attr *attr;

    ctx->last_error = PJMEDIA_SDP_EINATTR;

    attr = PJ_POOL_ALLOC_T(pool, pjmedia_sdp_attr);

    /* check equal sign */
    if (scanner->curptr+1 >= scanner->end || *(scanner->curptr+1) != '=') {
        on_scanner_error(scanner);
        return NULL;
    }

    /* skip a= */
    pj_scan_advance_n(scanner, 2, SKIP_WS);
    
    /* get attr name. */
    pj_scan_get(scanner, &cs_token, &attr->name);

    if (*scanner->curptr && *scanner->curptr != '\r' && 
        *scanner->curptr != '\n') 
    {
        /* skip ':' if present. */
        if (*scanner->curptr == ':')
            pj_scan_get_char(scanner);

        /* get value */
        if (!pj_scan_is_eof(scanner) && *scanner->curptr != '\r' && *scanner->curptr != '\n') {
            pj_scan_get_until_chr(scanner, "\r\n", &attr->value);
        } else {
            attr->value.ptr = NULL;
            attr->value.slen = 0;
        }

    } else {
        attr->value.ptr = NULL;
        attr->value.slen = 0;
    }

    /* We've got what we're looking for, skip anything until newline */
    pj_scan_skip_line(scanner);

    return attr;
}


/*
 * Apply direction attribute in session to all media.
 */
static void apply_media_direction(pjmedia_sdp_session *sdp)
{
    pjmedia_sdp_attr *dir_attr = NULL;
    unsigned i;

    const pj_str_t inactive = { "inactive", 8 };
    const pj_str_t sendonly = { "sendonly", 8 };
    const pj_str_t recvonly = { "recvonly", 8 };
    const pj_str_t sendrecv = { "sendrecv", 8 };

    /* Find direction attribute in session, don't need to find default 
     * direction "sendrecv".
     */
    for (i = 0; i < sdp->attr_count && !dir_attr; ++i) {
        if (!pj_strcmp(&sdp->attr[i]->name, &sendonly) ||
            !pj_strcmp(&sdp->attr[i]->name, &recvonly) ||
            !pj_strcmp(&sdp->attr[i]->name, &inactive)) 
        {
            dir_attr = sdp->attr[i];
        }
    }

    /* Found the direction attribute */
    if (dir_attr) {
        /* Remove the direction attribute in session */
        pjmedia_sdp_attr_remove(&sdp->attr_count, sdp->attr, dir_attr);

        /* Apply the direction attribute to all media, but not overriding it
         * if media already has direction attribute.
         */
        for (i = 0; i < sdp->media_count; ++i) {
            pjmedia_sdp_media *m;
            unsigned j;

            /* Find direction attribute in this media */
            m = sdp->media[i];
            for (j = 0; j < m->attr_count; ++j) {
                if (!pj_strcmp(&m->attr[j]->name, &sendrecv) ||
                    !pj_strcmp(&m->attr[j]->name, &sendonly) ||
                    !pj_strcmp(&m->attr[j]->name, &recvonly) ||
                    !pj_strcmp(&m->attr[j]->name, &inactive)) 
                {
                    break;
                }
            }

            /* Not found, apply direction attribute from session */
            if (j == m->attr_count)
                pjmedia_sdp_media_add_attr(m, dir_attr);
        }
    }
}


/*
 * Parse SDP message.
 */
PJ_DEF(pj_status_t) pjmedia_sdp_parse( pj_pool_t *pool,
                                       char *buf, pj_size_t len, 
                                       pjmedia_sdp_session **p_sdp)
{
    pj_scanner scanner;
    pjmedia_sdp_session *session;
    pjmedia_sdp_media *media = NULL;
    pjmedia_sdp_attr *attr;
    pjmedia_sdp_conn *conn;
    pjmedia_sdp_bandw *bandw;
    pj_str_t dummy;
    int cur_name = 254;
    volatile parse_context ctx;
    PJ_USE_EXCEPTION;

    ctx.last_error = PJ_SUCCESS;

    init_sdp_parser();

    pj_scan_init(&scanner, buf, len, 0, &on_scanner_error);
    session = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_session);
    PJ_ASSERT_RETURN(session != NULL, PJ_ENOMEM);

    /* Ignore leading newlines */
    while (*scanner.curptr=='\r' || *scanner.curptr=='\n')
        pj_scan_get_char(&scanner);

    PJ_TRY {
        while (!pj_scan_is_eof(&scanner)) {
                cur_name = *scanner.curptr;
                switch (cur_name) {
                case 'a':
                    attr = parse_attr(pool, &scanner, &ctx);
                    if (attr) {
                        if (media) {
                            if (media->attr_count < PJMEDIA_MAX_SDP_ATTR)
                                pjmedia_sdp_media_add_attr(media, attr);
                            else
                                PJ_PERROR(2, (THIS_FILE, PJ_ETOOMANY,
                                              "Error adding media attribute, "
                                              "attribute is ignored"));
                        } else {
                            if (session->attr_count < PJMEDIA_MAX_SDP_ATTR)
                                pjmedia_sdp_session_add_attr(session, attr);
                            else
                                PJ_PERROR(2, (THIS_FILE, PJ_ETOOMANY,
                                              "Error adding session attribute"
                                              ", attribute is ignored"));
                        }
                    }
                    break;
                case 'o':
                    parse_origin(&scanner, session, &ctx);
                    break;
                case 's':
                    parse_generic_line(&scanner, &session->name, &ctx);
                    break;
                case 'c':
                    conn = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_conn);
                    parse_connection_info(&scanner, conn, &ctx);
                    if (media) {
                        media->conn = conn;
                    } else {
                        session->conn = conn;
                    }
                    break;
                case 't':
                    parse_time(&scanner, session, &ctx);
                    break;
                case 'm':
                    media = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_media);
                    parse_media(&scanner, media, &ctx);
                    if (session->media_count < PJMEDIA_MAX_SDP_MEDIA)
                        session->media[ session->media_count++ ] = media;
                    else
                        PJ_PERROR(2,(THIS_FILE, PJ_ETOOMANY,
                                     "Error adding media, media is ignored"));
                    break;
                case 'v':
                    parse_version(&scanner, &ctx);
                    break;
                case 13:
                case 10:
                    pj_scan_get_char(&scanner);
                    /* Allow empty newlines at the end of the message */
                    while (!pj_scan_is_eof(&scanner)) {
                        if (*scanner.curptr != 13 && *scanner.curptr != 10) {
                            ctx.last_error = PJMEDIA_SDP_EINSDP;
                            on_scanner_error(&scanner);
                        }
                        pj_scan_get_char(&scanner);
                    }
                    break;
                case 'b':
                    bandw = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_bandw);
                    parse_bandwidth_info(&scanner, bandw, &ctx);
                    if (media) {
                        if (media->bandw_count < PJMEDIA_MAX_SDP_BANDW)
                            media->bandw[media->bandw_count++] = bandw;
                        else
                            PJ_PERROR(2, (THIS_FILE, PJ_ETOOMANY,
                                          "Error adding media bandwidth "
                                          "info, info is ignored"));
                    } else {
                        if (session->bandw_count < PJMEDIA_MAX_SDP_BANDW)
                            session->bandw[session->bandw_count++] = bandw;
                        else
                            PJ_PERROR(2, (THIS_FILE, PJ_ETOOMANY,
                                          "Error adding session bandwidth "
                                          "info, info is ignored"));
                    }
                    break;
                default:
                    if (cur_name >= 'a' && cur_name <= 'z')
                        parse_generic_line(&scanner, &dummy, &ctx);
                    else  {
                        ctx.last_error = PJMEDIA_SDP_EINSDP;
                        on_scanner_error(&scanner);
                    }
                    break;
                }
        }

        ctx.last_error = PJ_SUCCESS;

    }
    PJ_CATCH_ANY {              
        PJ_PERROR(4, (THIS_FILE, ctx.last_error,
                      "Error parsing SDP in line %d col %d",
                      scanner.line, pj_scan_get_col(&scanner)));

        session = NULL;

        // pj_assert(ctx.last_error != PJ_SUCCESS);
    }
    PJ_END;

    pj_scan_fini(&scanner);

    if (session)
        apply_media_direction(session);

    *p_sdp = session;
    return ctx.last_error;
}

/*
 * Print SDP description.
 */
PJ_DEF(int) pjmedia_sdp_print( const pjmedia_sdp_session *desc, 
                               char *buf, pj_size_t size)
{
    return print_session(desc, buf, size);
}


/*
 * Clone session
 */
PJ_DEF(pjmedia_sdp_session*) pjmedia_sdp_session_clone( pj_pool_t *pool,
                                                const pjmedia_sdp_session *rhs)
{
    pjmedia_sdp_session *sess;
    unsigned i;

    PJ_ASSERT_RETURN(pool && rhs, NULL);

    sess = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_session);
    PJ_ASSERT_RETURN(sess != NULL, NULL);

    /* Clone origin line. */
    pj_strdup(pool, &sess->origin.user, &rhs->origin.user);
    sess->origin.id = rhs->origin.id;
    sess->origin.version = rhs->origin.version;
    pj_strdup(pool, &sess->origin.net_type, &rhs->origin.net_type);
    pj_strdup(pool, &sess->origin.addr_type, &rhs->origin.addr_type);
    pj_strdup(pool, &sess->origin.addr, &rhs->origin.addr);

    /* Clone subject line. */
    pj_strdup(pool, &sess->name, &rhs->name);

    /* Clone connection line */
    if (rhs->conn) {
        sess->conn = pjmedia_sdp_conn_clone(pool, rhs->conn);
        PJ_ASSERT_RETURN(sess->conn != NULL, NULL);
    }

    /* Duplicate bandwidth info */
    sess->bandw_count = rhs->bandw_count;
    for (i=0; i<rhs->bandw_count; ++i) {
        sess->bandw[i] = pjmedia_sdp_bandw_clone(pool, rhs->bandw[i]);
    }

    /* Clone time line. */
    sess->time.start = rhs->time.start;
    sess->time.stop = rhs->time.stop;

    /* Duplicate session attributes. */
    sess->attr_count = rhs->attr_count;
    for (i=0; i<rhs->attr_count; ++i) {
        sess->attr[i] = pjmedia_sdp_attr_clone(pool, rhs->attr[i]);
    }

    /* Duplicate media descriptors. */
    sess->media_count = rhs->media_count;
    for (i=0; i<rhs->media_count; ++i) {
        sess->media[i] = pjmedia_sdp_media_clone(pool, rhs->media[i]);
    }

    return sess;
}


#define CHECK(exp,ret)  do {                    \
                            /*pj_assert(exp);*/ \
                            if (!(exp))         \
                                return ret;     \
                        } while (0)

/* Validate SDP connetion info. */
static pj_status_t validate_sdp_conn(const pjmedia_sdp_conn *c)
{
    CHECK( c, PJ_EINVAL);
    CHECK( pj_strcmp2(&c->net_type, "IN")==0, PJMEDIA_SDP_EINCONN);
    CHECK( pj_strcmp2(&c->addr_type, "IP4")==0 ||
           pj_strcmp2(&c->addr_type, "IP6")==0, 
           PJMEDIA_SDP_EINCONN);
    CHECK( c->addr.slen != 0, PJMEDIA_SDP_EINCONN);

    return PJ_SUCCESS;
}


/* Validate SDP session descriptor. */
PJ_DEF(pj_status_t) pjmedia_sdp_validate(const pjmedia_sdp_session *sdp)
{
    return pjmedia_sdp_validate2(sdp, PJ_TRUE);
}


/* Validate SDP session descriptor. */
PJ_DEF(pj_status_t) pjmedia_sdp_validate2(const pjmedia_sdp_session *sdp,
                                          pj_bool_t strict)
{
    unsigned i;
    const pj_str_t STR_RTPMAP = { "rtpmap", 6 };

    CHECK( sdp != NULL, PJ_EINVAL);

    /* Validate origin line. */
    CHECK( sdp->origin.user.slen != 0, PJMEDIA_SDP_EINORIGIN);
    CHECK( pj_strcmp2(&sdp->origin.net_type, "IN")==0, 
           PJMEDIA_SDP_EINORIGIN);
    CHECK( pj_strcmp2(&sdp->origin.addr_type, "IP4")==0 ||
           pj_strcmp2(&sdp->origin.addr_type, "IP6")==0, 
           PJMEDIA_SDP_EINORIGIN);
    CHECK( sdp->origin.addr.slen != 0, PJMEDIA_SDP_EINORIGIN);

    /* Validate subject line. */
    CHECK( sdp->name.slen != 0, PJMEDIA_SDP_EINNAME);

    /* Ignore start and stop time. */

    /* If session level connection info is present, validate it. */
    if (sdp->conn) {
        pj_status_t status = validate_sdp_conn(sdp->conn);
        if (status != PJ_SUCCESS)
            return status;
    }

    /* Validate each media. */
    for (i=0; i<sdp->media_count; ++i) {
        const pjmedia_sdp_media *m = sdp->media[i];
        unsigned j;

        /* Validate the m= line. */
        CHECK( m->desc.media.slen != 0, PJMEDIA_SDP_EINMEDIA);
        CHECK( m->desc.transport.slen != 0, PJMEDIA_SDP_EINMEDIA);
        CHECK( m->desc.fmt_count != 0 || m->desc.port==0, PJMEDIA_SDP_ENOFMT);

        /* If media level connection info is present, validate it. */
        if (m->conn) {
            pj_status_t status = validate_sdp_conn(m->conn);
            if (status != PJ_SUCCESS)
                return status;
        }

        /* If media doesn't have connection info, then connection info
         * must be present in the session.
         */
        if (m->conn == NULL) {
            if (sdp->conn == NULL)
                if (strict || m->desc.port != 0)
                    return PJMEDIA_SDP_EMISSINGCONN;
        }

        /* Verify payload type. */
        for (j=0; j<m->desc.fmt_count; ++j) {

            /* Arrgh noo!! Payload type can be non-numeric!!
             * RTC based programs sends "null" for instant messaging!
             */
            if (pj_isdigit(*m->desc.fmt[j].ptr)) {
                unsigned long pt;
                pj_status_t status = pj_strtoul3(&m->desc.fmt[j], &pt, 10);

                /* Payload type is between 0 and 127. 
                 */
                CHECK( status == PJ_SUCCESS && pt <= 127, PJMEDIA_SDP_EINPT);

                /* If port is not zero, then for each dynamic payload type, an
                 * rtpmap attribute must be specified.
                 */
                if (m->desc.port != 0 && pt >= 96) {
                    const pjmedia_sdp_attr *a;

                    a = pjmedia_sdp_media_find_attr(m, &STR_RTPMAP, 
                                                    &m->desc.fmt[j]);
                    CHECK( a != NULL, PJMEDIA_SDP_EMISSINGRTPMAP);
                }
            }
        }
    }

    /* Looks good. */
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_sdp_transport_cmp( const pj_str_t *t1,
                                               const pj_str_t *t2)
{
    pj_uint32_t t1_proto, t2_proto;

    /* Exactly equal? */
    if (pj_stricmp(t1, t2) == 0)
        return PJ_SUCCESS;

    /* Check if boths are RTP/AVP based */
    t1_proto = pjmedia_sdp_transport_get_proto(t1);
    t2_proto = pjmedia_sdp_transport_get_proto(t2);
    if (PJMEDIA_TP_PROTO_HAS_FLAG(t1_proto, PJMEDIA_TP_PROTO_RTP_AVP) && 
        PJMEDIA_TP_PROTO_HAS_FLAG(t2_proto, PJMEDIA_TP_PROTO_RTP_AVP))
    {
        return PJ_SUCCESS;
    }

    /* Compatible? */
    //{
    //  static const pj_str_t ID_RTP_AVP  = { "RTP/AVP", 7 };
    //  static const pj_str_t ID_RTP_SAVP = { "RTP/SAVP", 8 };
    //  if ((!pj_stricmp(t1, &ID_RTP_AVP) || !pj_stricmp(t1, &ID_RTP_SAVP)) &&
    //      (!pj_stricmp(t2, &ID_RTP_AVP) || !pj_stricmp(t2, &ID_RTP_SAVP)))
    //      return PJ_SUCCESS;
    //}

    return PJMEDIA_SDP_ETPORTNOTEQUAL;
}


/*
 * Get media transport info, e.g: protocol and profile.
 */
PJ_DEF(pj_uint32_t) pjmedia_sdp_transport_get_proto(const pj_str_t *tp)
{
    pj_str_t token, rest = {0};
    pj_ssize_t idx;

    PJ_ASSERT_RETURN(tp, PJMEDIA_TP_PROTO_NONE);

    idx = pj_strtok2(tp, "/", &token, 0);
    if (idx != tp->slen)
        pj_strset(&rest, tp->ptr + token.slen + 1, tp->slen - token.slen - 1);

    if (pj_stricmp2(&token, "RTP") == 0) {
        /* Starts with "RTP" */

        /* RTP/AVP */
        if (pj_stricmp2(&rest, "AVP") == 0)
            return PJMEDIA_TP_PROTO_RTP_AVP;

        /* RTP/SAVP */
        if (pj_stricmp2(&rest, "SAVP") == 0)
            return PJMEDIA_TP_PROTO_RTP_SAVP;

        /* RTP/AVPF */
        if (pj_stricmp2(&rest, "AVPF") == 0)
            return PJMEDIA_TP_PROTO_RTP_AVPF;

        /* RTP/SAVPF */
        if (pj_stricmp2(&rest, "SAVPF") == 0)
            return PJMEDIA_TP_PROTO_RTP_SAVPF;

    } else if (pj_stricmp2(&token, "UDP") == 0) {
        /* Starts with "UDP" */

        /* Plain UDP */
        if (rest.slen == 0)
            return PJMEDIA_TP_PROTO_UDP;

        /* DTLS-SRTP */
        if (pj_stricmp2(&rest, "TLS/RTP/SAVP") == 0)
            return PJMEDIA_TP_PROTO_DTLS_SRTP;

        /* DTLS-SRTP with RTCP-FB */
        if (pj_stricmp2(&rest, "TLS/RTP/SAVPF") == 0)
            return PJMEDIA_TP_PROTO_DTLS_SRTPF;
    }

    /* Unknown transport */
    return PJMEDIA_TP_PROTO_UNKNOWN;
}


PJ_DEF(pj_status_t) pjmedia_sdp_media_deactivate(pj_pool_t *pool,
                                                 pjmedia_sdp_media *m)
{
    PJ_ASSERT_RETURN(m, PJ_EINVAL);
    PJ_UNUSED_ARG(pool);

    /* Set port to zero */
    m->desc.port = 0;

    /* And remove attributes */
    m->attr_count = 0;

    return PJ_SUCCESS;
}


PJ_DEF(pjmedia_sdp_media*) pjmedia_sdp_media_clone_deactivate(
                                                pj_pool_t *pool,
                                                const pjmedia_sdp_media *rhs)
{
    unsigned int i;
    pjmedia_sdp_media *m;

    PJ_ASSERT_RETURN(pool && rhs, NULL);

    m = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_media);
    pj_memcpy(m, rhs, sizeof(*m));

    /* Clone the media line only */
    pj_strdup (pool, &m->desc.media, &rhs->desc.media);
    pj_strdup (pool, &m->desc.transport, &rhs->desc.transport);
    for (i=0; i<rhs->desc.fmt_count; ++i)
        pj_strdup(pool, &m->desc.fmt[i], &rhs->desc.fmt[i]);

    if (rhs->conn) {
        m->conn = pjmedia_sdp_conn_clone (pool, rhs->conn);
        PJ_ASSERT_RETURN(m->conn != NULL, NULL);
    }

    m->bandw_count = rhs->bandw_count;
    for (i=0; i < rhs->bandw_count; ++i) {
        m->bandw[i] = pjmedia_sdp_bandw_clone (pool, rhs->bandw[i]);
        PJ_ASSERT_RETURN(m->bandw[i] != NULL, NULL);
    }

    /* And deactivate it */
    pjmedia_sdp_media_deactivate(pool, m);

    return m;
}
