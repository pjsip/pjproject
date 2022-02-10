/* $Id$ */
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
#include <pjsip/sip_parser.h>
#include <pjsip/sip_uri.h>
#include <pjsip/sip_msg.h>
#include <pjsip/sip_multipart.h>
#include <pjsip/sip_auth_parser.h>
#include <pjsip/sip_errno.h>
#include <pjsip/sip_transport.h>        /* rdata structure */
#include <pjlib-util/scanner.h>
#include <pjlib-util/string.h>
#include <pj/except.h>
#include <pj/log.h>
#include <pj/hash.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/ctype.h>
#include <pj/assert.h>
#include <pj/limits.h>

#define THIS_FILE	    "sip_parser.c"

#define ALNUM
#define RESERVED	    ";/?:@&=+$,"
#define MARK		    "-_.!~*'()"
#define UNRESERVED	    ALNUM MARK
#define ESCAPED		    "%"
#define USER_UNRESERVED	    "&=+$,;?/"
#define PASS		    "&=+$,"
#define TOKEN		    "-.!%*_`'~+"   /* '=' was removed for parsing 
					    * param */
#define HOST		    "_-."
#define HEX_DIGIT	    "abcdefABCDEF"
#define PARAM_CHAR	    "[]/:&+$" UNRESERVED ESCAPED
#define HNV_UNRESERVED	    "[]/?:+$"
#define HDR_CHAR	    HNV_UNRESERVED UNRESERVED ESCAPED

/* A generic URI can consist of (For a complete BNF see RFC 2396):
     #?;:@&=+-_.!~*'()%$,/
 */
#define GENERIC_URI_CHARS   "#?;:@&=+-_.!~*'()%$,/" "%"

#define UNREACHED(expr)

#define IS_NEWLINE(c)	((c)=='\r' || (c)=='\n')
#define IS_SPACE(c)	((c)==' ' || (c)=='\t')

/*
 * Header parser records.
 */
typedef struct handler_rec
{
    char		  hname[PJSIP_MAX_HNAME_LEN+1];
    pj_size_t		  hname_len;
    pj_uint32_t		  hname_hash;
    pjsip_parse_hdr_func *handler;
} handler_rec;

static handler_rec handler[PJSIP_MAX_HEADER_TYPES];
static unsigned handler_count;
static int parser_is_initialized;

/*
 * URI parser records.
 */
typedef struct uri_parser_rec
{
    pj_str_t		     scheme;
    pjsip_parse_uri_func    *parse;
} uri_parser_rec;

static uri_parser_rec uri_handler[PJSIP_MAX_URI_TYPES];
static unsigned uri_handler_count;

/*
 * Global vars (also extern).
 */
int PJSIP_SYN_ERR_EXCEPTION = -1;
int PJSIP_EINVAL_ERR_EXCEPTION = -2;

/* Parser constants */
static pjsip_parser_const_t pconst =
{
    { "user", 4},	/* pjsip_USER_STR	*/
    { "method", 6},	/* pjsip_METHOD_STR	*/
    { "transport", 9},	/* pjsip_TRANSPORT_STR	*/
    { "maddr", 5 },	/* pjsip_MADDR_STR	*/
    { "lr", 2 },	/* pjsip_LR_STR		*/
    { "sip", 3 },	/* pjsip_SIP_STR	*/
    { "sips", 4 },	/* pjsip_SIPS_STR	*/
    { "tel", 3 },	/* pjsip_TEL_STR	*/
    { "branch", 6 },	/* pjsip_BRANCH_STR	*/
    { "ttl", 3 },	/* pjsip_TTL_STR	*/
    { "received", 8 },	/* pjsip_RECEIVED_STR	*/
    { "q", 1 },		/* pjsip_Q_STR		*/
    { "expires", 7 },	/* pjsip_EXPIRES_STR	*/
    { "tag", 3 },	/* pjsip_TAG_STR	*/
    { "rport", 5}	/* pjsip_RPORT_STR	*/
};

/* Character Input Specification buffer. */
static pj_cis_buf_t cis_buf;


/*
 * Forward decl.
 */
static pjsip_msg *  int_parse_msg( pjsip_parse_ctx *ctx, 
				   pjsip_parser_err_report *err_list);
static void	    int_parse_param( pj_scanner *scanner, 
				     pj_pool_t *pool,
				     pj_str_t *pname, 
				     pj_str_t *pvalue,
				     unsigned option);
static void	    int_parse_uri_param( pj_scanner *scanner, 
					 pj_pool_t *pool,
					 pj_str_t *pname, 
					 pj_str_t *pvalue,
					 unsigned option);
static void	    int_parse_hparam( pj_scanner *scanner,
				      pj_pool_t *pool,
				      pj_str_t *hname,
				      pj_str_t *hvalue );
static void         int_parse_req_line( pj_scanner *scanner, 
					pj_pool_t *pool,
					pjsip_request_line *req_line);
static int          int_is_next_user( pj_scanner *scanner);
static void	    int_parse_status_line( pj_scanner *scanner, 
					   pjsip_status_line *line);
static void	    int_parse_user_pass( pj_scanner *scanner, 
					 pj_pool_t *pool,
					 pj_str_t *user, 
					 pj_str_t *pass);
static void	    int_parse_uri_host_port( pj_scanner *scanner, 
					     pj_str_t *p_host, 
					     int *p_port);
static pjsip_uri *  int_parse_uri_or_name_addr( pj_scanner *scanner, 
					        pj_pool_t *pool, 
                                                unsigned option);
static void*	    int_parse_sip_url( pj_scanner *scanner, 
				         pj_pool_t *pool,
				         pj_bool_t parse_params);
static pjsip_name_addr *
                    int_parse_name_addr( pj_scanner *scanner, 
					 pj_pool_t *pool );
static void*	    int_parse_other_uri(pj_scanner *scanner, 
					pj_pool_t *pool,
					pj_bool_t parse_params);
static void	    parse_hdr_end( pj_scanner *scanner );

static pjsip_hdr*   parse_hdr_accept( pjsip_parse_ctx *ctx );
static pjsip_hdr*   parse_hdr_allow( pjsip_parse_ctx *ctx );
static pjsip_hdr*   parse_hdr_call_id( pjsip_parse_ctx *ctx);
static pjsip_hdr*   parse_hdr_contact( pjsip_parse_ctx *ctx);
static pjsip_hdr*   parse_hdr_content_len( pjsip_parse_ctx *ctx );
static pjsip_hdr*   parse_hdr_content_type( pjsip_parse_ctx *ctx );
static pjsip_hdr*   parse_hdr_cseq( pjsip_parse_ctx *ctx );
static pjsip_hdr*   parse_hdr_expires( pjsip_parse_ctx *ctx );
static pjsip_hdr*   parse_hdr_from( pjsip_parse_ctx *ctx );
static pjsip_hdr*   parse_hdr_max_forwards( pjsip_parse_ctx *ctx);
static pjsip_hdr*   parse_hdr_min_expires( pjsip_parse_ctx *ctx );
static pjsip_hdr*   parse_hdr_rr( pjsip_parse_ctx *ctx );
static pjsip_hdr*   parse_hdr_route( pjsip_parse_ctx *ctx );
static pjsip_hdr*   parse_hdr_require( pjsip_parse_ctx *ctx );
static pjsip_hdr*   parse_hdr_retry_after( pjsip_parse_ctx *ctx );
static pjsip_hdr*   parse_hdr_supported( pjsip_parse_ctx *ctx );
static pjsip_hdr*   parse_hdr_to( pjsip_parse_ctx *ctx );
static pjsip_hdr*   parse_hdr_unsupported( pjsip_parse_ctx *ctx );
static pjsip_hdr*   parse_hdr_via( pjsip_parse_ctx *ctx );
static pjsip_hdr*   parse_hdr_generic_string( pjsip_parse_ctx *ctx);

/* Convert non NULL terminated string to integer. */
static unsigned long pj_strtoul_mindigit(const pj_str_t *str, 
                                         unsigned mindig)
{
    unsigned long value;
    unsigned i;

    value = 0;
    for (i=0; i<(unsigned)str->slen; ++i) {
	value = value * 10 + (str->ptr[i] - '0');
    }
    for (; i<mindig; ++i) {
	value = value * 10;
    }
    return value;
}

/* Case insensitive comparison */
#define parser_stricmp(s1, s2)  (s1.slen!=s2.slen || pj_stricmp_alnum(&s1, &s2))

/* Get a token and unescape */
PJ_INLINE(void) parser_get_and_unescape(pj_scanner *scanner, pj_pool_t *pool,
					const pj_cis_t *spec, 
					const pj_cis_t *unesc_spec,
					pj_str_t *token)
{
#if defined(PJSIP_UNESCAPE_IN_PLACE) && PJSIP_UNESCAPE_IN_PLACE!=0
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(spec);
    pj_scan_get_unescape(scanner, unesc_spec, token);
#else
    PJ_UNUSED_ARG(unesc_spec);
    pj_scan_get(scanner, spec, token);
    *token = pj_str_unescape(pool, token);
#endif
}

/* Syntax error handler for parser. */
static void on_syntax_error(pj_scanner *scanner)
{
    PJ_UNUSED_ARG(scanner);
    PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);
}

/* Syntax error handler for parser. */
static void on_str_parse_error(const pj_str_t *str, int rc)
{
    char *s;

    switch(rc) {
    case PJ_EINVAL:
        s = "NULL input string, invalid input string, or NULL return "\
	    "value pointer";
        break;
    case PJ_ETOOSMALL:
        s = "String value was less than the minimum allowed value.";
        break;
    case PJ_ETOOBIG:
        s = "String value was greater than the maximum allowed value.";
        break;
    default:
        s = "Unknown error";
    }

    if (str) {
        PJ_LOG(1, (THIS_FILE, "Error parsing '%.*s': %s",
                   (int)str->slen, str->ptr, s));
    } else {
        PJ_LOG(1, (THIS_FILE, "Can't parse input string: %s", s));
    }
    PJ_THROW(PJSIP_EINVAL_ERR_EXCEPTION);
}

static void strtoi_validate(const pj_str_t *str, int min_val,
			    int max_val, int *value)
{ 
    long retval;
    pj_status_t status;

    if (!str || !value) {
        on_str_parse_error(str, PJ_EINVAL);
    }
    status = pj_strtol2(str, &retval);
    if (status != PJ_EINVAL) {
	if (min_val > retval) {
	    *value = min_val;
	    status = PJ_ETOOSMALL;
	} else if (retval > max_val) {
	    *value = max_val;
	    status = PJ_ETOOBIG;
	} else
	    *value = (int)retval;
    }

    if (status != PJ_SUCCESS)
	on_str_parse_error(str, status);
}

/* Get parser constants. */
PJ_DEF(const pjsip_parser_const_t*) pjsip_parser_const(void)
{
    return &pconst;
}

