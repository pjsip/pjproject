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
#define TOKEN		"-.!%*_=`'~"
#define NTP_OFFSET	((pj_uint32_t)2208988800)
#define THIS_FILE	"sdp.c"

typedef struct parse_context
{ 
    pj_status_t last_error;
} parse_context;


/*
 * Prototypes for line parser.
 */
static void parse_version(pj_scanner *scanner, parse_context *ctx);
static void parse_origin(pj_scanner *scanner, pjmedia_sdp_session *ses,
			 parse_context *ctx);
static void parse_time(pj_scanner *scanner, pjmedia_sdp_session *ses,
		       parse_context *ctx);
static void parse_generic_line(pj_scanner *scanner, pj_str_t *str,
			       parse_context *ctx);
static void parse_connection_info(pj_scanner *scanner, pjmedia_sdp_conn *conn,
				  parse_context *ctx);
static pjmedia_sdp_attr *parse_attr(pj_pool_t *pool, pj_scanner *scanner,
				    parse_context *ctx);
static void parse_media(pj_scanner *scanner, pjmedia_sdp_media *med,
			parse_context *ctx);


/*
 * Scanner character specification.
 */
static int is_initialized;
static pj_cis_buf_t cis_buf;
static pj_cis_t cs_token;

static void init_sdp_parser(void)
{
    if (is_initialized == 0) {
	is_initialized = 1;
	if (is_initialized != 1) {
	    return;
	}
    }

    pj_cis_buf_init(&cis_buf);
    pj_cis_init(&cis_buf, &cs_token);
    pj_cis_add_alpha(&cs_token);
    pj_cis_add_num(&cs_token);
    pj_cis_add_str(&cs_token, TOKEN);
}

PJ_DEF(pjmedia_sdp_attr*) pjmedia_sdp_attr_create( pj_pool_t *pool,
						   const char *name,
						   const pj_str_t *value)
{
    pjmedia_sdp_attr *attr;

    PJ_ASSERT_RETURN(pool && name, NULL);

    attr = pj_pool_alloc(pool, sizeof(pjmedia_sdp_attr));
    pj_strdup2(pool, &attr->name, name);

    if (value)
	pj_strdup(pool, &attr->value, value);
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

    attr = pj_pool_alloc(pool, sizeof(pjmedia_sdp_attr));

    pj_strdup(pool, &attr->name, &rhs->name);
    pj_strdup(pool, &attr->value, &rhs->value);

    return attr;
}

PJ_DEF(pjmedia_sdp_attr*) 
pjmedia_sdp_attr_find (unsigned count, 
		       const pjmedia_sdp_attr *const attr_array[],
		       const pj_str_t *name,
		       const pj_str_t *c_fmt)
{
    char fmtbuf[16];
    pj_str_t fmt = { NULL, 0};
    unsigned i;

    if (c_fmt) {
	/* To search the format, we prepend the string with a colon and
	 * append space
	 */
	PJ_ASSERT_RETURN(c_fmt->slen<sizeof(fmtbuf)-2, NULL);
	fmt.ptr = fmtbuf;
	fmt.slen = c_fmt->slen + 2;
	fmtbuf[0] = ':';
	pj_memcpy(fmt.ptr+1, c_fmt->ptr, c_fmt->slen);
	fmtbuf[c_fmt->slen+1] = ' ';

    } 

    for (i=0; i<count; ++i) {
	if (pj_strcmp(&attr_array[i]->name, name) == 0) {
	    const pjmedia_sdp_attr *a = attr_array[i];
	    if (c_fmt) {
		if (a->value.slen > fmt.slen &&
		    pj_strncmp(&a->value, &fmt, fmt.slen)==0)
		{
		    return (pjmedia_sdp_attr*)a;
		}
	    } else 
		return (pjmedia_sdp_attr*)a;
	}
    }
    return NULL;
}

