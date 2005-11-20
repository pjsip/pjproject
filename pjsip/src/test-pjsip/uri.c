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
#include "test.h"
#include <pjsip_core.h>
#include <pjlib.h>


#define ALPHANUM    "abcdefghijklmnopqrstuvwxyz" \
		    "ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
		    "0123456789"
#define MARK	    "-_.!~*'()"
#define USER	    "&=+$,;?/%"
#define PASS	    "&=+$,%"
#define PARAM_CHAR  "[]/:&+$" MARK "%"

#define POOL_SIZE	4096

static pj_uint32_t parse_len, parse_time, print_time;


/* URI creator functions. */
static pjsip_uri *create_uri1( pj_pool_t *pool );
static pjsip_uri *create_uri2( pj_pool_t *pool );
static pjsip_uri *create_uri3( pj_pool_t *pool );
static pjsip_uri *create_uri4( pj_pool_t *pool );
static pjsip_uri *create_uri5( pj_pool_t *pool );
static pjsip_uri *create_uri6( pj_pool_t *pool );
static pjsip_uri *create_uri7( pj_pool_t *pool );
static pjsip_uri *create_uri8( pj_pool_t *pool );
static pjsip_uri *create_uri9( pj_pool_t *pool );
static pjsip_uri *create_uri10( pj_pool_t *pool );
static pjsip_uri *create_uri11( pj_pool_t *pool );
static pjsip_uri *create_uri12( pj_pool_t *pool );
static pjsip_uri *create_uri13( pj_pool_t *pool );
static pjsip_uri *create_uri14( pj_pool_t *pool );
static pjsip_uri *create_uri15( pj_pool_t *pool );
static pjsip_uri *create_uri16( pj_pool_t *pool );
static pjsip_uri *create_uri17( pj_pool_t *pool );
static pjsip_uri *create_uri18( pj_pool_t *pool );
static pjsip_uri *create_uri19( pj_pool_t *pool );
static pjsip_uri *create_dummy( pj_pool_t *pool );

#define ERR_NOT_EQUAL	-1001
#define ERR_SYNTAX_ERR	-1002

struct uri_test
{
    pj_status_t	     status;
    char	     str[PJSIP_MAX_URL_SIZE];
    pjsip_uri *(*creator)(pj_pool_t *pool);
    pj_size_t	     len;
} uri_test_array[] = 
{
    {
	PJ_SUCCESS,
	"sip:localhost",
	&create_uri1
    },
    {
	PJ_SUCCESS,
	"sip:user@localhost",
	&create_uri2
    },
    {
	PJ_SUCCESS,
	"sip:user:password@localhost:5060",
	&create_uri3,
    },
    {
	/* Port is specified should not match unspecified port. */
	ERR_NOT_EQUAL,
	"sip:localhost:5060",
	&create_uri4
    },
    {
	/* All recognized parameters. */
	PJ_SUCCESS,
	"sip:localhost;transport=tcp;user=ip;ttl=255;lr;maddr=127.0.0.1;method=ACK",
	&create_uri5
    },
    {
	/* Params mixed with other params and header params. */
	PJ_SUCCESS,
	"sip:localhost;pickup=hurry;user=phone;message=I%20am%20sorry"
	"?Subject=Hello%20There&Server=SIP%20Server",
	&create_uri6
    },
    {
	/* SIPS. */
	PJ_SUCCESS,
	"sips:localhost",
	&create_uri7,
    },
    {
	/* Name address */
	PJ_SUCCESS,
	"<sip:localhost>",
	&create_uri8
    },
    {
	/* Name address with display name and SIPS scheme with some redundant
	 * whitespaced.
	 */
	PJ_SUCCESS,
	"  Power Administrator  <sips:localhost>",
	&create_uri9
    },
    {
	/* Name address. */
	PJ_SUCCESS,
	" \"User\" <sip:user@localhost:5071>",
	&create_uri10
    },
    {
	/* Escaped sequence in display name (display=Strange User\"\\\"). */
	PJ_SUCCESS,
	" \"Strange User\\\"\\\\\\\"\" <sip:localhost>",
	&create_uri11,
    },
    {
	/* Errorneous escaping in display name. */
	ERR_SYNTAX_ERR,
	" \"Rogue User\\\" <sip:localhost>",
	&create_uri12,
    },
    {
	/* Dangling quote in display name, but that should be OK. */
	PJ_SUCCESS,
	"Strange User\" <sip:localhost>",
	&create_uri13,
    },
    {
	/* Special characters in parameter value must be quoted. */
	PJ_SUCCESS,
	"sip:localhost;pvalue=\"hello world\"",
	&create_uri14,
    },
    {
	/* Excercise strange character sets allowed in display, user, password,
	 * host, and port. 
	 */
	PJ_SUCCESS,
	"This is -. !% *_+`'~ me <sip:a19A&=+$,;?/%2c:%09a&Zz=+$,@"
	"my_proxy09.MY-domain.com:9801>",
	&create_uri15,
    },
    {
	/* Another excercise to the allowed character sets to the hostname. */
	PJ_SUCCESS,
	"sip:" ALPHANUM "-_.com",
	&create_uri16,
    },
    {
	/* Another excercise to the allowed character sets to the username 
	 * and password.
	 */
	PJ_SUCCESS,
	"sip:" ALPHANUM USER ":" ALPHANUM PASS "@host",
	&create_uri17,
    },
    {
	/* Excercise to the pname and pvalue, and mixup of other-param
	 * between 'recognized' params.
	 */
	PJ_SUCCESS,
	"sip:host;user=ip;" ALPHANUM PARAM_CHAR "=" ALPHANUM PARAM_CHAR 
	";lr;other=1;transport=sctp;other2",
	&create_uri18,
    },
    {
	/* This should trigger syntax error. */
	ERR_SYNTAX_ERR,
	"sip:",
	&create_dummy,
    },
    {
	/* Syntax error: whitespace after scheme. */
	ERR_SYNTAX_ERR,
	"sip :host",
	&create_dummy,
    },
    {
	/* Syntax error: whitespace before hostname. */
	ERR_SYNTAX_ERR,
	"sip: host",
	&create_dummy,
    },
    {
	/* Syntax error: invalid port. */
	ERR_SYNTAX_ERR,
	"sip:user:password",
	&create_dummy,
    },
    {
	/* Syntax error: no host. */
	ERR_SYNTAX_ERR,
	"sip:user@",
	&create_dummy,
    },
    {
	/* Syntax error: no user/host. */
	ERR_SYNTAX_ERR,
	"sip:@",
	&create_dummy,
    },
    {
	/* Syntax error: empty string. */
	ERR_SYNTAX_ERR,
	"",
	&create_dummy,
    }
};