/* Concatenate unrecognized params into single string. */
PJ_DEF(void) pjsip_concat_param_imp(pj_str_t *param, pj_pool_t *pool, 
			     	    const pj_str_t *pname, 
				    const pj_str_t *pvalue, 
                             	    int sepchar)
{
    char *new_param, *p;
    pj_size_t len;

    len = param->slen + pname->slen + pvalue->slen + 3;
    p = new_param = (char*) pj_pool_alloc(pool, len);
    
    if (param->slen) {
	pj_size_t old_len = param->slen;
	pj_memcpy(p, param->ptr, old_len);
	p += old_len;
    }
    *p++ = (char)sepchar;
    pj_memcpy(p, pname->ptr, pname->slen);
    p += pname->slen;

    if (pvalue->slen) {
	*p++ = '=';
	pj_memcpy(p, pvalue->ptr, pvalue->slen);
	p += pvalue->slen;
    }

    *p = '\0';
    
    param->ptr = new_param;
    param->slen = p - new_param;
}

/* Initialize static properties of the parser. */
static pj_status_t init_parser()
{
    pj_status_t status;

    /*
     * Syntax error exception number.
     */
    pj_assert (PJSIP_SYN_ERR_EXCEPTION == -1);
    status = pj_exception_id_alloc("PJSIP syntax error", 
				   &PJSIP_SYN_ERR_EXCEPTION);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /*
     * Invalid value exception.
     */
    pj_assert (PJSIP_EINVAL_ERR_EXCEPTION == -2);
    status = pj_exception_id_alloc("PJSIP invalid value error", 
				   &PJSIP_EINVAL_ERR_EXCEPTION);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /*
     * Init character input spec (cis)
     */

    pj_cis_buf_init(&cis_buf);

    status = pj_cis_init(&cis_buf, &pconst.pjsip_DIGIT_SPEC);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    pj_cis_add_num(&pconst.pjsip_DIGIT_SPEC);
    
    status = pj_cis_init(&cis_buf, &pconst.pjsip_ALPHA_SPEC);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    pj_cis_add_alpha( &pconst.pjsip_ALPHA_SPEC );
    
    status = pj_cis_init(&cis_buf, &pconst.pjsip_ALNUM_SPEC);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    pj_cis_add_alpha( &pconst.pjsip_ALNUM_SPEC );
    pj_cis_add_num( &pconst.pjsip_ALNUM_SPEC );

    status = pj_cis_init(&cis_buf, &pconst.pjsip_NOT_NEWLINE);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    pj_cis_add_str(&pconst.pjsip_NOT_NEWLINE, "\r\n");
    pj_cis_invert(&pconst.pjsip_NOT_NEWLINE);

    status = pj_cis_init(&cis_buf, &pconst.pjsip_NOT_COMMA_OR_NEWLINE);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    pj_cis_add_str( &pconst.pjsip_NOT_COMMA_OR_NEWLINE, ",\r\n");
    pj_cis_invert(&pconst.pjsip_NOT_COMMA_OR_NEWLINE);

    status = pj_cis_dup(&pconst.pjsip_TOKEN_SPEC, &pconst.pjsip_ALNUM_SPEC);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    pj_cis_add_str( &pconst.pjsip_TOKEN_SPEC, TOKEN);

    /* Token is allowed to have '%' so we do not need this. */
    /*
    status = pj_cis_dup(&pconst.pjsip_TOKEN_SPEC_ESC, &pconst.pjsip_TOKEN_SPEC);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    pj_cis_del_str(&pconst.pjsip_TOKEN_SPEC_ESC, "%");
    */

    status = pj_cis_dup(&pconst.pjsip_VIA_PARAM_SPEC, &pconst.pjsip_TOKEN_SPEC);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    pj_cis_add_str(&pconst.pjsip_VIA_PARAM_SPEC, "[:]");

    /* Token is allowed to have '%' */
    /*
    status = pj_cis_dup(&pconst.pjsip_VIA_PARAM_SPEC_ESC, &pconst.pjsip_TOKEN_SPEC_ESC);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    pj_cis_add_str(&pconst.pjsip_VIA_PARAM_SPEC_ESC, "[:]");
    */

    status = pj_cis_dup(&pconst.pjsip_HOST_SPEC, &pconst.pjsip_ALNUM_SPEC);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    pj_cis_add_str( &pconst.pjsip_HOST_SPEC, HOST);

    status = pj_cis_dup(&pconst.pjsip_HEX_SPEC, &pconst.pjsip_DIGIT_SPEC);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    pj_cis_add_str( &pconst.pjsip_HEX_SPEC, HEX_DIGIT);

    status = pj_cis_dup(&pconst.pjsip_PARAM_CHAR_SPEC, &pconst.pjsip_ALNUM_SPEC);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    pj_cis_add_str(&pconst.pjsip_PARAM_CHAR_SPEC, PARAM_CHAR);

    status = pj_cis_dup(&pconst.pjsip_PARAM_CHAR_SPEC_ESC, &pconst.pjsip_PARAM_CHAR_SPEC);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    pj_cis_del_str(&pconst.pjsip_PARAM_CHAR_SPEC_ESC, ESCAPED);

    status = pj_cis_dup(&pconst.pjsip_HDR_CHAR_SPEC, &pconst.pjsip_ALNUM_SPEC);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    pj_cis_add_str(&pconst.pjsip_HDR_CHAR_SPEC, HDR_CHAR);

    status = pj_cis_dup(&pconst.pjsip_HDR_CHAR_SPEC_ESC, &pconst.pjsip_HDR_CHAR_SPEC);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    pj_cis_del_str(&pconst.pjsip_HDR_CHAR_SPEC_ESC, ESCAPED);

    status = pj_cis_dup(&pconst.pjsip_USER_SPEC, &pconst.pjsip_ALNUM_SPEC);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    pj_cis_add_str( &pconst.pjsip_USER_SPEC, UNRESERVED ESCAPED USER_UNRESERVED );

    status = pj_cis_dup(&pconst.pjsip_USER_SPEC_ESC, &pconst.pjsip_USER_SPEC);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    pj_cis_del_str( &pconst.pjsip_USER_SPEC_ESC, ESCAPED);

    status = pj_cis_dup(&pconst.pjsip_USER_SPEC_LENIENT, &pconst.pjsip_USER_SPEC);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    pj_cis_add_str(&pconst.pjsip_USER_SPEC_LENIENT, "#");

    status = pj_cis_dup(&pconst.pjsip_USER_SPEC_LENIENT_ESC, &pconst.pjsip_USER_SPEC_ESC);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    pj_cis_add_str(&pconst.pjsip_USER_SPEC_LENIENT_ESC, "#");

    status = pj_cis_dup(&pconst.pjsip_PASSWD_SPEC, &pconst.pjsip_ALNUM_SPEC);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    pj_cis_add_str( &pconst.pjsip_PASSWD_SPEC, UNRESERVED ESCAPED PASS);

    status = pj_cis_dup(&pconst.pjsip_PASSWD_SPEC_ESC, &pconst.pjsip_PASSWD_SPEC);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    pj_cis_del_str( &pconst.pjsip_PASSWD_SPEC_ESC, ESCAPED);

    status = pj_cis_init(&cis_buf, &pconst.pjsip_PROBE_USER_HOST_SPEC);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    pj_cis_add_str( &pconst.pjsip_PROBE_USER_HOST_SPEC, "@ \n>");
    pj_cis_invert( &pconst.pjsip_PROBE_USER_HOST_SPEC );

    status = pj_cis_init(&cis_buf, &pconst.pjsip_DISPLAY_SPEC);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    pj_cis_add_str( &pconst.pjsip_DISPLAY_SPEC, ":\r\n<");
    pj_cis_invert(&pconst.pjsip_DISPLAY_SPEC);

    status = pj_cis_dup(&pconst.pjsip_OTHER_URI_CONTENT, &pconst.pjsip_ALNUM_SPEC);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    pj_cis_add_str( &pconst.pjsip_OTHER_URI_CONTENT, GENERIC_URI_CHARS);

    /*
     * Register URI parsers.
     */

    status = pjsip_register_uri_parser("sip", &int_parse_sip_url);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    status = pjsip_register_uri_parser("sips", &int_parse_sip_url);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /*
     * Register header parsers.
     */

    status = pjsip_register_hdr_parser( "Accept", NULL, &parse_hdr_accept);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    status = pjsip_register_hdr_parser( "Allow", NULL, &parse_hdr_allow);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    status = pjsip_register_hdr_parser( "Call-ID", "i", &parse_hdr_call_id);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    status = pjsip_register_hdr_parser( "Contact", "m", &parse_hdr_contact);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    status = pjsip_register_hdr_parser( "Content-Length", "l", 
                                        &parse_hdr_content_len);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    status = pjsip_register_hdr_parser( "Content-Type", "c", 
                                        &parse_hdr_content_type);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    status = pjsip_register_hdr_parser( "CSeq", NULL, &parse_hdr_cseq);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    status = pjsip_register_hdr_parser( "Expires", NULL, &parse_hdr_expires);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    status = pjsip_register_hdr_parser( "From", "f", &parse_hdr_from);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    status = pjsip_register_hdr_parser( "Max-Forwards", NULL, 
                                        &parse_hdr_max_forwards);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    status = pjsip_register_hdr_parser( "Min-Expires", NULL, 
                                        &parse_hdr_min_expires);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    status = pjsip_register_hdr_parser( "Record-Route", NULL, &parse_hdr_rr);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    status = pjsip_register_hdr_parser( "Route", NULL, &parse_hdr_route);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    status = pjsip_register_hdr_parser( "Require", NULL, &parse_hdr_require);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    status = pjsip_register_hdr_parser( "Retry-After", NULL, 
                                        &parse_hdr_retry_after);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    status = pjsip_register_hdr_parser( "Supported", "k", 
                                        &parse_hdr_supported);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    status = pjsip_register_hdr_parser( "To", "t", &parse_hdr_to);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    status = pjsip_register_hdr_parser( "Unsupported", NULL, 
                                        &parse_hdr_unsupported);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    status = pjsip_register_hdr_parser( "Via", "v", &parse_hdr_via);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /* 
     * Register auth parser. 
     */

    status = pjsip_auth_init_parser();

    return status;
}

void init_sip_parser(void)
{
    pj_enter_critical_section();
    if (++parser_is_initialized == 1) {
	init_parser();
    }
    pj_leave_critical_section();
}

void deinit_sip_parser(void)
{
    pj_enter_critical_section();
    if (--parser_is_initialized == 0) {
	/* Clear header handlers */
	pj_bzero(handler, sizeof(handler));
	handler_count = 0;

	/* Clear URI handlers */
	pj_bzero(uri_handler, sizeof(uri_handler));
	uri_handler_count = 0;

	/* Deregister exception ID */
	pj_exception_id_free(PJSIP_SYN_ERR_EXCEPTION);
	PJSIP_SYN_ERR_EXCEPTION = -1;

	pj_exception_id_free(PJSIP_EINVAL_ERR_EXCEPTION);
	PJSIP_EINVAL_ERR_EXCEPTION = -2;
    }
    pj_leave_critical_section();
}

/* Compare the handler record with header name, and return:
 * - 0  if handler match.
 * - <0 if handler is 'less' than the header name.
 * - >0 if handler is 'greater' than header name.
 */