PJ_DEF(pjmedia_sdp_attr*) 
pjmedia_sdp_attr_find2(unsigned count, 
		       const pjmedia_sdp_attr *const attr_array[],
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
    PJ_ASSERT_RETURN(*count < PJMEDIA_MAX_SDP_ATTR, PJ_ETOOMANY);

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
    const char *p = attr->value.ptr;
    const char *end = attr->value.ptr + attr->value.slen;
    pj_str_t token;

    PJ_ASSERT_RETURN(pj_strcmp2(&attr->name, "rtpmap")==0, PJ_EINVALIDOP);

    /* rtpmap sample:
     *	a=rtpmap:98 L16/16000/2.
     */

    /* Eat the first ':' */
    if (*p != ':') return PJMEDIA_SDP_EINRTPMAP;

    /* Get ':' */
    ++p;

    /* Get payload type. */
    token.ptr = (char*)p;
    while (pj_isdigit(*p) && p!=end)
	++p;
    token.slen = p - token.ptr;
    if (token.slen == 0)
	return PJMEDIA_SDP_EINRTPMAP;

    rtpmap->pt = token;

    /* Expecting space after payload type. */
    if (*p != ' ') return PJMEDIA_SDP_EINRTPMAP;

    /* Get space. */
    ++p;

    /* Get encoding name. */
    token.ptr = (char*)p;
    while (*p != '/' && p != end)
	++p;
    token.slen = p - token.ptr;
    if (token.slen == 0)
	return PJMEDIA_SDP_EINRTPMAP;
    rtpmap->enc_name = token;

    /* Expecting '/' after encoding name. */
    if (*p != '/') return PJMEDIA_SDP_EINRTPMAP;

    /* Get '/' */
    ++p;

    /* Get the clock rate. */
    token.ptr = (char*)p;
    while (pj_isdigit(*p) && p != end)
	++p;
    token.slen = p - token.ptr;
    if (token.slen == 0)
	return PJMEDIA_SDP_EINRTPMAP;

    rtpmap->clock_rate = pj_strtoul(&token);

    /* Expecting either '/' or EOF */
    if (*p != '/' && p != end)
	return PJMEDIA_SDP_EINRTPMAP;

    if (*p == '/') {
	++p;
	token.ptr = (char*)p;
	token.slen = end-p;
	rtpmap->param = token;
    } else {
	rtpmap->param.ptr = NULL;
	rtpmap->param.slen = 0;
    }

    return PJ_SUCCESS;

}

