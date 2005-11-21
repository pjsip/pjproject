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
#include <pj/scanner.h>
#include <pj/except.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/string.h>
#include <pj/pool.h>

enum {
    SKIP_WS = 0,
    SYNTAX_ERROR = 1,
};
#define TOKEN		"-.!%*_=`'~"
#define NTP_OFFSET	((pj_uint32_t)2208988800)
#define LOG_THIS	"sdp"

/*
 * Prototypes for line parser.
 */
static void parse_version(pj_scanner *scanner);
static void parse_origin(pj_scanner *scanner, pjsdp_session_desc *ses);
static void parse_time(pj_scanner *scanner, pjsdp_session_desc *ses);
static void parse_generic_line(pj_scanner *scanner, pj_str_t *str);
static void parse_connection_info(pj_scanner *scanner, pjsdp_conn_info *conn);
static pjsdp_attr *parse_attr(pj_pool_t *pool, pj_scanner *scanner);
static void parse_media(pj_scanner *scanner, pjsdp_media_desc *med);

/*
 * Prototypes for attribute parsers.
 */
static pjsdp_rtpmap_attr *  parse_rtpmap_attr( pj_pool_t *pool, pj_scanner *scanner );
static pjsdp_attr_string *  parse_generic_string_attr( pj_pool_t *pool, pj_scanner *scanner );
static pjsdp_attr_num *	    parse_generic_num_attr( pj_pool_t *pool, pj_scanner *scanner );
static pjsdp_attr *	    parse_name_only_attr( pj_pool_t *pool, pj_scanner *scanner );
static pjsdp_fmtp_attr *    parse_fmtp_attr( pj_pool_t *pool, pj_scanner *scanner );


/* 
 * Prototypes for functions to print attribute.
 * All of them returns integer for the length printed, or -1 on error.
 */
static int print_rtpmap_attr(const pjsdp_rtpmap_attr *attr, 
			     char *buf, int length);
static int print_generic_string_attr(const pjsdp_attr_string *attr, 
				     char *buf, int length);
static int print_generic_num_attr(const pjsdp_attr_num *attr, 
				  char *buf, int length);
static int print_name_only_attr(const pjsdp_attr *attr, 
				char *buf, int length);
static int print_fmtp_attr(const pjsdp_fmtp_attr *attr, 
			   char *buf, int length);

/*
 * Prototypes for cloning attributes.
 */
static pjsdp_attr* clone_rtpmap_attr (pj_pool_t *pool, const pjsdp_attr *rhs);
static pjsdp_attr* clone_generic_string_attr (pj_pool_t *pool, const pjsdp_attr *rhs);
static pjsdp_attr* clone_generic_num_attr (pj_pool_t *pool, const pjsdp_attr *rhs);
static pjsdp_attr* clone_name_only_attr (pj_pool_t *pool, const pjsdp_attr *rhs);
static pjsdp_attr* clone_fmtp_attr (pj_pool_t *pool, const pjsdp_attr *rhs);


/*
 * Prototypes
 */
static void init_sdp_parser(void);


typedef void *  (*FPARSE)(pj_pool_t *pool, pj_scanner *scanner);
typedef int (*FPRINT)(const void *attr, char *buf, int length);
typedef pjsdp_attr*  (*FCLONE)(pj_pool_t *pool, const pjsdp_attr *rhs);

/*
 * Array of functions to print attribute.
 */