PJ_INLINE(int) compare_handler( const handler_rec *r1, 
				const char *name, 
				pj_size_t name_len,
				pj_uint32_t hash )
{
    PJ_UNUSED_ARG(name_len);

    /* Compare hashed value. */
    if (r1->hname_hash < hash)
	return -1;
    if (r1->hname_hash > hash)
	return 1;

    /* Compare length. */
    /*
    if (r1->hname_len < name_len)
	return -1;
    if (r1->hname_len > name_len)
	return 1;
     */

    /* Equal length and equal hash. compare the strings. */
    return pj_memcmp(r1->hname, name, name_len);
}

/* Register one handler for one header name. */
static pj_status_t int_register_parser( const char *name, 
                                        pjsip_parse_hdr_func *fptr )
{
    unsigned	pos;
    handler_rec rec;

    if (handler_count >= PJ_ARRAY_SIZE(handler)) {
	pj_assert(!"Too many handlers!");
	return PJ_ETOOMANY;
    }

    /* Initialize temporary handler. */
    rec.handler = fptr;
    rec.hname_len = strlen(name);
    if (rec.hname_len >= sizeof(rec.hname)) {
	pj_assert(!"Header name is too long!");
	return PJ_ENAMETOOLONG;
    }
    /* Copy name. */
    pj_memcpy(rec.hname, name, rec.hname_len);
    rec.hname[rec.hname_len] = '\0';

    /* Calculate hash value. */
    rec.hname_hash = pj_hash_calc(0, rec.hname, (unsigned)rec.hname_len);

    /* Get the pos to insert the new handler. */
    for (pos=0; pos < handler_count; ++pos) {
	int d;
	d = compare_handler(&handler[pos], rec.hname, rec.hname_len, 
                            rec.hname_hash);
	if (d == 0) {
	    pj_assert(0);
	    return PJ_EEXISTS;
	}
	if (d > 0) {
	    break;
	}
    }

    /* Shift handlers. */
    if (pos != handler_count) {
	pj_memmove( &handler[pos+1], &handler[pos], 
                    (handler_count-pos)*sizeof(handler_rec));
    }
    /* Add new handler. */
    pj_memcpy( &handler[pos], &rec, sizeof(handler_rec));
    ++handler_count;

    return PJ_SUCCESS;
}

/* Register parser handler. If both header name and short name are valid,
 * then two instances of handler will be registered.
 */
PJ_DEF(pj_status_t) pjsip_register_hdr_parser( const char *hname,
					       const char *hshortname,
					       pjsip_parse_hdr_func *fptr)
{
    unsigned i;
    pj_size_t len;
    char hname_lcase[PJSIP_MAX_HNAME_LEN+1];
    pj_status_t status;

    /* Check that name is not too long */
    len = pj_ansi_strlen(hname);
    if (len > PJSIP_MAX_HNAME_LEN) {
	pj_assert(!"Header name is too long!");
	return PJ_ENAMETOOLONG;
    }

    /* Register the normal Mixed-Case name */
    status = int_register_parser(hname, fptr);
    if (status != PJ_SUCCESS) {
	return status;
    }

    /* Get the lower-case name */
    for (i=0; i<len; ++i) {
	hname_lcase[i] = (char)pj_tolower(hname[i]);
    }
    hname_lcase[len] = '\0';

    /* Register the lower-case version of the name */
    status = int_register_parser(hname_lcase, fptr);
    if (status != PJ_SUCCESS) {
	return status;
    }
    

    /* Register the shortname version of the name */
    if (hshortname) {
        status = int_register_parser(hshortname, fptr);
        if (status != PJ_SUCCESS) 
	    return status;
    }
    return PJ_SUCCESS;
}


/* Find handler to parse the header name. */
static pjsip_parse_hdr_func * find_handler_imp(pj_uint32_t  hash, 
					       const pj_str_t *hname)
{
    handler_rec *first;
    int		 comp;
    unsigned	 n;

    /* Binary search for the handler. */
    comp = -1;
    first = &handler[0];
    n = handler_count;
    for (; n > 0; ) {
	unsigned half = n / 2;
	handler_rec *mid = first + half;

	comp = compare_handler(mid, hname->ptr, hname->slen, hash);
	if (comp < 0) {
	    first = ++mid;
	    n -= half + 1;
	} else if (comp==0) {
	    first = mid;
	    break;
	} else {
	    n = half;
	}
    }

    return comp==0 ? first->handler : NULL;
}


/* Find handler to parse the header name. */
static pjsip_parse_hdr_func* find_handler(const pj_str_t *hname)
{
    pj_uint32_t hash;
    char hname_copy[PJSIP_MAX_HNAME_LEN];
    pj_str_t tmp;
    pjsip_parse_hdr_func *func;

    if (hname->slen >= PJSIP_MAX_HNAME_LEN) {
	/* Guaranteed not to be able to find handler. */
        return NULL;
    }

    /* First, common case, try to find handler with exact name */
    hash = pj_hash_calc(0, hname->ptr, (unsigned)hname->slen);
    func = find_handler_imp(hash, hname);
    if (func)
	return func;


    /* If not found, try converting the header name to lowercase and
     * search again.
     */
    hash = pj_hash_calc_tolower(0, hname_copy, hname);
    tmp.ptr = hname_copy;
    tmp.slen = hname->slen;
    return find_handler_imp(hash, &tmp);
}


/* Find URI handler. */
static pjsip_parse_uri_func* find_uri_handler(const pj_str_t *scheme)
{
    unsigned i;
    for (i=0; i<uri_handler_count; ++i) {
	if (parser_stricmp(uri_handler[i].scheme, (*scheme))==0)
	    return uri_handler[i].parse;
    }
    return &int_parse_other_uri;
}

/* Register URI parser. */
PJ_DEF(pj_status_t) pjsip_register_uri_parser( char *scheme,
					       pjsip_parse_uri_func *func)
{
    if (uri_handler_count >= PJ_ARRAY_SIZE(uri_handler))
	return PJ_ETOOMANY;

    uri_handler[uri_handler_count].scheme = pj_str((char*)scheme);
    uri_handler[uri_handler_count].parse = func;
    ++uri_handler_count;

    return PJ_SUCCESS;
}

/* Public function to parse SIP message. */
PJ_DEF(pjsip_msg*) pjsip_parse_msg( pj_pool_t *pool, 
                                    char *buf, pj_size_t size,
				    pjsip_parser_err_report *err_list)
{
    pjsip_msg *msg = NULL;
    pj_scanner scanner;
    pjsip_parse_ctx context;

    pj_scan_init(&scanner, buf, size, PJ_SCAN_AUTOSKIP_WS_HEADER, 
                 &on_syntax_error);

    context.scanner = &scanner;
    context.pool = pool;
    context.rdata = NULL;

    msg = int_parse_msg(&context, err_list);

    pj_scan_fini(&scanner);
    return msg;
}

/* Public function to parse as rdata.*/
PJ_DEF(pjsip_msg *) pjsip_parse_rdata( char *buf, pj_size_t size,
                                       pjsip_rx_data *rdata )
{
    pj_scanner scanner;
    pjsip_parse_ctx context;

    pj_scan_init(&scanner, buf, size, PJ_SCAN_AUTOSKIP_WS_HEADER, 
                 &on_syntax_error);

    context.scanner = &scanner;
    context.pool = rdata->tp_info.pool;
    context.rdata = rdata;

    rdata->msg_info.msg = int_parse_msg(&context, &rdata->msg_info.parse_err);

    pj_scan_fini(&scanner);
    return rdata->msg_info.msg;
}

/* Determine if a message has been received. */
PJ_DEF(pj_status_t) pjsip_find_msg( const char *buf, pj_size_t size, 
				  pj_bool_t is_datagram, pj_size_t *msg_size)
{
#if PJ_HAS_TCP
    const char *volatile hdr_end;
    const char *volatile body_start;
    const char *pos;
    const char *volatile line;
    int content_length = -1;
    pj_str_t cur_msg;
    pj_status_t status = PJSIP_EMISSINGHDR;
    const pj_str_t end_hdr = { "\n\r\n", 3};

    *msg_size = size;

    /* For datagram, the whole datagram IS the message. */
    if (is_datagram) {
	return PJ_SUCCESS;
    }


    /* Find the end of header area by finding an empty line. 
     * Don't use plain strstr() since we want to be able to handle
     * NULL character in the message
     */
    cur_msg.ptr = (char*)buf; cur_msg.slen = size;
    pos = pj_strstr(&cur_msg, &end_hdr);
    if (pos == NULL) {
	return PJSIP_EPARTIALMSG;
    }
 
    hdr_end = pos+1;
    body_start = pos+3;

    /* Find "Content-Length" header the hard way. */
    line = pj_strchr(&cur_msg, '\n');
    while (line && line < hdr_end) {
	++line;
	if ( ((*line=='C' || *line=='c') && 
              strnicmp_alnum(line, "Content-Length", 14) == 0) ||
	     ((*line=='l' || *line=='L') && 
              (*(line+1)==' ' || *(line+1)=='\t' || *(line+1)==':')))
	{
	    /* Try to parse the header. */
	    pj_scanner scanner;
	    PJ_USE_EXCEPTION;

	    /* The buffer passed to the scanner is not NULL terminated,
             * but should be safe. See ticket #2063.
	     */ 
	    pj_scan_init(&scanner, (char*)line, hdr_end-line, 
			 PJ_SCAN_AUTOSKIP_WS_HEADER, &on_syntax_error);

	    PJ_TRY {
		pj_str_t str_clen;

		/* Get "Content-Length" or "L" name */
		if (*line=='C' || *line=='c')
		    pj_scan_advance_n(&scanner, 14, PJ_TRUE);
		else if (*line=='l' || *line=='L')
		    pj_scan_advance_n(&scanner, 1, PJ_TRUE);

		/* Get colon */
		if (pj_scan_get_char(&scanner) != ':') {
		    PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);
		}

		/* Get number */
		pj_scan_get(&scanner, &pconst.pjsip_DIGIT_SPEC, &str_clen);

		/* Get newline. */
		pj_scan_get_newline(&scanner);

		/* Found a valid Content-Length header. */
		strtoi_validate(&str_clen, PJSIP_MIN_CONTENT_LENGTH,
				PJSIP_MAX_CONTENT_LENGTH, &content_length);
	    }
	    PJ_CATCH_ANY {
		int eid = PJ_GET_EXCEPTION();
		if (eid == PJSIP_SYN_ERR_EXCEPTION) {
		    status = PJSIP_EMISSINGHDR;
		} else if (eid == PJSIP_EINVAL_ERR_EXCEPTION) {
		    status = PJSIP_EINVALIDHDR;
		}
		content_length = -1;
	    }
	    PJ_END

	    pj_scan_fini(&scanner);
	}

	/* Found valid Content-Length? */
	if (content_length != -1)
	    break;

	/* Go to next line. */
	cur_msg.slen -= (line - cur_msg.ptr);
	cur_msg.ptr = (char*)line;
	line = pj_strchr(&cur_msg, '\n');
    }

    /* Found Content-Length? */
    if (content_length == -1) {
	return status;
    }

    /* Enough packet received? */
    *msg_size = (body_start - buf) + content_length;
    return (*msg_size) <= size ? PJ_SUCCESS : PJSIP_EPARTIALMSG;