PJ_DEF(pj_status_t) pjmedia_sdp_attr_get_fmtp( const pjmedia_sdp_attr *attr,
					       pjmedia_sdp_fmtp *fmtp)
{
    const char *p = attr->value.ptr;
    const char *end = attr->value.ptr + attr->value.slen;
    pj_str_t token;

    PJ_ASSERT_RETURN(pj_strcmp2(&attr->name, "fmtp")==0, PJ_EINVALIDOP);

    /* fmtp BNF:
     *	a=fmtp:<format> <format specific parameter>
     */

    /* Eat the first ':' */
    if (*p != ':') return PJMEDIA_SDP_EINFMTP;

    /* Get ':' */
    ++p;

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

PJ_DEF(pj_status_t) pjmedia_sdp_attr_to_rtpmap(pj_pool_t *pool,
					       const pjmedia_sdp_attr *attr,
					       pjmedia_sdp_rtpmap **p_rtpmap)
{
    PJ_ASSERT_RETURN(pool && attr && p_rtpmap, PJ_EINVAL);

    *p_rtpmap = pj_pool_alloc(pool, sizeof(pjmedia_sdp_rtpmap));
    PJ_ASSERT_RETURN(*p_rtpmap, PJ_ENOMEM);

    return pjmedia_sdp_attr_get_rtpmap(attr, *p_rtpmap);
}


PJ_DEF(pj_status_t) pjmedia_sdp_rtpmap_to_attr(pj_pool_t *pool,
					       const pjmedia_sdp_rtpmap *rtpmap,
					       pjmedia_sdp_attr **p_attr)
{
    pjmedia_sdp_attr *attr;
    char tempbuf[64], *p, *endbuf;
    int i;

    /* Check arguments. */
    PJ_ASSERT_RETURN(pool && rtpmap && p_attr, PJ_EINVAL);

    /* Check that mandatory attributes are specified. */
    PJ_ASSERT_RETURN(rtpmap->enc_name.slen && rtpmap->clock_rate,
		     PJMEDIA_SDP_EINRTPMAP);

    /* Check size. */
    i = rtpmap->enc_name.slen + rtpmap->param.slen + 32;
    if (i >= sizeof(tempbuf)-1) {
	pj_assert(!"rtpmap attribute is too long");
	return PJMEDIA_SDP_ERTPMAPTOOLONG;
    }

    attr = pj_pool_alloc(pool, sizeof(pjmedia_sdp_attr));
    PJ_ASSERT_RETURN(attr != NULL, PJ_ENOMEM);

    attr->name.ptr = "rtpmap";
    attr->name.slen = 6;

    p = tempbuf;
    endbuf = tempbuf+sizeof(tempbuf);

    /* Add colon */
    *p++ = ':';

    /* Add payload type. */
    pj_memcpy(p, rtpmap->pt.ptr, rtpmap->pt.slen);
    p += rtpmap->pt.slen;
    *p++ = ' ';

    /* Add encoding name. */
    for (i=0; i<rtpmap->enc_name.slen; ++i)
	p[i] = rtpmap->enc_name.ptr[i];
    p += rtpmap->enc_name.slen;
    *p++ = '/';

    /* Add clock rate. */
    p += pj_utoa(rtpmap->clock_rate, p);

    /* Add parameter if necessary. */
    if (rtpmap->param.slen > 0) {
	*p++ = '/';
	for (i=0; i<rtpmap->param.slen; ++i)
	    p[i] = rtpmap->param.ptr[i];
	p += rtpmap->param.slen;
    }

    *p = '\0';

    attr->value.slen = p-tempbuf;
    attr->value.ptr = pj_pool_alloc(pool, attr->value.slen);
    pj_memcpy(attr->value.ptr, tempbuf, attr->value.slen);

    *p_attr = attr;
    return PJ_SUCCESS;
}


static int print_connection_info( pjmedia_sdp_conn *c, char *buf, int len)
{
    char *p = buf;

    if (len < 8+c->net_type.slen+c->addr_type.slen+c->addr.slen) {
	return -1;
    }
    *p++ = 'c';
    *p++ = '=';
    pj_memcpy(p, c->net_type.ptr, c->net_type.slen);
    p += c->net_type.slen;
    *p++ = ' ';
    pj_memcpy(p, c->addr_type.ptr, c->addr_type.slen);
    p += c->addr_type.slen;
    *p++ = ' ';
    pj_memcpy(p, c->addr.ptr, c->addr.slen);
    p += c->addr.slen;
    *p++ = '\r';
    *p++ = '\n';

    return p-buf;
}

PJ_DEF(pjmedia_sdp_conn*) pjmedia_sdp_conn_clone (pj_pool_t *pool, 
						  const pjmedia_sdp_conn *rhs)
{
    pjmedia_sdp_conn *c = pj_pool_alloc (pool, sizeof(pjmedia_sdp_conn));
    if (!c) return NULL;

    if (!pj_strdup (pool, &c->net_type, &rhs->net_type)) return NULL;
    if (!pj_strdup (pool, &c->addr_type, &rhs->addr_type)) return NULL;
    if (!pj_strdup (pool, &c->addr, &rhs->addr)) return NULL;

    return c;
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
	pj_memcpy(p, attr->value.ptr, attr->value.slen);
	p += attr->value.slen;
    }

    *p++ = '\r';
    *p++ = '\n';
    return p-buf;
}