static struct attr_map_rec
{
    pj_str_t name;
    FPARSE   parse_attr;
    FPRINT   print_attr;
    FCLONE   clone;
} attr_map[] = 
{
    {{"rtpmap", 6},    (FPARSE)&parse_rtpmap_attr,	   (FPRINT)&print_rtpmap_attr,		(FCLONE)&clone_rtpmap_attr},
    {{"cat", 3},       (FPARSE)&parse_generic_string_attr, (FPRINT)&print_generic_string_attr,	(FCLONE)&clone_generic_string_attr},
    {{"keywds", 6},    (FPARSE)&parse_generic_string_attr, (FPRINT)&print_generic_string_attr,	(FCLONE)&clone_generic_string_attr},
    {{"tool", 4},      (FPARSE)&parse_generic_string_attr, (FPRINT)&print_generic_string_attr,	(FCLONE)&clone_generic_string_attr},
    {{"ptime", 5},     (FPARSE)&parse_generic_num_attr,    (FPRINT)&print_generic_num_attr,	(FCLONE)&clone_generic_num_attr},
    {{"recvonly", 8},  (FPARSE)&parse_name_only_attr,	   (FPRINT)&print_name_only_attr,	(FCLONE)&clone_name_only_attr},
    {{"sendonly", 8},  (FPARSE)&parse_name_only_attr,	   (FPRINT)&print_name_only_attr,	(FCLONE)&clone_name_only_attr},
    {{"sendrecv", 8},  (FPARSE)&parse_name_only_attr,	   (FPRINT)&print_name_only_attr,	(FCLONE)&clone_name_only_attr},
    {{"orient", 6},    (FPARSE)&parse_generic_string_attr, (FPRINT)&print_generic_string_attr,	(FCLONE)&clone_generic_string_attr},
    {{"type", 4},      (FPARSE)&parse_generic_string_attr, (FPRINT)&print_generic_string_attr,	(FCLONE)&clone_generic_string_attr},
    {{"charset", 7},   (FPARSE)&parse_generic_string_attr, (FPRINT)&print_generic_string_attr,	(FCLONE)&clone_generic_string_attr},
    {{"sdplang", 7},   (FPARSE)&parse_generic_string_attr, (FPRINT)&print_generic_string_attr,	(FCLONE)&clone_generic_string_attr},
    {{"lang", 4},      (FPARSE)&parse_generic_string_attr, (FPRINT)&print_generic_string_attr,	(FCLONE)&clone_generic_string_attr},
    {{"framerate", 9}, (FPARSE)&parse_generic_string_attr, (FPRINT)&print_generic_string_attr,	(FCLONE)&clone_generic_string_attr},
    {{"quality", 7},   (FPARSE)&parse_generic_num_attr,    (FPRINT)&print_generic_num_attr,	(FCLONE)&clone_generic_num_attr},
    {{"fmtp", 4},      (FPARSE)&parse_fmtp_attr,	   (FPRINT)&print_fmtp_attr,		(FCLONE)&clone_fmtp_attr},
    {{"inactive", 8},  (FPARSE)&parse_name_only_attr,	   (FPRINT)&print_name_only_attr,	(FCLONE)&clone_name_only_attr},
    {{"", 0},	       NULL, (FPRINT)&print_generic_string_attr,	(FCLONE)&clone_generic_string_attr}
};

/*
 * Scanner character specification.
 */
static int is_initialized;
static pj_char_spec cs_token;

static void init_sdp_parser(void)
{
    if (is_initialized == 0) {
	is_initialized = 1;
	if (is_initialized != 1) {
	    return;
	}
    }
    pj_cs_add_alpha(cs_token);
    pj_cs_add_num(cs_token);
    pj_cs_add_str( cs_token, TOKEN);
}

static int print_rtpmap_attr(const pjsdp_rtpmap_attr *rtpmap, 
			     char *buf, int len)
{
    char *p = buf;

    if (len < 16+rtpmap->encoding_name.slen+rtpmap->parameter.slen) {
	return -1;
    }
    
    /* colon and payload type. */
    *p++ = ':';
    len = pj_utoa(rtpmap->payload_type, p);
    p += len;

    /* space, encoding name */
    *p++ = ' ';
    pj_memcpy(p, rtpmap->encoding_name.ptr, rtpmap->encoding_name.slen);
    p += rtpmap->encoding_name.slen;

    /* slash, clock-rate. */
    *p++ = '/';
    len = pj_utoa(rtpmap->clock_rate, p);
    p += len;

    /* optionally add encoding parameter. */
    if (rtpmap->parameter.slen) {
	*p++ = '/';
	pj_memcpy(p, rtpmap->parameter.ptr, rtpmap->parameter.slen);
	p += rtpmap->parameter.slen;
    }

    return p-buf;
}

static int print_generic_string_attr(const pjsdp_attr_string *attr, 
				     char *buf, int len)
{
    char *p = buf;

    if (len < attr->value.slen + 4) {
	return -1;
    }

    /* colon and attribute value. */
    *p++ = ':';
    pj_memcpy(p, attr->value.ptr, attr->value.slen);
    p += attr->value.slen;

    return p-buf;
}