#else
    PJ_UNUSED_ARG(buf);
    PJ_UNUSED_ARG(is_datagram);
    *msg_size = size;
    return PJ_SUCCESS;
#endif
}

/* Public function to parse URI */
PJ_DEF(pjsip_uri*) pjsip_parse_uri( pj_pool_t *pool, 
					 char *buf, pj_size_t size,
					 unsigned option)
{
    pj_scanner scanner;
    pjsip_uri *uri = NULL;
    PJ_USE_EXCEPTION;

    pj_scan_init(&scanner, buf, size, 0, &on_syntax_error);

    
    PJ_TRY {
	uri = int_parse_uri_or_name_addr(&scanner, pool, option);
    }
    PJ_CATCH_ANY {
	uri = NULL;
    }
    PJ_END;

    /* Must have exhausted all inputs. */
    if (pj_scan_is_eof(&scanner) || IS_NEWLINE(*scanner.curptr)) {
	/* Success. */
	pj_scan_fini(&scanner);
	return uri;
    }

    /* Still have some characters unparsed. */
    pj_scan_fini(&scanner);
    return NULL;
}

/* SIP version */
static void parse_sip_version(pj_scanner *scanner)
{
    pj_str_t SIP = { "SIP", 3 };
    pj_str_t V2 = { "2.0", 3 };
    pj_str_t sip, version;

    pj_scan_get( scanner, &pconst.pjsip_ALPHA_SPEC, &sip);
    if (pj_scan_get_char(scanner) != '/')
	on_syntax_error(scanner);
    pj_scan_get_n( scanner, 3, &version);
    if (pj_stricmp(&sip, &SIP) || pj_stricmp(&version, &V2))
	on_syntax_error(scanner);
}

static pj_bool_t is_next_sip_version(pj_scanner *scanner)
{
    pj_str_t SIP = { "SIP", 3 };
    pj_str_t sip;
    int c;

    c = pj_scan_peek(scanner, &pconst.pjsip_ALPHA_SPEC, &sip);
    /* return TRUE if it is "SIP" followed by "/" or space.
     * we include space since the "/" may be separated by space,
     * although this would mean it would return TRUE if it is a
     * request and the method is "SIP"!
     */
    return c && (c=='/' || c==' ' || c=='\t') && pj_stricmp(&sip, &SIP)==0;
}

/* Internal function to parse SIP message */
static pjsip_msg *int_parse_msg( pjsip_parse_ctx *ctx,
				 pjsip_parser_err_report *err_list)
{
    /* These variables require "volatile" so their values get
     * preserved when re-entering the PJ_TRY block after an error.
     */
    volatile pj_bool_t parsing_headers;
    pjsip_msg *volatile msg = NULL;
    pjsip_ctype_hdr *volatile ctype_hdr = NULL;

    pj_str_t hname;
    pj_scanner *scanner = ctx->scanner;
    pj_pool_t *pool = ctx->pool;
    PJ_USE_EXCEPTION;

    parsing_headers = PJ_FALSE;

retry_parse:
    PJ_TRY 
    {
	if (parsing_headers)
	    goto parse_headers;

	/* Skip leading newlines. */
	while (IS_NEWLINE(*scanner->curptr)) {
	    pj_scan_get_newline(scanner);
	}

	/* Check if we still have valid packet.
	 * Sometimes endpoints just send blank (CRLF) packets just to keep
	 * NAT bindings open.
	 */
	if (pj_scan_is_eof(scanner))
	    return NULL;

	/* Parse request or status line */
	if (is_next_sip_version(scanner)) {
	    msg = pjsip_msg_create(pool, PJSIP_RESPONSE_MSG);
	    int_parse_status_line( scanner, &msg->line.status );
	} else {
	    msg = pjsip_msg_create(pool, PJSIP_REQUEST_MSG);
	    int_parse_req_line(scanner, pool, &msg->line.req );
	}

	parsing_headers = PJ_TRUE;

parse_headers:
	/* Parse headers. */
	do {
	    pjsip_parse_hdr_func * func;
	    pjsip_hdr *hdr = NULL;

	    /* Init hname just in case parsing fails.
	     * Ref: PROTOS #2412
	     */
	    hname.slen = 0;
	    
	    /* Get hname. */
	    pj_scan_get( scanner, &pconst.pjsip_TOKEN_SPEC, &hname);
	    if (pj_scan_get_char( scanner ) != ':') {
		PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);
	    }
	    
	    /* Find handler. */
	    func = find_handler(&hname);
	    
	    /* Call the handler if found.
	     * If no handler is found, then treat the header as generic
	     * hname/hvalue pair.
	     */
	    if (func) {
		hdr = (*func)(ctx);

		/* Note:
		 *  hdr MAY BE NULL, if parsing does not yield a new header
		 *  instance, e.g. the values have been added to existing
		 *  header. See http://trac.pjsip.org/repos/ticket/940
		 */

		/* Check if we've just parsed a Content-Type header. 
		 * We will check for a message body if we've got Content-Type 
		 * header.
		 */
		if (hdr && hdr->type == PJSIP_H_CONTENT_TYPE) {
		    ctype_hdr = (pjsip_ctype_hdr*)hdr;
		}

	    } else {
		hdr = parse_hdr_generic_string(ctx);
		hdr->name = hdr->sname = hname;
	    }
	    
	    /* Single parse of header line can produce multiple headers.
	     * For example, if one Contact: header contains Contact list
	     * separated by comma, then these Contacts will be split into
	     * different Contact headers.
	     * So here we must insert list instead of just insert one header.
	     */
	    if (hdr)
		pj_list_insert_nodes_before(&msg->hdr, hdr);
	    
	    /* Parse until EOF or an empty line is found. */
	} while (!pj_scan_is_eof(scanner) && !IS_NEWLINE(*scanner->curptr));
	
	parsing_headers = PJ_FALSE;

	/* If empty line is found, eat it. */
	if (!pj_scan_is_eof(scanner)) {
	    if (IS_NEWLINE(*scanner->curptr)) {
		pj_scan_get_newline(scanner);
	    }
	}

	/* If we have Content-Type header, treat the rest of the message 
	 * as body.
	 */
	if (ctype_hdr && scanner->curptr!=scanner->end) {
	    /* New: if Content-Type indicates that this is a multipart
	     * message body, parse it.
	     */
	    const pj_str_t STR_MULTIPART = { "multipart", 9 };
	    pjsip_msg_body *body;

	    if (pj_stricmp(&ctype_hdr->media.type, &STR_MULTIPART)==0) {
		body = pjsip_multipart_parse(pool, scanner->curptr,
					     scanner->end - scanner->curptr,
					     &ctype_hdr->media, 0);
	    } else {
		body = PJ_POOL_ALLOC_T(pool, pjsip_msg_body);
		pjsip_media_type_cp(pool, &body->content_type,
		                    &ctype_hdr->media);

		body->data = scanner->curptr;
		body->len = (unsigned)(scanner->end - scanner->curptr);
		body->print_body = &pjsip_print_text_body;
		body->clone_data = &pjsip_clone_text_data;
	    }

	    msg->body = body;
	}
    }
    PJ_CATCH_ANY 
    {
	/* Exception was thrown during parsing. 
	 * Skip until newline, and parse next header. 
	 */
	if (err_list) {
	    pjsip_parser_err_report *err_info;
	    
	    err_info = PJ_POOL_ALLOC_T(pool, pjsip_parser_err_report);
	    err_info->except_code = PJ_GET_EXCEPTION();
	    err_info->line = scanner->line;
	    /* Scanner's column is zero based, so add 1 */
	    err_info->col = pj_scan_get_col(scanner) + 1;
	    if (parsing_headers)
		err_info->hname = hname;
	    else if (msg && msg->type == PJSIP_REQUEST_MSG)
		err_info->hname = pj_str("Request Line");
	    else if (msg && msg->type == PJSIP_RESPONSE_MSG)
		err_info->hname = pj_str("Status Line");
	    else
		err_info->hname.slen = 0;
	    
	    pj_list_insert_before(err_list, err_info);
	}
	
	if (parsing_headers) {
	    if (!pj_scan_is_eof(scanner)) {
		/* Skip until next line.
		 * Watch for header continuation.
		 */
		do {
		    pj_scan_skip_line(scanner);
		} while (IS_SPACE(*scanner->curptr));
	    }

	    /* Restore flag. Flag may be set in int_parse_sip_url() */
	    scanner->skip_ws = PJ_SCAN_AUTOSKIP_WS_HEADER;

	    /* Continue parse next header, if any. */
	    if (!pj_scan_is_eof(scanner) && !IS_NEWLINE(*scanner->curptr)) {
		goto retry_parse;
	    }
	}

	msg = NULL;
    }
    PJ_END;

    return msg;
}


/* Parse parameter (pname ["=" pvalue]). */
static void parse_param_imp( pj_scanner *scanner, pj_pool_t *pool,
			     pj_str_t *pname, pj_str_t *pvalue,
			     const pj_cis_t *spec, const pj_cis_t *esc_spec,
			     unsigned option)
{
    /* pname */
    if (!esc_spec) {
    	pj_scan_get(scanner, spec, pname);
    } else {
	parser_get_and_unescape(scanner, pool, spec, esc_spec, pname);
    }

    /* init pvalue */
    pvalue->ptr = NULL;
    pvalue->slen = 0;

    /* pvalue, if any */
    if (*scanner->curptr == '=') {
	pj_scan_get_char(scanner);
	if (!pj_scan_is_eof(scanner)) {
	    /* pvalue can be a quoted string. */
	    if (*scanner->curptr == '"') {
		pj_scan_get_quote( scanner, '"', '"', pvalue);
		if (option & PJSIP_PARSE_REMOVE_QUOTE) {
		    pvalue->ptr++;
		    pvalue->slen -= 2;
		}
	    // } else if (*scanner->curptr == '[') {
		/* pvalue can be a quoted IPv6; in this case, the
		 * '[' and ']' quote characters are to be removed
		 * from the pvalue.
		 *
		 * Update: this seems to be unnecessary and may cause
		 * parsing error for cases such as IPv6 reference with
		 * port number.
		 */
		// pj_scan_get_char(scanner);
		// pj_scan_get_until_ch(scanner, ']', pvalue);
		// pj_scan_get_char(scanner);
	    } else if(pj_cis_match(spec, *scanner->curptr)) {
	    	if (!esc_spec) {
    		    pj_scan_get(scanner, spec, pvalue);
    		} else {
		    parser_get_and_unescape(scanner, pool, spec, esc_spec,
		    			    pvalue);
		}
	    }
	}
    }
}

/* Parse parameter (pname ["=" pvalue]) using token. */
PJ_DEF(void) pjsip_parse_param_imp(pj_scanner *scanner, pj_pool_t *pool,
			     	   pj_str_t *pname, pj_str_t *pvalue,
			     	   unsigned option)
{
    parse_param_imp(scanner, pool, pname, pvalue, &pconst.pjsip_TOKEN_SPEC,
		    // Token does not need to be unescaped.
		    // Refer to PR #2933.
		    // &pconst.pjsip_TOKEN_SPEC_ESC,
		    NULL, option);
}