static int print_media_desc( pjmedia_sdp_media *m, char *buf, int len)
{
    char *p = buf;
    char *end = buf+len;
    unsigned i;
    int printed;

    /* check length for the "m=" line. */
    if (len < m->desc.media.slen+m->desc.transport.slen+12+24) {
	return -1;
    }
    *p++ = 'm';	    /* m= */
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
	*p++ = ' ';
	pj_memcpy(p, m->desc.fmt[i].ptr, m->desc.fmt[i].slen);
	p += m->desc.fmt[i].slen;
    }
    *p++ = '\r';
    *p++ = '\n';

    /* print connection info, if present. */
    if (m->conn) {
	printed = print_connection_info(m->conn, p, end-p);
	if (printed < 0) {
	    return -1;
	}
	p += printed;
    }

    /* print attributes. */
    for (i=0; i<m->attr_count; ++i) {
	printed = print_attr(m->attr[i], p, end-p);
	if (printed < 0) {
	    return -1;
	}
	p += printed;
    }

    return p-buf;
}

PJ_DEF(pjmedia_sdp_media*) pjmedia_sdp_media_clone(
						 pj_pool_t *pool, 
						 const pjmedia_sdp_media *rhs)
{
    unsigned int i;
    pjmedia_sdp_media *m = pj_pool_alloc (pool, sizeof(pjmedia_sdp_media));
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

    m->attr_count = rhs->attr_count;
    for (i=0; i < rhs->attr_count; ++i) {
	m->attr[i] = pjmedia_sdp_attr_clone (pool, rhs->attr[i]);
	PJ_ASSERT_RETURN(m->attr[i] != NULL, NULL);
    }

    return m;
}

PJ_DEF(pjmedia_sdp_attr*) 
pjmedia_sdp_media_find_attr(const pjmedia_sdp_media *m,
			    const pj_str_t *name, const pj_str_t *fmt)
{
    PJ_ASSERT_RETURN(m && name, NULL);
    return pjmedia_sdp_attr_find(m->attr_count, m->attr, name, fmt);
}