static pjsip_uri *create_uri1(pj_pool_t *pool)
{
    /* "sip:localhost" */
    pjsip_url *url = pjsip_url_create(pool, 0);

    pj_strdup2(pool, &url->host, "localhost");
    return (pjsip_uri*)url;
}

static pjsip_uri *create_uri2(pj_pool_t *pool)
{
    /* "sip:user@localhost" */
    pjsip_url *url = pjsip_url_create(pool, 0);

    pj_strdup2( pool, &url->user, "user");
    pj_strdup2( pool, &url->host, "localhost");

    return (pjsip_uri*) url;
}

static pjsip_uri *create_uri3(pj_pool_t *pool)
{
    /* "sip:user:password@localhost:5060" */
    pjsip_url *url = pjsip_url_create(pool, 0);

    pj_strdup2( pool, &url->user, "user");
    pj_strdup2( pool, &url->passwd, "password");
    pj_strdup2( pool, &url->host, "localhost");
    url->port = 5060;

    return (pjsip_uri*) url;
}

static pjsip_uri *create_uri4(pj_pool_t *pool)
{
    /* Like: "sip:localhost:5060", but without the port. */
    pjsip_url *url = pjsip_url_create(pool, 0);

    pj_strdup2(pool, &url->host, "localhost");
    return (pjsip_uri*)url;
}

static pjsip_uri *create_uri5(pj_pool_t *pool)
{
    /* "sip:localhost;transport=tcp;user=ip;ttl=255;lr;maddr=127.0.0.1;method=ACK" */
    pjsip_url *url = pjsip_url_create(pool, 0);

    pj_strdup2(pool, &url->host, "localhost");
    pj_strdup2(pool, &url->transport_param, "tcp");
    pj_strdup2(pool, &url->user_param, "ip");
    url->ttl_param = 255;
    url->lr_param = 1;
    pj_strdup2(pool, &url->maddr_param, "127.0.0.1");
    pj_strdup2(pool, &url->method_param, "ACK");

    return (pjsip_uri*)url;
}

static pjsip_uri *create_uri6(pj_pool_t *pool)
{
    /* "sip:localhost;pickup=hurry;user=phone;message=I%20am%20sorry"
       "?Subject=Hello%20There&Server=SIP%20Server" 
     */
    pjsip_url *url = pjsip_url_create(pool, 0);

    pj_strdup2(pool, &url->host, "localhost");
    pj_strdup2(pool, &url->user_param, "phone");
    pj_strdup2(pool, &url->other_param, ";pickup=hurry;message=I%20am%20sorry");
    pj_strdup2(pool, &url->header_param, "?Subject=Hello%20There&Server=SIP%20Server");
    return (pjsip_uri*)url;

}