/* Parse parameter (pname ["=" pvalue]) using paramchar. */
PJ_DEF(void) pjsip_parse_uri_param_imp( pj_scanner *scanner, pj_pool_t *pool,
			       		pj_str_t *pname, pj_str_t *pvalue,
			       		unsigned option)
{
    parse_param_imp(scanner,pool, pname, pvalue, &pconst.pjsip_PARAM_CHAR_SPEC,
		    &pconst.pjsip_PARAM_CHAR_SPEC_ESC, option);
}


/* Parse parameter (";" pname ["=" pvalue]) in SIP header. */
static void int_parse_param( pj_scanner *scanner, pj_pool_t *pool,
			     pj_str_t *pname, pj_str_t *pvalue,
			     unsigned option)
{
    /* Get ';' character */
    pj_scan_get_char(scanner);

    /* Get pname and optionally pvalue */
    pjsip_parse_param_imp(scanner, pool, pname, pvalue, option);
}

/* Parse parameter (";" pname ["=" pvalue]) in URI. */
static void int_parse_uri_param( pj_scanner *scanner, pj_pool_t *pool,
				 pj_str_t *pname, pj_str_t *pvalue,
				 unsigned option)
{
    /* Get ';' character */
    pj_scan_get_char(scanner);

    /* Get pname and optionally pvalue */
    pjsip_parse_uri_param_imp(scanner, pool, pname, pvalue, 
			      option);
}


/* Parse header parameter. */
static void int_parse_hparam( pj_scanner *scanner, pj_pool_t *pool,
			      pj_str_t *hname, pj_str_t *hvalue )
{
    /* Get '?' or '&' character. */
    pj_scan_get_char(scanner);

    /* hname */
    parser_get_and_unescape(scanner, pool, &pconst.pjsip_HDR_CHAR_SPEC, 
			    &pconst.pjsip_HDR_CHAR_SPEC_ESC, hname);

    /* Init hvalue */
    hvalue->ptr = NULL;
    hvalue->slen = 0;

    /* pvalue, if any */
    if (*scanner->curptr == '=') {
	pj_scan_get_char(scanner);
	if (!pj_scan_is_eof(scanner) && 
	    pj_cis_match(&pconst.pjsip_HDR_CHAR_SPEC, *scanner->curptr))
	{
	    parser_get_and_unescape(scanner, pool, &pconst.pjsip_HDR_CHAR_SPEC,
				    &pconst.pjsip_HDR_CHAR_SPEC_ESC, hvalue);
	}
    }
}

/* Parse host part:
 *   host =  hostname / IPv4address / IPv6reference
 */
static void int_parse_host(pj_scanner *scanner, pj_str_t *host)
{
    if (*scanner->curptr == '[') {
	/* Note: the '[' and ']' characters are removed from the host */
	pj_scan_get_char(scanner);
	pj_scan_get_until_ch(scanner, ']', host);
	pj_scan_get_char(scanner);
    } else {
	pj_scan_get( scanner, &pconst.pjsip_HOST_SPEC, host);
    }
}

/* Parse host:port in URI. */
static void int_parse_uri_host_port( pj_scanner *scanner, 
				     pj_str_t *host, int *p_port)
{
    int_parse_host(scanner, host);

    /* RFC3261 section 19.1.2: host don't need to be unescaped */
    if (*scanner->curptr == ':') {
	pj_str_t port;
	pj_scan_get_char(scanner);
	pj_scan_get(scanner, &pconst.pjsip_DIGIT_SPEC, &port);
	strtoi_validate(&port, PJSIP_MIN_PORT, PJSIP_MAX_PORT, p_port);
    } else {
	*p_port = 0;
    }
}

/* Determine if the next token in an URI is a user specification. */
static int int_is_next_user(pj_scanner *scanner)
{
    pj_str_t dummy;
    int is_user;

    /* Find character '@'. If this character exist, then the token
     * must be a username.
     */
    if (pj_scan_peek( scanner, &pconst.pjsip_PROBE_USER_HOST_SPEC, &dummy) == '@')
	is_user = 1;
    else
	is_user = 0;

    return is_user;
}

/* Parse user:pass tokens in an URI. */
static void int_parse_user_pass( pj_scanner *scanner, pj_pool_t *pool,
				 pj_str_t *user, pj_str_t *pass)
{
    parser_get_and_unescape(scanner, pool, &pconst.pjsip_USER_SPEC_LENIENT, 
			    &pconst.pjsip_USER_SPEC_LENIENT_ESC, user);

    if ( *scanner->curptr == ':') {
	pj_scan_get_char( scanner );
	parser_get_and_unescape(scanner, pool, &pconst.pjsip_PASSWD_SPEC,
				&pconst.pjsip_PASSWD_SPEC_ESC, pass);
    } else {
	pass->ptr = NULL;
	pass->slen = 0;
    }

    /* Get the '@' */
    pj_scan_get_char( scanner );
}

/* Parse all types of URI. */
static pjsip_uri *int_parse_uri_or_name_addr( pj_scanner *scanner, pj_pool_t *pool,
					      unsigned opt)
{
    pjsip_uri *uri;
    int is_name_addr = 0;

    /* Exhaust any whitespaces. */
    pj_scan_skip_whitespace(scanner);

    if (*scanner->curptr=='"' || *scanner->curptr=='<') {
	uri = (pjsip_uri*)int_parse_name_addr( scanner, pool );
	is_name_addr = 1;
    } else {
	pj_str_t scheme;
	int next_ch;

	next_ch = pj_scan_peek( scanner, &pconst.pjsip_DISPLAY_SPEC, &scheme);

	if (next_ch==':') {
	    pjsip_parse_uri_func *func = find_uri_handler(&scheme);

	    if (func == NULL) {
		/* Unsupported URI scheme */
		PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);
	    }

	    uri = (pjsip_uri*)
	    	  (*func)(scanner, pool, 
			  (opt & PJSIP_PARSE_URI_IN_FROM_TO_HDR)==0);


	} else {
	    uri = (pjsip_uri*)int_parse_name_addr( scanner, pool );
	    is_name_addr = 1;
	}
    }

    /* Should we return the URI object as name address? */
    if (opt & PJSIP_PARSE_URI_AS_NAMEADDR) {
	if (is_name_addr == 0) {
	    pjsip_name_addr *name_addr;

	    name_addr = pjsip_name_addr_create(pool);
	    name_addr->uri = uri;

	    uri = (pjsip_uri*)name_addr;
	}
    }

    return uri;
}

/* Parse URI. */
static pjsip_uri *int_parse_uri(pj_scanner *scanner, pj_pool_t *pool, 
				pj_bool_t parse_params)
{
    /* Bug:
     * This function should not call back int_parse_name_addr() because
     * it is called by that function. This would cause stack overflow
     * with PROTOS test #1223.
    if (*scanner->curptr=='"' || *scanner->curptr=='<') {
	return (pjsip_uri*)int_parse_name_addr( scanner, pool );
    } else {
    */
	pj_str_t scheme;
	int colon;
	pjsip_parse_uri_func *func;

	/* Get scheme. */
	colon = pj_scan_peek(scanner, &pconst.pjsip_TOKEN_SPEC, &scheme);
	if (colon != ':') {
	    PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);
	}

	func = find_uri_handler(&scheme);
	if (func)  {
	    return (pjsip_uri*)(*func)(scanner, pool, parse_params);

	} else {
	    /* Unsupported URI scheme */
	    PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);
	    UNREACHED({ return NULL; /* Not reached. */ })
	}

    /*
    }
    */
}

/* Parse "sip:" and "sips:" URI. 
 * This actually returns (pjsip_sip_uri*) type,
 */
static void* int_parse_sip_url( pj_scanner *scanner, 
				pj_pool_t *pool,
				pj_bool_t parse_params)
{
    pj_str_t scheme;
    pjsip_sip_uri *url = NULL;
    int colon;
    int skip_ws = scanner->skip_ws;
    scanner->skip_ws = 0;

    pj_scan_get(scanner, &pconst.pjsip_TOKEN_SPEC, &scheme);
    colon = pj_scan_get_char(scanner);
    if (colon != ':') {
	PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);
    }

    if (parser_stricmp(scheme, pconst.pjsip_SIP_STR)==0) {
	url = pjsip_sip_uri_create(pool, 0);

    } else if (parser_stricmp(scheme, pconst.pjsip_SIPS_STR)==0) {
	url = pjsip_sip_uri_create(pool, 1);

    } else {
	PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);
	/* should not reach here */
	UNREACHED({
	    pj_assert(0);
	    return 0;
	})
    }

    if (int_is_next_user(scanner)) {
	int_parse_user_pass(scanner, pool, &url->user, &url->passwd);
    }

    /* Get host:port */
    int_parse_uri_host_port(scanner, &url->host, &url->port);

    /* Get URL parameters. */
    if (parse_params) {
      while (*scanner->curptr == ';' ) {
	pj_str_t pname, pvalue;

	int_parse_uri_param( scanner, pool, &pname, &pvalue, 0);

	if (!parser_stricmp(pname, pconst.pjsip_USER_STR) && pvalue.slen) {
	    url->user_param = pvalue;

	} else if (!parser_stricmp(pname, pconst.pjsip_METHOD_STR) && pvalue.slen) {
	    url->method_param = pvalue;

	} else if (!parser_stricmp(pname, pconst.pjsip_TRANSPORT_STR) && pvalue.slen) {
	    url->transport_param = pvalue;

	} else if (!parser_stricmp(pname, pconst.pjsip_TTL_STR) && pvalue.slen) {
	    strtoi_validate(&pvalue, PJSIP_MIN_TTL, PJSIP_MAX_TTL,
			    &url->ttl_param);
	} else if (!parser_stricmp(pname, pconst.pjsip_MADDR_STR) && pvalue.slen) {
	    url->maddr_param = pvalue;

	} else if (!parser_stricmp(pname, pconst.pjsip_LR_STR)) {
	    url->lr_param = 1;

	} else {
	    pjsip_param *p = PJ_POOL_ALLOC_T(pool, pjsip_param);
	    p->name = pname;
	    p->value = pvalue;
	    pj_list_insert_before(&url->other_param, p);
	}
      }
    }

    /* Get header params. */
    if (parse_params && *scanner->curptr == '?') {
      do {
	pjsip_param *param;
	param = PJ_POOL_ALLOC_T(pool, pjsip_param);
	int_parse_hparam(scanner, pool, &param->name, &param->value);
	pj_list_insert_before(&url->header_param, param);
      } while (*scanner->curptr == '&');
    }

    scanner->skip_ws = skip_ws;
    pj_scan_skip_whitespace(scanner);
    return url;
}