PJ_DEF(pjmedia_sdp_attr*) 
pjmedia_sdp_media_find_attr2(const pjmedia_sdp_media *m,
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

PJ_DEF(unsigned) 
pjmedia_sdp_media_remove_all_attr(pjmedia_sdp_media *m,
				  const char *name)
{
    return pjmedia_sdp_attr_remove_all(&m->attr_count, m->attr, name);
}

PJ_DEF(pj_status_t)
pjmedia_sdp_media_remove_attr(pjmedia_sdp_media *m,
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
    printed = pj_utoa(ses->origin.id, p);
    p += printed;
    *p++ = ' ';
    printed = pj_utoa(ses->origin.version, p);
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
	printed = print_connection_info(ses->conn, p, end-p);
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
    printed = pj_utoa(ses->time.start, p);
    p += printed;
    *p++ = ' ';
    printed = pj_utoa(ses->time.stop, p);
    p += printed;
    *p++ = '\r';
    *p++ = '\n';

    /* Print all attribute (a=) lines. */
    for (i=0; i<ses->attr_count; ++i) {
	printed = print_attr(ses->attr[i], p, end-p);
	if (printed < 0) {
	    return -1;
	}
	p += printed;
    }

    /* Print media (m=) lines. */
    for (i=0; i<ses->media_count; ++i) {
	printed = print_media_desc(ses->media[i], p, end-p);
	if (printed < 0) {
	    return -1;
	}
	p += printed;
    }

    return p-buf;
}

/******************************************************************************
 * PARSERS
 */

static void parse_version(pj_scanner *scanner, parse_context *ctx)
{
    ctx->last_error = PJMEDIA_SDP_EINVER;

    pj_scan_advance_n(scanner, 3, SKIP_WS);
    pj_scan_get_newline(scanner);
}

static void parse_origin(pj_scanner *scanner, pjmedia_sdp_session *ses,
			 parse_context *ctx)
{
    pj_str_t str;

    ctx->last_error = PJMEDIA_SDP_EINORIGIN;

    /* o= */
    pj_scan_advance_n(scanner, 2, SKIP_WS);

    /* username. */
    pj_scan_get_until_ch(scanner, ' ', &ses->origin.user);
    pj_scan_get_char(scanner);

    /* id */
    pj_scan_get_until_ch(scanner, ' ', &str);
    ses->origin.id = pj_strtoul(&str);
    pj_scan_get_char(scanner);

    /* version */
    pj_scan_get_until_ch(scanner, ' ', &str);
    ses->origin.version = pj_strtoul(&str);
    pj_scan_get_char(scanner);

    /* network-type */
    pj_scan_get_until_ch(scanner, ' ', &ses->origin.net_type);
    pj_scan_get_char(scanner);

    /* addr-type */
    pj_scan_get_until_ch(scanner, ' ', &ses->origin.addr_type);
    pj_scan_get_char(scanner);

    /* address */
    pj_scan_get_until_ch(scanner, '\r', &ses->origin.addr);

    /* newline */
    pj_scan_get_newline(scanner);
}

static void parse_time(pj_scanner *scanner, pjmedia_sdp_session *ses,
		       parse_context *ctx)
{
    pj_str_t str;

    ctx->last_error = PJMEDIA_SDP_EINTIME;

    /* t= */
    pj_scan_advance_n(scanner, 2, SKIP_WS);

    /* start time */
    pj_scan_get_until_ch(scanner, ' ', &str);
    ses->time.start = pj_strtoul(&str);

    pj_scan_get_char(scanner);

    /* stop time */
    pj_scan_get_until_ch(scanner, '\r', &str);
    ses->time.stop = pj_strtoul(&str);

    /* newline */
    pj_scan_get_newline(scanner);
}

static void parse_generic_line(pj_scanner *scanner, pj_str_t *str,
			       parse_context *ctx)
{
    ctx->last_error = PJMEDIA_SDP_EINSDP;

    /* x= */
    pj_scan_advance_n(scanner, 2, SKIP_WS);

    /* get anything until newline. */
    pj_scan_get_until_ch(scanner, '\r', str);

    /* newline. */
    pj_scan_get_newline(scanner);
}

static void parse_connection_info(pj_scanner *scanner, pjmedia_sdp_conn *conn,
				  parse_context *ctx)
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
    pj_scan_get_until_ch(scanner, '\r', &conn->addr);

    /* newline */
    pj_scan_get_newline(scanner);
}

static void parse_media(pj_scanner *scanner, pjmedia_sdp_media *med,
			parse_context *ctx)
{
    pj_str_t str;

    ctx->last_error = PJMEDIA_SDP_EINMEDIA;

    /* m= */
    pj_scan_advance_n(scanner, 2, SKIP_WS);

    /* type */
    pj_scan_get_until_ch(scanner, ' ', &med->desc.media);
    pj_scan_get_char(scanner);

    /* port */
    pj_scan_get(scanner, &cs_token, &str);
    med->desc.port = (unsigned short)pj_strtoul(&str);
    if (*scanner->curptr == '/') {
	/* port count */
	pj_scan_get_char(scanner);
	pj_scan_get(scanner, &cs_token, &str);
	med->desc.port_count = pj_strtoul(&str);

    } else {
	med->desc.port_count = 0;
    }

    if (pj_scan_get_char(scanner) != ' ') {
	PJ_THROW(SYNTAX_ERROR);
    }

    /* transport */
    pj_scan_get_until_ch(scanner, ' ', &med->desc.transport);

    /* format list */
    med->desc.fmt_count = 0;
    while (*scanner->curptr == ' ') {
	pj_scan_get_char(scanner);
	pj_scan_get(scanner, &cs_token, &med->desc.fmt[med->desc.fmt_count++]);
    }

    /* newline */
    pj_scan_get_newline(scanner);
}

static void on_scanner_error(pj_scanner *scanner)
{
    PJ_UNUSED_ARG(scanner);

    PJ_THROW(SYNTAX_ERROR);
}