static pjsip_uri *create_uri7(pj_pool_t *pool)
{
    /* "sips:localhost" */
    pjsip_url *url = pjsip_url_create(pool, 1);

    pj_strdup2(pool, &url->host, "localhost");
    return (pjsip_uri*)url;
}

static pjsip_uri *create_uri8(pj_pool_t *pool)
{
    /* "<sip:localhost>" */
    pjsip_name_addr *name_addr = pjsip_name_addr_create(pool);
    pjsip_url *url;

    url = pjsip_url_create(pool, 0);
    name_addr->uri = (pjsip_uri*) url;

    pj_strdup2(pool, &url->host, "localhost");
    return (pjsip_uri*)name_addr;
}

static pjsip_uri *create_uri9(pj_pool_t *pool)
{
    /* "  Power Administrator <sips:localhost>" */
    pjsip_name_addr *name_addr = pjsip_name_addr_create(pool);
    pjsip_url *url;

    url = pjsip_url_create(pool, 1);
    name_addr->uri = (pjsip_uri*) url;

    pj_strdup2(pool, &name_addr->display, "Power Administrator");
    pj_strdup2(pool, &url->host, "localhost");
    return (pjsip_uri*)name_addr;
}

static pjsip_uri *create_uri10(pj_pool_t *pool)
{
    /* " \"User\" <sip:user@localhost:5071>" */
    pjsip_name_addr *name_addr = pjsip_name_addr_create(pool);
    pjsip_url *url;

    url = pjsip_url_create(pool, 0);
    name_addr->uri = (pjsip_uri*) url;

    pj_strdup2(pool, &name_addr->display, "\"User\"");
    pj_strdup2(pool, &url->user, "user");
    pj_strdup2(pool, &url->host, "localhost");
    url->port = 5071;
    return (pjsip_uri*)name_addr;
}

static pjsip_uri *create_uri11(pj_pool_t *pool)
{
    /* " \"Strange User\\\"\\\\\\\"\" <sip:localhost>" */
    pjsip_name_addr *name_addr = pjsip_name_addr_create(pool);
    pjsip_url *url;

    url = pjsip_url_create(pool, 0);
    name_addr->uri = (pjsip_uri*) url;

    pj_strdup2(pool, &name_addr->display, "\"Strange User\\\"\\\\\\\"\"");
    pj_strdup2(pool, &url->host, "localhost");
    return (pjsip_uri*)name_addr;
}

static pjsip_uri *create_uri12(pj_pool_t *pool)
{
    /* " \"Rogue User\\\" <sip:localhost>" */
    pjsip_name_addr *name_addr = pjsip_name_addr_create(pool);
    pjsip_url *url;

    url = pjsip_url_create(pool, 0);
    name_addr->uri = (pjsip_uri*) url;

    pj_strdup2(pool, &name_addr->display, "\"Rogue User\\\"");
    pj_strdup2(pool, &url->host, "localhost");
    return (pjsip_uri*)name_addr;
}

static pjsip_uri *create_uri13(pj_pool_t *pool)
{
    /* "Strange User\" <sip:localhost>" */
    pjsip_name_addr *name_addr = pjsip_name_addr_create(pool);
    pjsip_url *url;

    url = pjsip_url_create(pool, 0);
    name_addr->uri = (pjsip_uri*) url;

    pj_strdup2(pool, &name_addr->display, "Strange User\"");
    pj_strdup2(pool, &url->host, "localhost");
    return (pjsip_uri*)name_addr;
}

static pjsip_uri *create_uri14(pj_pool_t *pool)
{
    /* "sip:localhost;pvalue=\"hello world\"" */
    pjsip_url *url;
    url = pjsip_url_create(pool, 0);
    pj_strdup2(pool, &url->host, "localhost");
    pj_strdup2(pool, &url->other_param, ";pvalue=\"hello world\"");
    return (pjsip_uri*)url;
}

static pjsip_uri *create_uri15(pj_pool_t *pool)
{
    /* "This is -. !% *_+`'~ me <sip:a19A&=+$,;?/%2c:%09a&Zz=+$,@my_proxy09.my-domain.com:9801>" */
    pjsip_name_addr *name_addr = pjsip_name_addr_create(pool);
    pjsip_url *url;

    url = pjsip_url_create(pool, 0);
    name_addr->uri = (pjsip_uri*) url;

    pj_strdup2(pool, &name_addr->display, "This is -. !% *_+`'~ me");
    pj_strdup2(pool, &url->user, "a19A&=+$,;?/%2c");
    pj_strdup2(pool, &url->passwd, "%09a&Zz=+$,");
    pj_strdup2(pool, &url->host, "my_proxy09.MY-domain.com");
    url->port = 9801;
    return (pjsip_uri*)name_addr;
}

static pjsip_uri *create_uri16(pj_pool_t *pool)
{
    /* "sip:abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.com" */
    pjsip_url *url;
    url = pjsip_url_create(pool, 0);
    pj_strdup2(pool, &url->host, ALPHANUM "-_.com");
    return (pjsip_uri*)url;
}