static int print_generic_num_attr(const pjsdp_attr_num *attr, char *buf, int len)
{
    char *p = buf;

    if (len < 10) {
	return -1;
    }
    *p++ = ':';
    return pj_utoa(attr->value, p);
}

static int print_name_only_attr(const pjsdp_attr *attr, char *buf, int len)
{
    PJ_UNUSED_ARG(attr)
    PJ_UNUSED_ARG(buf)
    PJ_UNUSED_ARG(len)
    return 0;
}

static int print_fmtp_attr(const pjsdp_fmtp_attr *fmtp, char *buf, int len)
{
    char *p = buf;

    if (len < 4+fmtp->format.slen+fmtp->param.slen) {
	return -1;
    }

    /* colon and format. */
    *p++ = ':';
    pj_memcpy(p, fmtp->format.ptr, fmtp->format.slen);
    p += fmtp->format.slen;

    /* space and parameter. */
    *p++ = ' ';
    pj_memcpy(p, fmtp->param.ptr, fmtp->param.slen);
    p += fmtp->param.slen;

    return p-buf;
}


static int print_attr(const pjsdp_attr *attr, char *buf, int len)
{
    char *p = buf;
    struct attr_map_rec *desc = &attr_map[attr->type];

    if (len < 16) {
	return -1;
    }

    *p++ = 'a';
    *p++ = '=';
    pj_memcpy(p, desc->name.ptr, desc->name.slen);
    p += desc->name.slen;
    
    len = (*desc->print_attr)(attr, p, (buf+len)-p);
    if (len < 0) {
	return -1;
    }
    p += len;
    *p++ = '\r';
    *p++ = '\n';
    return p-buf;
}

static pjsdp_attr* clone_rtpmap_attr (pj_pool_t *pool, const pjsdp_attr *p)
{
    const pjsdp_rtpmap_attr *rhs = (const pjsdp_rtpmap_attr*)p;
    pjsdp_rtpmap_attr *attr = pj_pool_alloc (pool, sizeof(pjsdp_rtpmap_attr));
    if (!attr)
	return NULL;

    attr->type = rhs->type;
    attr->payload_type = rhs->payload_type;
    if (!pj_strdup (pool, &attr->encoding_name, &rhs->encoding_name)) return NULL;
    attr->clock_rate = rhs->clock_rate;
    if (!pj_strdup (pool, &attr->parameter, &rhs->parameter)) return NULL;

    return (pjsdp_attr*)attr;
}

static pjsdp_attr* clone_generic_string_attr (pj_pool_t *pool, const pjsdp_attr *p)
{
    const pjsdp_attr_string* rhs = (const pjsdp_attr_string*) p;
    pjsdp_attr_string *attr = pj_pool_alloc (pool, sizeof(pjsdp_attr_string));
    if (!attr)
	return NULL;

    attr->type = rhs->type;
    if (!pj_strdup (pool, &attr->value, &rhs->value)) return NULL;

    return (pjsdp_attr*)attr;
}

static pjsdp_attr* clone_generic_num_attr (pj_pool_t *pool, const pjsdp_attr *p)
{
    const pjsdp_attr_num* rhs = (const pjsdp_attr_num*) p;
    pjsdp_attr_num *attr = pj_pool_alloc (pool, sizeof(pjsdp_attr_num));
    if (!attr)
	return NULL;

    attr->type = rhs->type;
    attr->value = rhs->value;

    return (pjsdp_attr*)attr;
}

static pjsdp_attr* clone_name_only_attr (pj_pool_t *pool, const pjsdp_attr *rhs)
{
    pjsdp_attr *attr = pj_pool_alloc (pool, sizeof(pjsdp_attr));
    if (!attr)
	return NULL;

    attr->type = rhs->type;
    return attr;
}

static pjsdp_attr* clone_fmtp_attr (pj_pool_t *pool, const pjsdp_attr *p)
{
    const pjsdp_fmtp_attr* rhs = (const pjsdp_fmtp_attr*) p;
    pjsdp_fmtp_attr *attr = pj_pool_alloc (pool, sizeof(pjsdp_fmtp_attr));
    if (!attr)
	return NULL;

    attr->type = rhs->type;
    if (!pj_strdup (pool, &attr->format, &rhs->format)) return NULL;
    if (!pj_strdup (pool, &attr->param, &rhs->param)) return NULL;

    return (pjsdp_attr*)attr;
}

