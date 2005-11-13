/* $Id$
 *
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
#include <pjsip/sip_parser.h>
#include <pjsip/sip_uri.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <stdlib.h>
#include <stdio.h>
#include "test.h"

#define ERR_SYNTAX_ERR	(-2)
#define ERR_NOT_EQUAL	(-3)

#define ALPHANUM    "abcdefghijklmnopqrstuvwxyz" \
		    "ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
		    "0123456789"
#define MARK	    "-_.!~*'()"
#define USER	    "&=+$,;?/%"
#define PASS	    "&=+$,%"
#define PARAM_CHAR  "[]/:&+$" MARK "%"

#define POOL_SIZE	4096

static const char *STATUS_STR(pj_status_t status)
{
    switch (status) {
    case 0: return "OK";
    case ERR_SYNTAX_ERR: return "Syntax Error";
    case ERR_NOT_EQUAL: return "Not Equal";
    }
    return "???";
}

static pj_uint32_t parse_len, parse_time, print_time;
static pj_caching_pool cp;


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
    },
    {
	PJ_SUCCESS,
	"",
	NULL,
    },
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
    PJ_UNUSED_ARG(pool)
    return NULL;
}

/*****************************************************************************/

static void pool_error(pj_pool_t *pool, pj_size_t sz)
{
    PJ_UNUSED_ARG(pool)
    PJ_UNUSED_ARG(sz)

    pj_assert(0);
    exit(1);
}

/*
 * Test one test entry.
 */
static pj_status_t test_entry(struct uri_test *entry)
{
    pj_status_t status;
    pj_pool_t *pool;
    int len;
    pjsip_uri *parsed_uri, *ref_uri;
    pj_str_t s1 = {NULL, 0}, s2 = {NULL, 0};
    pj_hr_timestamp t1, t2;

    pool = (*cp.factory.create_pool)( &cp.factory, "", POOL_SIZE, 0, &pool_error);

    /* Parse URI text. */
    pj_hr_gettimestamp(&t1);
    parse_len += entry->len;
    parsed_uri = pjsip_parse_uri(pool, entry->str, entry->len, 0);
    if (!parsed_uri) {
	/* Parsing failed. If the entry says that this is expected, then
	 * return OK.
	 */
	status = entry->status==ERR_SYNTAX_ERR ? PJ_SUCCESS : ERR_SYNTAX_ERR;
	goto on_return;
    }
    pj_hr_gettimestamp(&t2);
    parse_time += t2.u32.lo - t1.u32.lo;

    /* Create the reference URI. */
    ref_uri = entry->creator(pool);

    /* Print both URI. */
    s1.ptr = pj_pool_alloc(pool, PJSIP_MAX_URL_SIZE);
    s2.ptr = pj_pool_alloc(pool, PJSIP_MAX_URL_SIZE);

    pj_hr_gettimestamp(&t1);
    len = pjsip_uri_print( PJSIP_URI_IN_OTHER, parsed_uri, s1.ptr, PJSIP_MAX_URL_SIZE);
    if (len < 1) {
	status = -1;
	goto on_return;
    }
    s1.slen = len;

    len = pjsip_uri_print( PJSIP_URI_IN_OTHER, ref_uri, s2.ptr, PJSIP_MAX_URL_SIZE);
    if (len < 1) {
	status = -1;
	goto on_return;
    }
    s2.slen = len;
    pj_hr_gettimestamp(&t2);
    print_time += t2.u32.lo - t1.u32.lo;

    /* Full comparison of parsed URI with reference URI. */
    if (pjsip_uri_cmp(PJSIP_URI_IN_OTHER, parsed_uri, ref_uri) != 0) {
	/* Not equal. See if this is the expected status. */
	status = entry->status==ERR_NOT_EQUAL ? PJ_SUCCESS : ERR_NOT_EQUAL;
	goto on_return;

    } else {
	/* Equal. See if this is the expected status. */
	status = entry->status==PJ_SUCCESS ? PJ_SUCCESS : -1;
	if (status != PJ_SUCCESS) {
	    goto on_return;
	}
    }

    /* Compare text. */
    if (pj_strcmp(&s1, &s2) != 0) {
	/* Not equal. */
	status = ERR_NOT_EQUAL;
    }

on_return:
    if (!SILENT) {
	printf("%.2d %s (expected status=%s)\n"
	       "   str=%s\n"
	       "   uri=%.*s\n"
	       "   ref=%.*s\n\n", 
	       entry-uri_test_array, 
	       STATUS_STR(status), 
	       STATUS_STR(entry->status), 
	       entry->str, 
	       (int)s1.slen, s1.ptr, (int)s2.slen, s2.ptr);
    }

    pj_pool_release(pool);
    return status;
}

static void warm_up(pj_pool_factory *pf)
{
    pj_pool_t *pool;
    struct uri_test *entry;

    pool = pj_pool_create(pf, "", POOL_SIZE, 0, &pool_error);
    pjsip_parse_uri(pool, "sip:host", 8, 0);
    entry = &uri_test_array[0];
    while (entry->creator) {
	entry->len = strlen(entry->str);
	++entry;
    }
    pj_pool_release(pool);
}

//#if !IS_PROFILING
#if 1
pj_status_t test_uri()
{
    struct uri_test *entry;
    int i=0, err=0;
    pj_status_t status;
    pj_hr_timestamp t1, t2;
    pj_uint32_t total_time;

    pj_caching_pool_init(&cp, &pj_pool_factory_default_policy, 0);
    warm_up(&cp.factory);

    pj_hr_gettimestamp(&t1);
    for (i=0; i<LOOP; ++i) {
	entry = &uri_test_array[0];
	while (entry->creator) {
	    status = test_entry(entry);
	    if (status != PJ_SUCCESS) {
		++err;
	    }
	    ++entry;
	}
    }
    pj_hr_gettimestamp(&t2);
    total_time = t2.u32.lo - t1.u32.lo;

    printf("Error=%d\n", err);
    printf("Total parse len:  %u bytes\n", parse_len);
    printf("Total parse time: %u (%f/char), print time: %u (%f/char)\n", 
	    parse_time, parse_time*1.0/parse_len,
	    print_time, print_time*1.0/parse_len);
    printf("Total time: %u (%f/char)\n", total_time, total_time*1.0/parse_len);
    return err;
}

#else

pj_status_t test_uri()
{
    struct uri_test *entry;
    unsigned i;

    warm_up();
    pj_caching_pool_init(&cp, 1024*1024);

    for (i=0; i<LOOP; ++i) {
	entry = &uri_test_array[0];
	while (entry->creator) {
	    pj_pool_t *pool;
	    pjsip_uri *uri1, *uri2;

	    pool = pj_pool_create( &cp.factory, "", POOL_SIZE, 0, &pool_error);
	    uri1 = pjsip_parse_uri(pool, entry->str, strlen(entry->str));
	    pj_pool_release(pool);
	    ++entry;
	}
    }

    return 0;
}

#endif