static pjsip_uri *create_uri17(pj_pool_t *pool)
{
    /* "sip:" ALPHANUM USER ":" ALPHANUM PASS "@host" */
    pjsip_url *url;
    url = pjsip_url_create(pool, 0);
    pj_strdup2(pool, &url->user, ALPHANUM USER);
    pj_strdup2(pool, &url->passwd, ALPHANUM PASS);
    pj_strdup2(pool, &url->host, "host");
    return (pjsip_uri*)url;
}

static pjsip_uri *create_uri18(pj_pool_t *pool)
{
    /* "sip:host;user=ip;" ALPHANUM PARAM_CHAR "=" ALPHANUM PARAM_CHAR ";lr;other=1;transport=sctp;other2" */
    pjsip_url *url;
    url = pjsip_url_create(pool, 0);
    pj_strdup2(pool, &url->host, "host");
    pj_strdup2(pool, &url->user_param, "ip");
    pj_strdup2(pool, &url->transport_param, "sctp");
    pj_strdup2(pool, &url->other_param, ";" ALPHANUM PARAM_CHAR "=" ALPHANUM PARAM_CHAR ";other=1;other2");    
    url->lr_param = 1;
    return (pjsip_uri*)url;
}

static pjsip_uri *create_dummy(pj_pool_t *pool)
{
    PJ_UNUSED_ARG(pool);
    return NULL;
}

/*****************************************************************************/

/*
 * Test one test entry.
 */
static pj_status_t do_uri_test(pj_pool_t *pool, struct uri_test *entry)
{
    pj_status_t status;
    int len;
    pjsip_uri *parsed_uri, *ref_uri;
    pj_str_t s1 = {NULL, 0}, s2 = {NULL, 0};
    pj_timestamp t1, t2;

    entry->len = pj_native_strlen(entry->str);

    /* Parse URI text. */
    pj_get_timestamp(&t1);
    parse_len += entry->len;
    parsed_uri = pjsip_parse_uri(pool, entry->str, entry->len, 0);
    if (!parsed_uri) {
	/* Parsing failed. If the entry says that this is expected, then
	 * return OK.
	 */
	status = entry->status==ERR_SYNTAX_ERR ? PJ_SUCCESS : -10;
	goto on_return;
    }
    pj_get_timestamp(&t2);
    parse_time += t2.u32.lo - t1.u32.lo;

    /* Create the reference URI. */
    ref_uri = entry->creator(pool);

    /* Print both URI. */
    s1.ptr = pj_pool_alloc(pool, PJSIP_MAX_URL_SIZE);
    s2.ptr = pj_pool_alloc(pool, PJSIP_MAX_URL_SIZE);

    pj_get_timestamp(&t1);
    len = pjsip_uri_print( PJSIP_URI_IN_OTHER, parsed_uri, s1.ptr, PJSIP_MAX_URL_SIZE);
    if (len < 1) {
	status = -20;
	goto on_return;
    }
    s1.slen = len;

    len = pjsip_uri_print( PJSIP_URI_IN_OTHER, ref_uri, s2.ptr, PJSIP_MAX_URL_SIZE);
    if (len < 1) {
	status = -30;
	goto on_return;
    }
    s2.slen = len;
    pj_get_timestamp(&t2);
    print_time += t2.u32.lo - t1.u32.lo;

    /* Full comparison of parsed URI with reference URI. */
    if (pjsip_uri_cmp(PJSIP_URI_IN_OTHER, parsed_uri, ref_uri) != 0) {
	/* Not equal. See if this is the expected status. */
	status = entry->status==ERR_NOT_EQUAL ? PJ_SUCCESS : -40;
	goto on_return;

    } else {
	/* Equal. See if this is the expected status. */
	status = entry->status==PJ_SUCCESS ? PJ_SUCCESS : -50;
	if (status != PJ_SUCCESS) {
	    goto on_return;
	}
    }

    /* Compare text. */
    if (pj_strcmp(&s1, &s2) != 0) {
	/* Not equal. */
	status = -60;
    }

on_return:
    return status;
}

pj_status_t uri_test()
{
    unsigned i;
    pj_pool_t *pool;
    pj_status_t status;

    pool = pjsip_endpt_create_pool(endpt, "", 4000, 4000);

    for (i=0; i<PJ_ARRAY_SIZE(uri_test_array); ++i) {
	status = do_uri_test(pool, &uri_test_array[i]);
	if (status != PJ_SUCCESS) {
	    PJ_LOG(3,("uri_test", "  error %d when testing entry %d",
		      status, i));
	    break;
	}
    }

    pjsip_endpt_destroy_pool(endpt, pool);
    return status;
}