PJ_DEF(pjsdp_attr*) pjsdp_attr_clone (pj_pool_t *pool, const pjsdp_attr *rhs)
{
    struct attr_map_rec *desc;

    if (rhs->type >= PJSDP_END_OF_ATTR) {
	pj_assert(0);
	return NULL;
    }

    desc = &attr_map[rhs->type];
    return (*desc->clone) (pool, rhs);
}

PJ_DEF(const pjsdp_attr*) pjsdp_attr_find (int count, const pjsdp_attr *attr_array[], int type)
{
    int i;

    for (i=0; i<count; ++i) {
	if (attr_array[i]->type == type)
	    return attr_array[i];
    }
    return NULL;
}

static int print_connection_info( pjsdp_conn_info *c, char *buf, int len)
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

PJ_DEF(pjsdp_conn_info*) pjsdp_conn_info_clone (pj_pool_t *pool, const pjsdp_conn_info *rhs)
{
    pjsdp_conn_info *c = pj_pool_alloc (pool, sizeof(pjsdp_conn_info));
    if (!c) return NULL;

    if (!pj_strdup (pool, &c->net_type, &rhs->net_type)) return NULL;
    if (!pj_strdup (pool, &c->addr_type, &rhs->addr_type)) return NULL;
    if (!pj_strdup (pool, &c->addr, &rhs->addr)) return NULL;

    return c;
}

static int print_media_desc( pjsdp_media_desc *m, char *buf, int len)
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

PJ_DEF(pjsdp_media_desc*) pjsdp_media_desc_clone (pj_pool_t *pool, 
						  const pjsdp_media_desc *rhs)
{
    unsigned int i;
    pjsdp_media_desc *m = pj_pool_alloc (pool, sizeof(pjsdp_media_desc));
    if (!m)
	return NULL;

    pj_strdup (pool, &m->desc.media, &rhs->desc.media);
    m->desc.port = rhs->desc.port;
    m->desc.port_count = rhs->desc.port_count;
    pj_strdup (pool, &m->desc.transport, &rhs->desc.transport);
    m->desc.fmt_count = rhs->desc.fmt_count;
    for (i=0; i<rhs->desc.fmt_count; ++i)
	m->desc.fmt[i] = rhs->desc.fmt[i];

    if (rhs->conn) {
	m->conn = pjsdp_conn_info_clone (pool, rhs->conn);
	if (!m->conn)
	    return NULL;
    } else {
	m->conn = NULL;
    }

    m->attr_count = rhs->attr_count;
    for (i=0; i < rhs->attr_count; ++i) {
	m->attr[i] = pjsdp_attr_clone (pool, rhs->attr[i]);
	if (!m->attr[i])
	    return NULL;
    }

    return m;
}

/** Check if the media description has the specified attribute. */
PJ_DEF(pj_bool_t) pjsdp_media_desc_has_attr (const pjsdp_media_desc *m, 
					     pjsdp_attr_type_e attr_type)
{
    unsigned i;
    for (i=0; i<m->attr_count; ++i) {
	pjsdp_attr *attr = m->attr[i];
	if (attr->type == attr_type)
	    return 1;
    }
    return 0;
}

/** Find rtpmap attribute for the specified payload type. */
PJ_DEF(const pjsdp_rtpmap_attr*) 
pjsdp_media_desc_find_rtpmap (const pjsdp_media_desc *m, unsigned pt)
{
    unsigned i;
    for (i=0; i<m->attr_count; ++i) {
	pjsdp_attr *attr = m->attr[i];
	if (attr->type == PJSDP_ATTR_RTPMAP) {
	    const pjsdp_rtpmap_attr* rtpmap = (const pjsdp_rtpmap_attr*)attr;
	    if (rtpmap->payload_type == pt)
		return rtpmap;
	}
    }
    return NULL;
}


static int print_session(const pjsdp_session_desc *ses, char *buf, pj_ssize_t len)
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

    /* Connection line (c=) if exist. */
    if (ses->conn) {
	printed = print_connection_info(ses->conn, p, end-p);
	if (printed < 1) {
	    return -1;
	}
	p += printed;
    }

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

