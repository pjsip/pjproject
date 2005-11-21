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
#define USER_CHAR   ALPHANUM MARK "&=+$,;?/"
#define PASS_CHAR   ALPHANUM MARK "&=+$,"
#define PARAM_CHAR  ALPHANUM MARK "[]/:&+$"

#define POOL_SIZE	8000
#define LOOP_COUNT	10000
#define AVERAGE_URL_LEN	80
#define THREAD_COUNT	4

static pj_highprec_t parse_len, print_len, cmp_len;
static pj_timestamp parse_time, print_time, cmp_time;


/* URI creator functions. */
static pjsip_uri *create_uri0( pj_pool_t *pool );
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
	&create_uri0
    },
    {
	PJ_SUCCESS,
	"sip:user@localhost",
	&create_uri1
    },
    {
	PJ_SUCCESS,
	"sip:user:password@localhost:5060",
	&create_uri2,    },
    {
	/* Port is specified should not match unspecified port. */
	ERR_NOT_EQUAL,
	"sip:localhost:5060",
	&create_uri3
    },
    {
	/* All recognized parameters. */
	PJ_SUCCESS,
	"sip:localhost;transport=tcp;user=ip;ttl=255;lr;maddr=127.0.0.1;method=ACK",
	&create_uri4
    },
    {
	/* Params mixed with other params and header params. */
	PJ_SUCCESS,
	"sip:localhost;pickup=hurry;user=phone;message=I%20am%20sorry"
	"?Subject=Hello%20There&Server=SIP%20Server",
	&create_uri5
    },
    {
	/* SIPS. */
	PJ_SUCCESS,
	"sips:localhost",
	&create_uri6,
    },
    {
	/* Name address */
	PJ_SUCCESS,
	"<sip:localhost>",
	&create_uri7
    },
    {
	/* Name address with display name and SIPS scheme with some redundant
	 * whitespaced.
	 */
	PJ_SUCCESS,
	"  Power Administrator  <sips:localhost>",
	&create_uri8
    },
    {
	/* Name address. */
	PJ_SUCCESS,
	" \"User\" <sip:user@localhost:5071>",
	&create_uri9
    },
    {
	/* Escaped sequence in display name (display=Strange User\"\\\"). */
	PJ_SUCCESS,
	" \"Strange User\\\"\\\\\\\"\" <sip:localhost>",
	&create_uri10,
    },
    {
	/* Errorneous escaping in display name. */
	ERR_SYNTAX_ERR,
	" \"Rogue User\\\" <sip:localhost>",
	&create_uri11,
    },
    {
	/* Dangling quote in display name, but that should be OK. */
	PJ_SUCCESS,
	"Strange User\" <sip:localhost>",
	&create_uri12,
    },
    {
	/* Special characters in parameter value must be quoted. */
	PJ_SUCCESS,
	"sip:localhost;pvalue=\"hello world\"",
	&create_uri13,
    },
    {
	/* Excercise strange character sets allowed in display, user, password,
	 * host, and port. 
	 */
	PJ_SUCCESS,
	"This is -. !% *_+`'~ me <sip:a19A&=+$,;?/%2c:%40a&Zz=+$,@"
	"my_proxy09.MY-domain.com:9801>",
	&create_uri14,
    },
    {
	/* Another excercise to the allowed character sets to the hostname. */
	PJ_SUCCESS,
	"sip:" ALPHANUM "-_.com",
	&create_uri15,
    },
    {
	/* Another excercise to the allowed character sets to the username 
	 * and password.
	 */
	PJ_SUCCESS,
	"sip:" USER_CHAR ":" PASS_CHAR "@host",
	&create_uri16,
    },
    {
	/* Excercise to the pname and pvalue, and mixup of other-param
	 * between 'recognized' params.
	 */
	PJ_SUCCESS,
	"sip:host;user=ip;" PARAM_CHAR "%21=" PARAM_CHAR "%21"
	";lr;other=1;transport=sctp;other2",
	&create_uri17,
    },
    {
	/* 18: This should trigger syntax error. */
	ERR_SYNTAX_ERR,
	"sip:",
	&create_dummy,
    },
    {
	/* 19: Syntax error: whitespace after scheme. */
	ERR_SYNTAX_ERR,
	"sip :host",
	&create_dummy,
    },
    {
	/* 20: Syntax error: whitespace before hostname. */
	ERR_SYNTAX_ERR,
	"sip: host",
	&create_dummy,
    },
    {
	/* 21: Syntax error: invalid port. */
	ERR_SYNTAX_ERR,
	"sip:user:password",
	&create_dummy,
    },
    {
	/* 22: Syntax error: no host. */
	ERR_SYNTAX_ERR,
	"sip:user@",
	&create_dummy,
    },
    {
	/* 23: Syntax error: no user/host. */
	ERR_SYNTAX_ERR,
	"sip:@",
	&create_dummy,
    },
    {
	/* 24: Syntax error: empty string. */
	ERR_SYNTAX_ERR,
	"",
	&create_dummy,
    }
};