/* Parse nameaddr. */
static pjsip_name_addr *int_parse_name_addr( pj_scanner *scanner, 
					     pj_pool_t *pool )
{
    int has_bracket;
    pjsip_name_addr *name_addr;

    name_addr = pjsip_name_addr_create(pool);

    if (*scanner->curptr == '"') {
	pj_scan_get_quote( scanner, '"', '"', &name_addr->display);
	/* Trim the leading and ending quote */
	name_addr->display.ptr++;
	name_addr->display.slen -= 2;

    } else if (*scanner->curptr != '<') {
	int next;
	pj_str_t dummy;

	/* This can be either the start of display name,
	 * the start of URL ("sip:", "sips:", "tel:", etc.), or '<' char.
	 * We're only interested in display name, because SIP URL
	 * will be parser later.
	 */
	next = pj_scan_peek(scanner, &pconst.pjsip_DISPLAY_SPEC, &dummy);
	if (next == '<') {
	    /* Ok, this is what we're looking for, a display name. */
	    pj_scan_get_until_ch( scanner, '<', &name_addr->display);
	    pj_strtrim(&name_addr->display);
	}
    }

    /* Manually skip whitespace. */
    pj_scan_skip_whitespace(scanner);

    /* Get the SIP-URL */
    has_bracket = (*scanner->curptr == '<');
    if (has_bracket) {
	pj_scan_get_char(scanner);
    } else if (name_addr->display.slen) {
	/* Must have bracket now (2012-10-26).
	 * Allowing (invalid) name-addr to pass URI verification will
	 * cause us to send invalid URI to the wire.
	 */
	PJ_THROW( PJSIP_SYN_ERR_EXCEPTION);
    }
    name_addr->uri = int_parse_uri( scanner, pool, PJ_TRUE );
    if (has_bracket) {
	if (pj_scan_get_char(scanner) != '>')
	    PJ_THROW( PJSIP_SYN_ERR_EXCEPTION);
    }

    return name_addr;
}


/* Parse other URI */
static void* int_parse_other_uri(pj_scanner *scanner, 
				 pj_pool_t *pool,
				 pj_bool_t parse_params)
{
    pjsip_other_uri *uri = 0;
    const pjsip_parser_const_t *pc = pjsip_parser_const();
    int skip_ws = scanner->skip_ws;

    PJ_UNUSED_ARG(parse_params);

    scanner->skip_ws = 0;
    
    uri = pjsip_other_uri_create(pool); 
    
    pj_scan_get(scanner, &pc->pjsip_TOKEN_SPEC, &uri->scheme);
    if (pj_scan_get_char(scanner) != ':') {
	PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);
    }
    
    pj_scan_get(scanner, &pc->pjsip_OTHER_URI_CONTENT, &uri->content);
    scanner->skip_ws = skip_ws;
    
    return uri;
}


/* Parse SIP request line. */
static void int_parse_req_line( pj_scanner *scanner, pj_pool_t *pool,
				pjsip_request_line *req_line)
{
    pj_str_t token;

    pj_scan_get( scanner, &pconst.pjsip_TOKEN_SPEC, &token);
    pjsip_method_init_np( &req_line->method, &token);

    req_line->uri = int_parse_uri(scanner, pool, PJ_TRUE);
    parse_sip_version(scanner);
    pj_scan_get_newline( scanner );
}

/* Parse status line. */
static void int_parse_status_line( pj_scanner *scanner, 
				   pjsip_status_line *status_line)
{
    pj_str_t token;

    parse_sip_version(scanner);
    pj_scan_get( scanner, &pconst.pjsip_DIGIT_SPEC, &token);
    strtoi_validate(&token, PJSIP_MIN_STATUS_CODE, PJSIP_MAX_STATUS_CODE,
                    &status_line->code);
    if (*scanner->curptr != '\r' && *scanner->curptr != '\n')
	pj_scan_get( scanner, &pconst.pjsip_NOT_NEWLINE, &status_line->reason);
    else
	status_line->reason.slen=0, status_line->reason.ptr=NULL;
    pj_scan_get_newline( scanner );
}


/*
 * Public API to parse SIP status line.
 */
PJ_DEF(pj_status_t) pjsip_parse_status_line( char *buf, pj_size_t size,
					     pjsip_status_line *status_line)
{
    pj_scanner scanner;
    PJ_USE_EXCEPTION;

    pj_bzero(status_line, sizeof(*status_line));
    pj_scan_init(&scanner, buf, size, PJ_SCAN_AUTOSKIP_WS_HEADER, 
		 &on_syntax_error);

    PJ_TRY {
	int_parse_status_line(&scanner, status_line);
    } 
    PJ_CATCH_ANY {
	/* Tolerate the error if it is caused only by missing newline */
	if (status_line->code == 0 && status_line->reason.slen == 0) {
	    pj_scan_fini(&scanner);
	    return PJSIP_EINVALIDMSG;
	}
    }
    PJ_END;

    pj_scan_fini(&scanner);
    return PJ_SUCCESS;
}


/* Parse ending of header. */
static void parse_hdr_end( pj_scanner *scanner )
{
    if (pj_scan_is_eof(scanner)) {
	;   /* Do nothing. */
    } else if (*scanner->curptr == '&') {
	pj_scan_get_char(scanner);
    } else {
	pj_scan_get_newline(scanner);
    }
}

/* Parse ending of header. */
PJ_DEF(void) pjsip_parse_end_hdr_imp( pj_scanner *scanner )
{
    parse_hdr_end(scanner);
}

/* Parse generic array header. */
static void parse_generic_array_hdr( pjsip_generic_array_hdr *hdr,
				     pj_scanner *scanner)
{
    /* Some header fields allow empty elements in the value:
     *   Accept, Allow, Supported
     */
    if (pj_scan_is_eof(scanner) || 
	*scanner->curptr == '\r' || *scanner->curptr == '\n') 
    {
	goto end;
    }

    if (hdr->count >= PJ_ARRAY_SIZE(hdr->values)) {
	/* Too many elements */
	on_syntax_error(scanner);
	return;
    }

    pj_scan_get( scanner, &pconst.pjsip_NOT_COMMA_OR_NEWLINE, 
		 &hdr->values[hdr->count]);
    hdr->count++;

    while ((hdr->count < PJSIP_GENERIC_ARRAY_MAX_COUNT) &&
    	   (*scanner->curptr == ','))
    {
	pj_scan_get_char(scanner);
	pj_scan_get( scanner, &pconst.pjsip_NOT_COMMA_OR_NEWLINE, 
		     &hdr->values[hdr->count]);
	hdr->count++;
    }

end:
    parse_hdr_end(scanner);
}

/* Parse generic array header. */
PJ_DEF(void) pjsip_parse_generic_array_hdr_imp( pjsip_generic_array_hdr *hdr,
						pj_scanner *scanner)
{
    parse_generic_array_hdr(hdr, scanner);
}


/* Parse generic string header. */
static void parse_generic_string_hdr( pjsip_generic_string_hdr *hdr,
				      pjsip_parse_ctx *ctx)
{
    pj_scanner *scanner = ctx->scanner;

    hdr->hvalue.slen = 0;

    /* header may be mangled hence the loop */
    while (pj_cis_match(&pconst.pjsip_NOT_NEWLINE, *scanner->curptr)) {
	pj_str_t next, tmp;

	pj_scan_get( scanner, &pconst.pjsip_NOT_NEWLINE, &hdr->hvalue);
	if (pj_scan_is_eof(scanner) || IS_NEWLINE(*scanner->curptr))
	    break;
	/* mangled, get next fraction */
	pj_scan_get( scanner, &pconst.pjsip_NOT_NEWLINE, &next);
	/* concatenate */
	tmp.ptr = (char*)pj_pool_alloc(ctx->pool, 
				       hdr->hvalue.slen + next.slen + 2);
	tmp.slen = 0;
	pj_strcpy(&tmp, &hdr->hvalue);
	pj_strcat2(&tmp, " ");
	pj_strcat(&tmp, &next);
	tmp.ptr[tmp.slen] = '\0';

	hdr->hvalue = tmp;
    }

    parse_hdr_end(scanner);
}

/* Parse generic integer header. */
static void parse_generic_int_hdr( pjsip_generic_int_hdr *hdr,
				   pj_scanner *scanner )
{
    pj_str_t tmp;
    pj_scan_get( scanner, &pconst.pjsip_DIGIT_SPEC, &tmp);
    hdr->ivalue = pj_strtoul(&tmp);
    parse_hdr_end(scanner);
}


/* Parse Accept header. */
static pjsip_hdr* parse_hdr_accept(pjsip_parse_ctx *ctx)
{
    pjsip_accept_hdr *accept = pjsip_accept_hdr_create(ctx->pool);
    parse_generic_array_hdr(accept, ctx->scanner);
    return (pjsip_hdr*)accept;
}

/* Parse Allow header. */
static pjsip_hdr* parse_hdr_allow(pjsip_parse_ctx *ctx)
{
    pjsip_allow_hdr *allow = pjsip_allow_hdr_create(ctx->pool);
    parse_generic_array_hdr(allow, ctx->scanner);
    return (pjsip_hdr*)allow;
}

/* Parse Call-ID header. */
static pjsip_hdr* parse_hdr_call_id(pjsip_parse_ctx *ctx)
{
    pjsip_cid_hdr *hdr = pjsip_cid_hdr_create(ctx->pool);
    pj_scan_get( ctx->scanner, &pconst.pjsip_NOT_NEWLINE, &hdr->id);
    parse_hdr_end(ctx->scanner);

    if (ctx->rdata)
        ctx->rdata->msg_info.cid = hdr;

    return (pjsip_hdr*)hdr;
}

/* Parse and interpret Contact param. */
static void int_parse_contact_param( pjsip_contact_hdr *hdr, 
				     pj_scanner *scanner,
				     pj_pool_t *pool)
{
    while ( *scanner->curptr == ';' ) {
	pj_str_t pname, pvalue;

	int_parse_param( scanner, pool, &pname, &pvalue, 0);
	if (!parser_stricmp(pname, pconst.pjsip_Q_STR) && pvalue.slen) {
	    char *dot_pos = (char*) pj_memchr(pvalue.ptr, '.', pvalue.slen);
	    if (!dot_pos) {
		strtoi_validate(&pvalue, PJSIP_MIN_Q1000, PJSIP_MAX_Q1000,
                                &hdr->q1000);
		hdr->q1000 *= 1000;
	    } else {
		pj_str_t tmp = pvalue;
		unsigned long qval_frac;

		tmp.slen = dot_pos - pvalue.ptr;
		strtoi_validate(&tmp, PJSIP_MIN_Q1000, PJSIP_MAX_Q1000,
                                &hdr->q1000);
                hdr->q1000 *= 1000;

		pvalue.slen = (pvalue.ptr+pvalue.slen) - (dot_pos+1);
		pvalue.ptr = dot_pos + 1;
		if (pvalue.slen > 3) {
		    pvalue.slen = 3;
		}
		qval_frac = pj_strtoul_mindigit(&pvalue, 3);
		if ((unsigned)hdr->q1000 > (PJ_MAXINT32 - qval_frac)) {
		    PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);
		}
		hdr->q1000 += qval_frac;
	    }    
	} else if (!parser_stricmp(pname, pconst.pjsip_EXPIRES_STR) && 
                   pvalue.slen) 
        {
            hdr->expires = pj_strtoul(&pvalue);
            if (hdr->expires == PJSIP_EXPIRES_NOT_SPECIFIED)
        	hdr->expires--;
            if (hdr->expires > PJSIP_MAX_EXPIRES)
            	hdr->expires = PJSIP_MAX_EXPIRES;
            if (hdr->expires < PJSIP_MIN_EXPIRES)
            	hdr->expires = PJSIP_MIN_EXPIRES;
	} else {
	    pjsip_param *p = PJ_POOL_ALLOC_T(pool, pjsip_param);
	    p->name = pname;
	    p->value = pvalue;
	    pj_list_insert_before(&hdr->other_param, p);
	}
    }
}