static void parse_version(pj_scanner *scanner)
{
    pj_scan_advance_n(scanner, 3, SKIP_WS);
    pj_scan_get_newline(scanner);
}

static void parse_origin(pj_scanner *scanner, pjsdp_session_desc *ses)
{
    pj_str_t str;

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

static void parse_time(pj_scanner *scanner, pjsdp_session_desc *ses)
{
    pj_str_t str;

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

static void parse_generic_line(pj_scanner *scanner, pj_str_t *str)
{
    /* x= */
    pj_scan_advance_n(scanner, 2, SKIP_WS);

    /* get anything until newline. */
    pj_scan_get_until_ch(scanner, '\r', str);

    /* newline. */
    pj_scan_get_newline(scanner);
}

static void parse_connection_info(pj_scanner *scanner, pjsdp_conn_info *conn)
{
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

static void parse_media(pj_scanner *scanner, pjsdp_media_desc *med)
{
    pj_str_t str;

    /* m= */
    pj_scan_advance_n(scanner, 2, SKIP_WS);

    /* type */
    pj_scan_get_until_ch(scanner, ' ', &med->desc.media);
    pj_scan_get_char(scanner);

    /* port */
    pj_scan_get(scanner, cs_token, &str);
    med->desc.port = (unsigned short)pj_strtoul(&str);
    if (*scanner->current == '/') {
	/* port count */
	pj_scan_get_char(scanner);
	pj_scan_get(scanner, cs_token, &str);
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
    while (*scanner->current == ' ') {
	pj_scan_get_char(scanner);
	pj_scan_get(scanner, cs_token, &med->desc.fmt[med->desc.fmt_count++]);
    }

    /* newline */
    pj_scan_get_newline(scanner);
}

static pjsdp_rtpmap_attr * parse_rtpmap_attr( pj_pool_t *pool, pj_scanner *scanner )
{
    pjsdp_rtpmap_attr *rtpmap;
    pj_str_t str;

    rtpmap = pj_pool_calloc(pool, 1, sizeof(*rtpmap));
    if (pj_scan_get_char(scanner) != ':') {
	PJ_THROW(SYNTAX_ERROR);
    }
    pj_scan_get_until_ch(scanner, ' ', &str);
    rtpmap->payload_type = pj_strtoul(&str);
    pj_scan_get_char(scanner);

    pj_scan_get_until_ch(scanner, '/', &rtpmap->encoding_name);
    pj_scan_get_char(scanner);
    pj_scan_get(scanner, cs_token, &str);
    rtpmap->clock_rate = pj_strtoul(&str);

    if (*scanner->current == '/') {
	pj_scan_get_char(scanner);
	pj_scan_get_until_ch(scanner, '\r', &rtpmap->parameter);
    }

    return rtpmap;
}

static pjsdp_attr_string * parse_generic_string_attr( pj_pool_t *pool, pj_scanner *scanner )
{
    pjsdp_attr_string *attr;
    attr = pj_pool_calloc(pool, 1, sizeof(*attr));

    if (pj_scan_get_char(scanner) != ':') {
	PJ_THROW(SYNTAX_ERROR);
    }
    pj_scan_get_until_ch(scanner, '\r', &attr->value);
    return attr;
}

static pjsdp_attr_num *	parse_generic_num_attr( pj_pool_t *pool, pj_scanner *scanner )
{
    pjsdp_attr_num *attr;
    pj_str_t str;

    attr = pj_pool_calloc(pool, 1, sizeof(*attr));

    if (pj_scan_get_char(scanner) != ':') {
	PJ_THROW(SYNTAX_ERROR);
    }
    pj_scan_get_until_ch(scanner, '\r', &str);
    attr->value = pj_strtoul(&str);
    return attr;
}

static pjsdp_attr * parse_name_only_attr( pj_pool_t *pool, pj_scanner *scanner )
{
    pjsdp_attr *attr;

    PJ_UNUSED_ARG(scanner)
    attr = pj_pool_calloc(pool, 1, sizeof(*attr));
    return attr;
}

static pjsdp_fmtp_attr * parse_fmtp_attr( pj_pool_t *pool, pj_scanner *scanner )
{
    pjsdp_fmtp_attr *fmtp;

    fmtp = pj_pool_calloc(pool, 1, sizeof(*fmtp));

    if (pj_scan_get_char(scanner) != ':') {
	PJ_THROW(SYNTAX_ERROR);
    }
    pj_scan_get_until_ch(scanner, ' ', &fmtp->format);
    pj_scan_get_char(scanner);
    pj_scan_get_until_ch(scanner, '\r', &fmtp->param);
    return fmtp;
}

static pjsdp_attr *parse_attr( pj_pool_t *pool, pj_scanner *scanner)
{
    void * (*parse_func)(pj_pool_t *pool, pj_scanner *scanner) = NULL;
    pj_str_t attrname;
    unsigned i;
    pjsdp_attr *attr;

    /* skip a= */
    pj_scan_advance_n(scanner, 2, SKIP_WS);
    
    /* get attr name. */
    pj_scan_get(scanner, cs_token, &attrname);

    /* find entry to handle attrname */
    for (i=0; i<PJ_ARRAY_SIZE(attr_map); ++i) {
	struct attr_map_rec *p = &attr_map[i];
	if (pj_strcmp(&attrname, &p->name) == 0) {
	    parse_func = p->parse_attr;
	    break;
	}
    }

    /* fallback to generic string parser. */
    if (parse_func == NULL) {
	parse_func = &parse_generic_string_attr;
    }

    attr = (*parse_func)(pool, scanner);
    attr->type = i;
	
    /* newline */
    pj_scan_get_newline(scanner);

    return attr;
}

static void on_scanner_error(pj_scanner *scanner)
{
    PJ_UNUSED_ARG(scanner)

    PJ_THROW(SYNTAX_ERROR);
}

/*
 * Parse SDP message.
 */
PJ_DEF(pjsdp_session_desc*) pjsdp_parse( char *buf, pj_size_t len, 
					 pj_pool_t *pool)
{
    pj_scanner scanner;
    pjsdp_session_desc *session;
    pjsdp_media_desc *media = NULL;
    void *attr;
    pjsdp_conn_info *conn;
    pj_str_t dummy;
    int cur_name = 254;
    PJ_USE_EXCEPTION;

    init_sdp_parser();

    pj_scan_init(&scanner, buf, len, 0, &on_scanner_error);
    session = pj_pool_calloc(pool, 1, sizeof(*session));

    PJ_TRY {
	while (!pj_scan_is_eof(&scanner)) {
		cur_name = *scanner.current;
		switch (cur_name) {
		case 'a':
		    attr = parse_attr(pool, &scanner);
		    if (attr) {
			if (media) {
			    media->attr[media->attr_count++] = attr;
			} else {
			    session->attr[session->attr_count++] = attr;
			}
		    }
		    break;
		case 'o':
		    parse_origin(&scanner, session);
		    break;
		case 's':
		    parse_generic_line(&scanner, &session->name);
		    break;
		case 'c':
		    conn = pj_pool_calloc(pool, 1, sizeof(*conn));
		    parse_connection_info(&scanner, conn);
		    if (media) {
			media->conn = conn;
		    } else {
			session->conn = conn;
		    }
		    break;
		case 't':
		    parse_time(&scanner, session);
		    break;
		case 'm':
		    media = pj_pool_calloc(pool, 1, sizeof(*media));
		    parse_media(&scanner, media);
		    session->media[ session->media_count++ ] = media;
		    break;
		case 'v':
		    parse_version(&scanner);
		    break;
		default:
		    parse_generic_line(&scanner, &dummy);
		    break;
		}
	}
    }
    PJ_CATCH(SYNTAX_ERROR) {
	PJ_LOG(2, (LOG_THIS, "Syntax error in SDP parser '%c' line %d col %d",
		cur_name, scanner.line, scanner.col));
	if (!pj_scan_is_eof(&scanner)) {
	    if (*scanner.current != '\r') {
		pj_scan_get_until_ch(&scanner, '\r', &dummy);
	    }
	    pj_scan_get_newline(&scanner);
	}
    }
    PJ_END;

    pj_scan_fini(&scanner);
    return session;
}

/*
 * Print SDP description.
 */
PJ_DEF(int) pjsdp_print( const pjsdp_session_desc *desc, char *buf, pj_size_t size)
{
    return print_session(desc, buf, size);
}