static pjsip_uri *create_uri0(pj_pool_t *pool)
{
    /* "sip:localhost" */
    pjsip_url *url = pjsip_url_create(pool, 0);

    pj_strdup2(pool, &url->host, "localhost");
    return (pjsip_uri*)url;
}

static pjsip_uri *create_uri1(pj_pool_t *pool)
{
    /* "sip:user@localhost" */
    pjsip_url *url = pjsip_url_create(pool, 0);

    pj_strdup2( pool, &url->user, "user");
    pj_strdup2( pool, &url->host, "localhost");

    return (pjsip_uri*) url;
}

static pjsip_uri *create_uri2(pj_pool_t *pool)
{
    /* "sip:user:password@localhost:5060" */
    pjsip_url *url = pjsip_url_create(pool, 0);

    pj_strdup2( pool, &url->user, "user");
    pj_strdup2( pool, &url->passwd, "password");
    pj_strdup2( pool, &url->host, "localhost");
    url->port = 5060;

    return (pjsip_uri*) url;
}

static pjsip_uri *create_uri3(pj_pool_t *pool)
{
    /* Like: "sip:localhost:5060", but without the port. */
    pjsip_url *url = pjsip_url_create(pool, 0);

    pj_strdup2(pool, &url->host, "localhost");
    return (pjsip_uri*)url;
}

static pjsip_uri *create_uri4(pj_pool_t *pool)
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

#define param_add(list,pname,pvalue)  \
	do { \
	    pjsip_param *param; \
	    param=pj_pool_alloc(pool, sizeof(pjsip_param)); \
	    param->name = pj_str(pname); \
	    param->value = pj_str(pvalue); \
	    pj_list_insert_before(&list, param); \
	} while (0)

static pjsip_uri *create_uri5(pj_pool_t *pool)
{
    /* "sip:localhost;pickup=hurry;user=phone;message=I%20am%20sorry"
       "?Subject=Hello%20There&Server=SIP%20Server" 
     */
    pjsip_url *url = pjsip_url_create(pool, 0);

    pj_strdup2(pool, &url->host, "localhost");
    pj_strdup2(pool, &url->user_param, "phone");

    //pj_strdup2(pool, &url->other_param, ";pickup=hurry;message=I%20am%20sorry");
    param_add(url->other_param, "pickup", "hurry");
    param_add(url->other_param, "message", "I am sorry");

    //pj_strdup2(pool, &url->header_param, "?Subject=Hello%20There&Server=SIP%20Server");
    param_add(url->header_param, "Subject", "Hello There");
    param_add(url->header_param, "Server", "SIP Server");
    return (pjsip_uri*)url;

}