/* Parse Contact header. */
static pjsip_hdr* parse_hdr_contact( pjsip_parse_ctx *ctx )
{
    pjsip_contact_hdr *first = NULL;
    pj_scanner *scanner = ctx->scanner;
    
    do {
	pjsip_contact_hdr *hdr = pjsip_contact_hdr_create(ctx->pool);
	if (first == NULL)
	    first = hdr;
	else
	    pj_list_insert_before(first, hdr);

	if (*scanner->curptr == '*') {
	    pj_scan_get_char(scanner);
	    hdr->star = 1;

	} else {
	    hdr->star = 0;
	    hdr->uri = int_parse_uri_or_name_addr(scanner, ctx->pool, 
                                                  PJSIP_PARSE_URI_AS_NAMEADDR |
						  PJSIP_PARSE_URI_IN_FROM_TO_HDR);

	    int_parse_contact_param(hdr, scanner, ctx->pool);
	}

	if (*scanner->curptr != ',')
	    break;

	pj_scan_get_char(scanner);

    } while (1);

    parse_hdr_end(scanner);

    return (pjsip_hdr*)first;
}

/* Parse Content-Length header. */
static pjsip_hdr* parse_hdr_content_len( pjsip_parse_ctx *ctx )
{
    pj_str_t digit;
    pjsip_clen_hdr *hdr;

    hdr = pjsip_clen_hdr_create(ctx->pool);
    pj_scan_get(ctx->scanner, &pconst.pjsip_DIGIT_SPEC, &digit);
    hdr->len = pj_strtoul(&digit);
    parse_hdr_end(ctx->scanner);

    if (ctx->rdata)
        ctx->rdata->msg_info.clen = hdr;

    return (pjsip_hdr*)hdr;
}

/* Parse Content-Type header. */
static pjsip_hdr* parse_hdr_content_type( pjsip_parse_ctx *ctx )
{
    pjsip_ctype_hdr *hdr;
    pj_scanner *scanner = ctx->scanner;

    hdr = pjsip_ctype_hdr_create(ctx->pool);
    
    /* Parse media type and subtype. */
    pj_scan_get(scanner, &pconst.pjsip_TOKEN_SPEC, &hdr->media.type);
    pj_scan_get_char(scanner);
    pj_scan_get(scanner, &pconst.pjsip_TOKEN_SPEC, &hdr->media.subtype);

    /* Parse media parameters */
    while (*scanner->curptr == ';') {
	pjsip_param *param = PJ_POOL_ALLOC_T(ctx->pool, pjsip_param);
	int_parse_param(scanner, ctx->pool, &param->name, &param->value, 0);
	pj_list_push_back(&hdr->media.param, param);
    }

    parse_hdr_end(ctx->scanner);

    if (ctx->rdata)
        ctx->rdata->msg_info.ctype = hdr;

    return (pjsip_hdr*)hdr;
}

/* Parse CSeq header. */
static pjsip_hdr* parse_hdr_cseq( pjsip_parse_ctx *ctx )
{
    pj_str_t cseq, method;
    pjsip_cseq_hdr *hdr = NULL;
    int cseq_val = 0;

    pj_scan_get( ctx->scanner, &pconst.pjsip_DIGIT_SPEC, &cseq);
    strtoi_validate(&cseq, PJSIP_MIN_CSEQ, PJSIP_MAX_CSEQ, &cseq_val);

    hdr = pjsip_cseq_hdr_create(ctx->pool);
    hdr->cseq = cseq_val;

    pj_scan_get( ctx->scanner, &pconst.pjsip_TOKEN_SPEC, &method);
    parse_hdr_end( ctx->scanner );

    pjsip_method_init_np(&hdr->method, &method);
    if (ctx->rdata) {
        ctx->rdata->msg_info.cseq = hdr;
    }

    return (pjsip_hdr*)hdr;
}

/* Parse Expires header. */
static pjsip_hdr* parse_hdr_expires(pjsip_parse_ctx *ctx)
{
    pjsip_expires_hdr *hdr = pjsip_expires_hdr_create(ctx->pool, 0);
    parse_generic_int_hdr(hdr, ctx->scanner);
    return (pjsip_hdr*)hdr;
}

/* Parse From: or To: header. */
static void parse_hdr_fromto( pj_scanner *scanner, 
			      pj_pool_t *pool, 
			      pjsip_from_hdr *hdr)
{
    hdr->uri = int_parse_uri_or_name_addr(scanner, pool, 
					  PJSIP_PARSE_URI_AS_NAMEADDR |
					  PJSIP_PARSE_URI_IN_FROM_TO_HDR);

    while ( *scanner->curptr == ';' ) {
	pj_str_t pname, pvalue;

	int_parse_param( scanner, pool, &pname, &pvalue, 0);

	if (!parser_stricmp(pname, pconst.pjsip_TAG_STR)) {
	    hdr->tag = pvalue;
	    
	} else {
	    pjsip_param *p = PJ_POOL_ALLOC_T(pool, pjsip_param);
	    p->name = pname;
	    p->value = pvalue;
	    pj_list_insert_before(&hdr->other_param, p);
	}
    }

    parse_hdr_end(scanner);
}

/* Parse From: header. */
static pjsip_hdr* parse_hdr_from( pjsip_parse_ctx *ctx )
{
    pjsip_from_hdr *hdr = pjsip_from_hdr_create(ctx->pool);
    parse_hdr_fromto(ctx->scanner, ctx->pool, hdr);
    if (ctx->rdata)
        ctx->rdata->msg_info.from = hdr;

    return (pjsip_hdr*)hdr;
}

/* Parse Require: header. */
static pjsip_hdr* parse_hdr_require( pjsip_parse_ctx *ctx )
{
    pjsip_require_hdr *hdr;
    pj_bool_t new_hdr = (ctx->rdata==NULL ||
			 ctx->rdata->msg_info.require == NULL);
    
    if (ctx->rdata && ctx->rdata->msg_info.require) {
	hdr = ctx->rdata->msg_info.require;
    } else {
	hdr = pjsip_require_hdr_create(ctx->pool);
	if (ctx->rdata)
	    ctx->rdata->msg_info.require = hdr;
    }

    parse_generic_array_hdr(hdr, ctx->scanner);

    return new_hdr ? (pjsip_hdr*)hdr : NULL;
}

/* Parse Retry-After: header. */
static pjsip_hdr* parse_hdr_retry_after(pjsip_parse_ctx *ctx)
{
    pjsip_retry_after_hdr *hdr;
    pj_scanner *scanner = ctx->scanner;
    pj_str_t tmp;

    hdr = pjsip_retry_after_hdr_create(ctx->pool, 0);
    
    pj_scan_get(scanner, &pconst.pjsip_DIGIT_SPEC, &tmp);
    strtoi_validate(&tmp, PJSIP_MIN_RETRY_AFTER, PJSIP_MAX_RETRY_AFTER,
                    &hdr->ivalue);

    while (!pj_scan_is_eof(scanner) && *scanner->curptr!='\r' &&
	   *scanner->curptr!='\n')
    {
	if (*scanner->curptr=='(') {
	    pj_scan_get_quote(scanner, '(', ')', &hdr->comment);
	    /* Trim the leading and ending parens */
	    hdr->comment.ptr++;
	    hdr->comment.slen -= 2;
	} else if (*scanner->curptr==';') {
	    pjsip_param *prm = PJ_POOL_ALLOC_T(ctx->pool, pjsip_param);
	    int_parse_param(scanner, ctx->pool, &prm->name, &prm->value, 0);
	    pj_list_push_back(&hdr->param, prm);
	} else {
	    on_syntax_error(scanner);
	}
    }

    parse_hdr_end(scanner);
    return (pjsip_hdr*)hdr;
}

/* Parse Supported: header. */
static pjsip_hdr* parse_hdr_supported(pjsip_parse_ctx *ctx)
{
    pjsip_supported_hdr *hdr;
    pj_bool_t new_hdr = (ctx->rdata==NULL || 
		         ctx->rdata->msg_info.supported == NULL);

    if (ctx->rdata && ctx->rdata->msg_info.supported) {
	hdr = ctx->rdata->msg_info.supported;
    } else {
	hdr = pjsip_supported_hdr_create(ctx->pool);
	if (ctx->rdata)
	    ctx->rdata->msg_info.supported = hdr;
    }

    parse_generic_array_hdr(hdr, ctx->scanner);
    return new_hdr ? (pjsip_hdr*)hdr : NULL;
}

/* Parse To: header. */
static pjsip_hdr* parse_hdr_to( pjsip_parse_ctx *ctx )
{
    pjsip_to_hdr *hdr = pjsip_to_hdr_create(ctx->pool);
    parse_hdr_fromto(ctx->scanner, ctx->pool, hdr);

    if (ctx->rdata)
        ctx->rdata->msg_info.to = hdr;

    return (pjsip_hdr*)hdr;
}

/* Parse Unsupported: header. */
static pjsip_hdr* parse_hdr_unsupported(pjsip_parse_ctx *ctx)
{
    pjsip_unsupported_hdr *hdr = pjsip_unsupported_hdr_create(ctx->pool);
    parse_generic_array_hdr(hdr, ctx->scanner);
    return (pjsip_hdr*)hdr;
}

/* Parse and interpret Via parameters. */
static void int_parse_via_param( pjsip_via_hdr *hdr, pj_scanner *scanner,
				 pj_pool_t *pool)
{
    while ( *scanner->curptr == ';' ) {
	pj_str_t pname, pvalue;

	//Parse with PARAM_CHAR instead, to allow IPv6
	//No, back to using int_parse_param() for the "`" character!
	//int_parse_param( scanner, pool, &pname, &pvalue, 0);
	//parse_param_imp(scanner, pool, &pname, &pvalue, 
	//		&pconst.pjsip_TOKEN_SPEC,
	//		&pconst.pjsip_TOKEN_SPEC_ESC, 0);
	//int_parse_param(scanner, pool, &pname, &pvalue, 0);
	// This should be the correct one:
	//  added special spec for Via parameter, basically token plus
	//  ":" to allow IPv6 address in the received param.
	pj_scan_get_char(scanner);
	parse_param_imp(scanner, pool, &pname, &pvalue,
			&pconst.pjsip_VIA_PARAM_SPEC,
		    	// Token does not need to be unescaped.
		     	// Refer to PR #2933.
		    	// &pconst.pjsip_VIA_PARAM_SPEC_ESC,
			NULL,
			0);

	if (!parser_stricmp(pname, pconst.pjsip_BRANCH_STR) && pvalue.slen) {
	    hdr->branch_param = pvalue;

	} else if (!parser_stricmp(pname, pconst.pjsip_TTL_STR) && pvalue.slen) {
	    strtoi_validate(&pvalue, PJSIP_MIN_TTL, PJSIP_MAX_TTL,
                            &hdr->ttl_param);
	    
	} else if (!parser_stricmp(pname, pconst.pjsip_MADDR_STR) && pvalue.slen) {
	    hdr->maddr_param = pvalue;

	} else if (!parser_stricmp(pname, pconst.pjsip_RECEIVED_STR) && pvalue.slen) {
	    hdr->recvd_param = pvalue;

	} else if (!parser_stricmp(pname, pconst.pjsip_RPORT_STR)) {
	    if (pvalue.slen) {
		strtoi_validate(&pvalue, PJSIP_MIN_PORT, PJSIP_MAX_PORT,
			        &hdr->rport_param);
            } else
		hdr->rport_param = 0;
	} else {
	    pjsip_param *p = PJ_POOL_ALLOC_T(pool, pjsip_param);
	    p->name = pname;
	    p->value = pvalue;
	    pj_list_insert_before(&hdr->other_param, p);
	}
    }
}