static pjmedia_sdp_attr *parse_attr( pj_pool_t *pool, pj_scanner *scanner,
				    parse_context *ctx)
{
    pjmedia_sdp_attr *attr;

    ctx->last_error = PJMEDIA_SDP_EINATTR;

    attr = pj_pool_alloc(pool, sizeof(pjmedia_sdp_attr));

    /* skip a= */
    pj_scan_advance_n(scanner, 2, SKIP_WS);
    
    /* get attr name. */
    pj_scan_get(scanner, &cs_token, &attr->name);

    if (*scanner->curptr != '\r' && *scanner->curptr != '\n') {
	/* get value */
	pj_scan_get_until_ch(scanner, '\r', &attr->value);
    } else {
	attr->value.ptr = NULL;
	attr->value.slen = 0;
    }

    /* newline */
    pj_scan_get_newline(scanner);

    return attr;
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
    void *attr;
    pjmedia_sdp_conn *conn;
    pj_str_t dummy;
    int cur_name = 254;
    parse_context ctx;
    PJ_USE_EXCEPTION;

    ctx.last_error = PJ_SUCCESS;

    init_sdp_parser();

    pj_scan_init(&scanner, buf, len, 0, &on_scanner_error);
    session = pj_pool_calloc(pool, 1, sizeof(pjmedia_sdp_session));
    PJ_ASSERT_RETURN(session != NULL, PJ_ENOMEM);

    PJ_TRY {
	while (!pj_scan_is_eof(&scanner)) {
		cur_name = *scanner.curptr;
		switch (cur_name) {
		case 'a':
		    attr = parse_attr(pool, &scanner, &ctx);
		    if (attr) {
			if (media) {
			    media->attr[media->attr_count++] = attr;
			} else {
			    session->attr[session->attr_count++] = attr;
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
		    conn = pj_pool_calloc(pool, 1, sizeof(*conn));
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
		    media = pj_pool_calloc(pool, 1, sizeof(*media));
		    parse_media(&scanner, media, &ctx);
		    session->media[ session->media_count++ ] = media;
		    break;
		case 'v':
		    parse_version(&scanner, &ctx);
		    break;
		default:
		    parse_generic_line(&scanner, &dummy, &ctx);
		    break;
		}
	}

	ctx.last_error = PJ_SUCCESS;

    }
    PJ_CATCH(SYNTAX_ERROR) {
	
	char errmsg[PJ_ERR_MSG_SIZE];
	pj_strerror(ctx.last_error, errmsg, sizeof(errmsg));

	PJ_LOG(4, (THIS_FILE, "Error parsing SDP in line %d col %d: %s",
		   scanner.line, pj_scan_get_col(&scanner),
		   errmsg));

	session = NULL;

    }
    PJ_END;

    pj_scan_fini(&scanner);

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
PJ_DEF(pjmedia_sdp_session*) 
pjmedia_sdp_session_clone( pj_pool_t *pool,
			   const pjmedia_sdp_session *rhs)
{
    pjmedia_sdp_session *sess;
    unsigned i;

    PJ_ASSERT_RETURN(pool && rhs, NULL);

    sess = pj_pool_zalloc(pool, sizeof(pjmedia_sdp_session));
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


#define CHECK(exp,ret)	do {			\
			    pj_assert(exp);	\
			    if (!(exp))		\
				return ret;	\
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
	CHECK( m->desc.fmt_count != 0, PJMEDIA_SDP_ENOFMT);

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
		return PJMEDIA_SDP_EMISSINGCONN;
	}

	/* Verify payload type. */
	for (j=0; j<m->desc.fmt_count; ++j) {

	    /* Arrgh noo!! Payload type can be non-numeric!!
	     * RTC based programs sends "null" for instant messaging!
	     */
	    if (pj_isdigit(*m->desc.fmt[j].ptr)) {
		unsigned pt = pj_strtoul(&m->desc.fmt[j]);

		/* Payload type is between 0 and 127. 
		 */
		CHECK( pt <= 127, PJMEDIA_SDP_EINPT);

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