static pjsip_uri *create_uri6(pj_pool_t *pool)
{
    /* "sips:localhost" */
    pjsip_url *url = pjsip_url_create(pool, 1);

    pj_strdup2(pool, &url->host, "localhost");
    return (pjsip_uri*)url;
}

static pjsip_uri *create_uri7(pj_pool_t *pool)
{
    /* "<sip:localhost>" */
    pjsip_name_addr *name_addr = pjsip_name_addr_create(pool);
    pjsip_url *url;

    url = pjsip_url_create(pool, 0);
    name_addr->uri = (pjsip_uri*) url;

    pj_strdup2(pool, &url->host, "localhost");
    return (pjsip_uri*)name_addr;
}

static pjsip_uri *create_uri8(pj_pool_t *pool)
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

static pjsip_uri *create_uri9(pj_pool_t *pool)
{
    /* " \"User\" <sip:user@localhost:5071>" */
    pjsip_name_addr *name_addr = pjsip_name_addr_create(pool);
    pjsip_url *url;

    url = pjsip_url_create(pool, 0);
    name_addr->uri = (pjsip_uri*) url;

    pj_strdup2(pool, &name_addr->display, "User");
    pj_strdup2(pool, &url->user, "user");
    pj_strdup2(pool, &url->host, "localhost");
    url->port = 5071;
    return (pjsip_uri*)name_addr;
}

static pjsip_uri *create_uri10(pj_pool_t *pool)
{
    /* " \"Strange User\\\"\\\\\\\"\" <sip:localhost>" */
    pjsip_name_addr *name_addr = pjsip_name_addr_create(pool);
    pjsip_url *url;

    url = pjsip_url_create(pool, 0);
    name_addr->uri = (pjsip_uri*) url;

    pj_strdup2(pool, &name_addr->display, "Strange User\\\"\\\\\\\"");
    pj_strdup2(pool, &url->host, "localhost");
    return (pjsip_uri*)name_addr;
}

static pjsip_uri *create_uri11(pj_pool_t *pool)
{
    /* " \"Rogue User\\\" <sip:localhost>" */
    pjsip_name_addr *name_addr = pjsip_name_addr_create(pool);
    pjsip_url *url;

    url = pjsip_url_create(pool, 0);
    name_addr->uri = (pjsip_uri*) url;

    pj_strdup2(pool, &name_addr->display, "Rogue User\\");
    pj_strdup2(pool, &url->host, "localhost");
    return (pjsip_uri*)name_addr;
}

static pjsip_uri *create_uri12(pj_pool_t *pool)
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

static pjsip_uri *create_uri13(pj_pool_t *pool)
{
    /* "sip:localhost;pvalue=\"hello world\"" */
    pjsip_url *url;
    url = pjsip_url_create(pool, 0);
    pj_strdup2(pool, &url->host, "localhost");
    //pj_strdup2(pool, &url->other_param, ";pvalue=\"hello world\"");
    param_add(url->other_param, "pvalue", "hello world");
    return (pjsip_uri*)url;
}

static pjsip_uri *create_uri14(pj_pool_t *pool)
{
    /* "This is -. !% *_+`'~ me <sip:a19A&=+$,;?/%2c:%40a&Zz=+$,@my_proxy09.my-domain.com:9801>" */
    pjsip_name_addr *name_addr = pjsip_name_addr_create(pool);
    pjsip_url *url;

    url = pjsip_url_create(pool, 0);
    name_addr->uri = (pjsip_uri*) url;

    pj_strdup2(pool, &name_addr->display, "This is -. !% *_+`'~ me");
    pj_strdup2(pool, &url->user, "a19A&=+$,;?/,");
    pj_strdup2(pool, &url->passwd, "@a&Zz=+$,");
    pj_strdup2(pool, &url->host, "my_proxy09.MY-domain.com");
    url->port = 9801;
    return (pjsip_uri*)name_addr;
}