/* Parse Max-Forwards header. */
static pjsip_hdr* parse_hdr_max_forwards( pjsip_parse_ctx *ctx )
{
    pjsip_max_fwd_hdr *hdr;
    hdr = pjsip_max_fwd_hdr_create(ctx->pool, 0);
    parse_generic_int_hdr(hdr, ctx->scanner);

    if (ctx->rdata)
        ctx->rdata->msg_info.max_fwd = hdr;

    return (pjsip_hdr*)hdr;
}

/* Parse Min-Expires header. */
static pjsip_hdr* parse_hdr_min_expires(pjsip_parse_ctx *ctx)
{
    pjsip_min_expires_hdr *hdr;
    hdr = pjsip_min_expires_hdr_create(ctx->pool, 0);
    parse_generic_int_hdr(hdr, ctx->scanner);
    return (pjsip_hdr*)hdr;
}


/* Parse Route: or Record-Route: header. */
static void parse_hdr_rr_route( pj_scanner *scanner, pj_pool_t *pool,
				pjsip_routing_hdr *hdr )
{
    pjsip_name_addr *temp=int_parse_name_addr(scanner, pool);

    pj_memcpy(&hdr->name_addr, temp, sizeof(*temp));

    while (*scanner->curptr == ';') {
	pjsip_param *p = PJ_POOL_ALLOC_T(pool, pjsip_param);
	int_parse_param(scanner, pool, &p->name, &p->value, 0);
	pj_list_insert_before(&hdr->other_param, p);
    }
}

/* Parse Record-Route header. */
static pjsip_hdr* parse_hdr_rr( pjsip_parse_ctx *ctx)
{
    pjsip_rr_hdr *first = NULL;
    pj_scanner *scanner = ctx->scanner;

    do {
	pjsip_rr_hdr *hdr = pjsip_rr_hdr_create(ctx->pool);
	if (!first) {
	    first = hdr;
	} else {
	    pj_list_insert_before(first, hdr);
	}
	parse_hdr_rr_route(scanner, ctx->pool, hdr);
	if (*scanner->curptr == ',') {
	    pj_scan_get_char(scanner);
	} else {
	    break;
	}
    } while (1);
    parse_hdr_end(scanner);

    if (ctx->rdata && ctx->rdata->msg_info.record_route==NULL)
        ctx->rdata->msg_info.record_route = first;

    return (pjsip_hdr*)first;
}

/* Parse Route: header. */
static pjsip_hdr* parse_hdr_route( pjsip_parse_ctx *ctx )
{
    pjsip_route_hdr *first = NULL;
    pj_scanner *scanner = ctx->scanner;

    do {
	pjsip_route_hdr *hdr = pjsip_route_hdr_create(ctx->pool);
	if (!first) {
	    first = hdr;
	} else {
	    pj_list_insert_before(first, hdr);
	}
	parse_hdr_rr_route(scanner, ctx->pool, hdr);
	if (*scanner->curptr == ',') {
	    pj_scan_get_char(scanner);
	} else {
	    break;
	}
    } while (1);
    parse_hdr_end(scanner);

    if (ctx->rdata && ctx->rdata->msg_info.route==NULL)
        ctx->rdata->msg_info.route = first;

    return (pjsip_hdr*)first;
}

/* Parse Via: header. */
static pjsip_hdr* parse_hdr_via( pjsip_parse_ctx *ctx )
{
    pjsip_via_hdr *first = NULL;
    pj_scanner *scanner = ctx->scanner;

    do {
	pjsip_via_hdr *hdr = pjsip_via_hdr_create(ctx->pool);
	if (!first)
	    first = hdr;
	else
	    pj_list_insert_before(first, hdr);

	parse_sip_version(scanner);
	if (pj_scan_get_char(scanner) != '/')
	    on_syntax_error(scanner);

	pj_scan_get( scanner, &pconst.pjsip_TOKEN_SPEC, &hdr->transport);
	int_parse_host(scanner, &hdr->sent_by.host);

	if (*scanner->curptr==':') {
	    pj_str_t digit;
	    pj_scan_get_char(scanner);
	    pj_scan_get(scanner, &pconst.pjsip_DIGIT_SPEC, &digit);
	    strtoi_validate(&digit, PJSIP_MIN_PORT, PJSIP_MAX_PORT,
                            &hdr->sent_by.port);
	}
	
	int_parse_via_param(hdr, scanner, ctx->pool);

	if (*scanner->curptr == '(') {
	    pj_scan_get_char(scanner);
	    pj_scan_get_until_ch( scanner, ')', &hdr->comment);
	    pj_scan_get_char( scanner );
	}

	if (*scanner->curptr != ',')
	    break;

	pj_scan_get_char(scanner);

    } while (1);

    parse_hdr_end(scanner);

    if (ctx->rdata && ctx->rdata->msg_info.via == NULL)
        ctx->rdata->msg_info.via = first;

    return (pjsip_hdr*)first;
}

/* Parse generic header. */
static pjsip_hdr* parse_hdr_generic_string( pjsip_parse_ctx *ctx )
{
    pjsip_generic_string_hdr *hdr;

    hdr = pjsip_generic_string_hdr_create(ctx->pool, NULL, NULL);
    parse_generic_string_hdr(hdr, ctx);
    return (pjsip_hdr*)hdr;

}

/* Public function to parse a header value. */
PJ_DEF(void*) pjsip_parse_hdr( pj_pool_t *pool, const pj_str_t *hname,
			       char *buf, pj_size_t size, int *parsed_len )
{
    pj_scanner scanner;
    pjsip_hdr *hdr = NULL;
    pjsip_parse_ctx context;
    PJ_USE_EXCEPTION;

    pj_scan_init(&scanner, buf, size, PJ_SCAN_AUTOSKIP_WS_HEADER, 
                 &on_syntax_error);

    context.scanner = &scanner;
    context.pool = pool;
    context.rdata = NULL;

    PJ_TRY {
	pjsip_parse_hdr_func *func = find_handler(hname);
	if (func) {
	    hdr = (*func)(&context);
	} else {
	    hdr = parse_hdr_generic_string(&context);
	    hdr->type = PJSIP_H_OTHER;
	    pj_strdup(pool, &hdr->name, hname);
	    hdr->sname = hdr->name;
	}

    } 
    PJ_CATCH_ANY {
	hdr = NULL;
    }
    PJ_END

    if (parsed_len) {
	*parsed_len = (unsigned)(scanner.curptr - scanner.begin);
    }

    pj_scan_fini(&scanner);

    return hdr;
}

/* Parse multiple header lines */
PJ_DEF(pj_status_t) pjsip_parse_headers( pj_pool_t *pool, char *input,
				         pj_size_t size, pjsip_hdr *hlist,
				         unsigned options)
{
    enum { STOP_ON_ERROR = 1 };
    pj_str_t hname;
    pj_scanner scanner;
    pjsip_parse_ctx ctx;

    PJ_USE_EXCEPTION;

    pj_scan_init(&scanner, input, size, PJ_SCAN_AUTOSKIP_WS_HEADER,
                 &on_syntax_error);

    pj_bzero(&ctx, sizeof(ctx));
    ctx.scanner = &scanner;
    ctx.pool = pool;

retry_parse:
    PJ_TRY
    {
	/* Parse headers. */
	do {
	    pjsip_parse_hdr_func * func;
	    pjsip_hdr *hdr = NULL;

	    /* Init hname just in case parsing fails.
	     * Ref: PROTOS #2412
	     */
	    hname.slen = 0;

	    /* Get hname. */            
	    pj_scan_get( &scanner, &pconst.pjsip_TOKEN_SPEC, &hname);
	    if (pj_scan_get_char( &scanner ) != ':') {
		PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);
	    }

	    /* Find handler. */
	    func = find_handler(&hname);

	    /* Call the handler if found.
	     * If no handler is found, then treat the header as generic
	     * hname/hvalue pair.
	     */
	    if (func) {
		hdr = (*func)(&ctx);
	    } else {
		hdr = parse_hdr_generic_string(&ctx);
		hdr->name = hdr->sname = hname;
	    }

	    /* Single parse of header line can produce multiple headers.
	     * For example, if one Contact: header contains Contact list
	     * separated by comma, then these Contacts will be split into
	     * different Contact headers.
	     * So here we must insert list instead of just insert one header.
	     */
	    if (hdr)
		pj_list_insert_nodes_before(hlist, hdr);

	    /* Parse until EOF or an empty line is found. */
	} while (!pj_scan_is_eof(&scanner) && !IS_NEWLINE(*scanner.curptr));

	/* If empty line is found, eat it. */
	if (!pj_scan_is_eof(&scanner)) {
	    if (IS_NEWLINE(*scanner.curptr)) {
		pj_scan_get_newline(&scanner);
	    }
	}
    }
    PJ_CATCH_ANY
    {
	PJ_LOG(4,(THIS_FILE, "Error parsing header: '%.*s' line %d col %d",
		  (int)hname.slen, hname.ptr, scanner.line,
		  pj_scan_get_col(&scanner)));

	/* Exception was thrown during parsing. */
	if ((options & STOP_ON_ERROR) == STOP_ON_ERROR) {
	    pj_scan_fini(&scanner);
	    return PJSIP_EINVALIDHDR;
	}

	/* Skip until newline, and parse next header. */
	if (!pj_scan_is_eof(&scanner)) {
	    /* Skip until next line.
	     * Watch for header continuation.
	     */
	    do {
		pj_scan_skip_line(&scanner);
	    } while (IS_SPACE(*scanner.curptr));
	}

	/* Restore flag. Flag may be set in int_parse_sip_url() */
	scanner.skip_ws = PJ_SCAN_AUTOSKIP_WS_HEADER;

	/* Continue parse next header, if any. */
	if (!pj_scan_is_eof(&scanner) && !IS_NEWLINE(*scanner.curptr)) {
	    goto retry_parse;
	}

    }
    PJ_END;

    return PJ_SUCCESS;
}