static pjsip_uri *create_uri15(pj_pool_t *pool)
{
    /* "sip:abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.com" */
    pjsip_url *url;
    url = pjsip_url_create(pool, 0);
    pj_strdup2(pool, &url->host, ALPHANUM "-_.com");
    return (pjsip_uri*)url;
}

static pjsip_uri *create_uri16(pj_pool_t *pool)
{
    /* "sip:" USER_CHAR ":" PASS_CHAR "@host" */
    pjsip_url *url;
    url = pjsip_url_create(pool, 0);
    pj_strdup2(pool, &url->user, USER_CHAR);
    pj_strdup2(pool, &url->passwd, PASS_CHAR);
    pj_strdup2(pool, &url->host, "host");
    return (pjsip_uri*)url;
}

static pjsip_uri *create_uri17(pj_pool_t *pool)
{
    /* "sip:host;user=ip;" PARAM_CHAR "%21=" PARAM_CHAR "%21;lr;other=1;transport=sctp;other2" */
    pjsip_url *url;
    url = pjsip_url_create(pool, 0);
    pj_strdup2(pool, &url->host, "host");
    pj_strdup2(pool, &url->user_param, "ip");
    pj_strdup2(pool, &url->transport_param, "sctp");
    param_add(url->other_param, PARAM_CHAR "!", PARAM_CHAR "!");
    param_add(url->other_param, "other", "1");
    param_add(url->other_param, "other2", "");
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
    parse_len = parse_len + entry->len;
    parsed_uri = pjsip_parse_uri(pool, entry->str, entry->len, 0);
    if (!parsed_uri) {
	/* Parsing failed. If the entry says that this is expected, then
	 * return OK.
	 */
	status = entry->status==ERR_SYNTAX_ERR ? PJ_SUCCESS : -10;
	if (status != 0) {
	    PJ_LOG(3,("", "   uri parse error!\n"
			  "   uri='%s'\n",
			  entry->str));
	}
	goto on_return;
    }
    pj_get_timestamp(&t2);
    pj_sub_timestamp(&t2, &t1);
    pj_add_timestamp(&parse_time, &t2);

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
    s1.ptr[len] = '\0';
    s1.slen = len;

    print_len = print_len + len;
    pj_get_timestamp(&t2);
    pj_sub_timestamp(&t2, &t1);
    pj_add_timestamp(&print_time, &t2);

    len = pjsip_uri_print( PJSIP_URI_IN_OTHER, ref_uri, s2.ptr, PJSIP_MAX_URL_SIZE);
    if (len < 1) {
	status = -30;
	goto on_return;
    }
    s2.ptr[len] = '\0';
    s2.slen = len;

    /* Full comparison of parsed URI with reference URI. */
    pj_get_timestamp(&t1);
    status = pjsip_uri_cmp(PJSIP_URI_IN_OTHER, parsed_uri, ref_uri);
    if (status != 0) {
	/* Not equal. See if this is the expected status. */
	status = entry->status==ERR_NOT_EQUAL ? PJ_SUCCESS : -40;
	if (status != 0) {
	    PJ_LOG(3,("", "   uri comparison mismatch, status=%d:\n"
			  "    uri1='%s'\n"
			  "    uri2='%s'",
			  status, s1.ptr, s2.ptr));
	}
	goto on_return;

    } else {
	/* Equal. See if this is the expected status. */
	status = entry->status==PJ_SUCCESS ? PJ_SUCCESS : -50;
	if (status != PJ_SUCCESS) {
	    goto on_return;
	}
    }

    cmp_len = cmp_len + len;
    pj_get_timestamp(&t2);
    pj_sub_timestamp(&t2, &t1);
    pj_add_timestamp(&cmp_time, &t2);

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
    unsigned i, loop;
    pj_pool_t *pool;
    pj_status_t status;
    pj_timestamp zero;
    pj_time_val elapsed;
    pj_highprec_t avg_parse, avg_print, avg_cmp, kbytes;

    zero.u32.hi = zero.u32.lo = 0;

    PJ_LOG(3,("", "  simple test"));
    pool = pjsip_endpt_create_pool(endpt, "", POOL_SIZE, POOL_SIZE);
    for (i=0; i<PJ_ARRAY_SIZE(uri_test_array); ++i) {
	status = do_uri_test(pool, &uri_test_array[i]);
	if (status != PJ_SUCCESS) {
	    PJ_LOG(3,("uri_test", "  error %d when testing entry %d",
		      status, i));
	    goto on_return;
	}
    }
    pjsip_endpt_destroy_pool(endpt, pool);

    PJ_LOG(3,("", "  benchmarking..."));
    parse_len = print_len = cmp_len = 0;
    parse_time.u32.hi = parse_time.u32.lo = 0;
    print_time.u32.hi = print_time.u32.lo = 0;
    cmp_time.u32.hi = cmp_time.u32.lo = 0;
    for (loop=0; loop<LOOP_COUNT; ++loop) {
	pool = pjsip_endpt_create_pool(endpt, "", POOL_SIZE, POOL_SIZE);
	for (i=0; i<PJ_ARRAY_SIZE(uri_test_array); ++i) {
	    status = do_uri_test(pool, &uri_test_array[i]);
	    if (status != PJ_SUCCESS) {
		PJ_LOG(3,("uri_test", "  error %d when testing entry %d",
			  status, i));
		pjsip_endpt_destroy_pool(endpt, pool);
		goto on_return;
	    }
	}
	pjsip_endpt_destroy_pool(endpt, pool);
    }

    kbytes = parse_len;
    pj_highprec_mod(kbytes, 1000000);
    pj_highprec_div(kbytes, 100000);
    elapsed = pj_elapsed_time(&zero, &parse_time);
    avg_parse = pj_elapsed_usec(&zero, &parse_time);
    pj_highprec_mul(avg_parse, AVERAGE_URL_LEN);
    pj_highprec_div(avg_parse, parse_len);
    avg_parse = 1000000 / avg_parse;

    PJ_LOG(3,("", "    %u.%u MB of urls parsed in %d.%03ds (avg=%d urls/sec)", 
		  (unsigned)(parse_len/1000000), (unsigned)kbytes,
		  elapsed.sec, elapsed.msec,
		  (unsigned)avg_parse));

    kbytes = print_len;
    pj_highprec_mod(kbytes, 1000000);
    pj_highprec_div(kbytes, 100000);
    elapsed = pj_elapsed_time(&zero, &print_time);
    avg_print = pj_elapsed_usec(&zero, &print_time);
    pj_highprec_mul(avg_print, AVERAGE_URL_LEN);
    pj_highprec_div(avg_print, parse_len);
    avg_print = 1000000 / avg_print;

    PJ_LOG(3,("", "    %u.%u MB of urls printed in %d.%03ds (avg=%d urls/sec)", 
		  (unsigned)(print_len/1000000), (unsigned)kbytes,
		  elapsed.sec, elapsed.msec,
		  (unsigned)avg_print));

    kbytes = cmp_len;
    pj_highprec_mod(kbytes, 1000000);
    pj_highprec_div(kbytes, 100000);
    elapsed = pj_elapsed_time(&zero, &cmp_time);
    avg_cmp = pj_elapsed_usec(&zero, &cmp_time);
    pj_highprec_mul(avg_cmp, AVERAGE_URL_LEN);
    pj_highprec_div(avg_cmp, cmp_len);
    avg_cmp = 1000000 / avg_cmp;

    PJ_LOG(3,("", "    %u.%u MB of urls compared in %d.%03ds (avg=%d urls/sec)", 
		  (unsigned)(cmp_len/1000000), (unsigned)kbytes,
		  elapsed.sec, elapsed.msec,
		  (unsigned)avg_cmp));

    PJ_LOG(3,("", "  multithreaded test"));


on_return:
    return status;
}

