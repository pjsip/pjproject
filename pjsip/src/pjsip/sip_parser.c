/* $Header: /pjproject/pjsip/src/pjsip/sip_parser.c 17    9/11/05 9:28a Bennylp $ */
#include <pjsip/sip_parser.h>
#include <pjsip/sip_uri.h>
#include <pjsip/sip_msg.h>
#include <pjsip/sip_auth_parser.h>
#include <pj/scanner.h>
#include <pj/except.h>
#include <pj/log.h>
#include <pj/hash.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <ctype.h>	/* tolower() */

#define RESERVED    ";/?:@&=+$,"
#define MARK	    "-_.!~*'()"
#define ESCAPED	    "%"
#define USER	    "&=+$,;?/"
#define PASS	    "&=+$,"
#define TOKEN	    "-.!%*_=`'~+"   /* '+' is because of application/pidf+xml in Content-Type! */
#define HOST	    "_-."
#define HEX_DIGIT   "abcdefABCDEF"
#define PARAM_CHAR  "[]/:&+$" MARK "%"

#define PJSIP_VERSION		"SIP/2.0"
#define PJSIP_SYN_ERR_EXCEPTION	1

#define UNREACHED(expr)

typedef struct handler_rec
{
    char		  hname[PJSIP_MAX_HNAME_LEN+1];
    pj_size_t		  hname_len;
    pj_uint32_t		  hname_hash;
    pjsip_parse_hdr_func *handler;
} handler_rec;

static handler_rec handler[127];
static unsigned handler_count;
static int parser_is_initialized;


/*
 * Global vars (also extern).
 */
const pj_str_t  pjsip_USER_STR = { "user", 4};
const pj_str_t  pjsip_METHOD_STR = { "method", 6};
const pj_str_t  pjsip_TRANSPORT_STR = { "transport", 9};
const pj_str_t  pjsip_MADDR_STR = { "maddr", 5 };
const pj_str_t  pjsip_LR_STR = { "lr", 2 };
const pj_str_t  pjsip_SIP_STR = { "sip", 3 };
const pj_str_t  pjsip_SIPS_STR = { "sips", 4 };
const pj_str_t  pjsip_TEL_STR = { "tel", 3 };
const pj_str_t  pjsip_BRANCH_STR = { "branch", 6 };
const pj_str_t  pjsip_TTL_STR = { "ttl", 3 };
const pj_str_t  pjsip_PNAME_STR = { "received", 8 };
const pj_str_t  pjsip_Q_STR = { "q", 1 };
const pj_str_t  pjsip_EXPIRES_STR = { "expires", 7 };
const pj_str_t  pjsip_TAG_STR = { "tag", 3 };
const pj_str_t  pjsip_RPORT_STR = { "rport", 5};

pj_char_spec	pjsip_HOST_SPEC,	    /* For scanning host part. */
		pjsip_DIGIT_SPEC,	    /* Decimal digits */
		pjsip_ALPHA_SPEC,	    /* Alpha (A-Z, a-z) */
		pjsip_ALNUM_SPEC,	    /* Decimal + Alpha. */
		pjsip_TOKEN_SPEC,	    /* Token. */
		pjsip_HEX_SPEC,		    /* Hexadecimal digits. */
		pjsip_PARAM_CHAR_SPEC,	    /* For scanning pname (or pvalue when it's not quoted.) */
		pjsip_PROBE_USER_HOST_SPEC, /* Hostname characters. */
		pjsip_PASSWD_SPEC,	    /* Password. */
		pjsip_USER_SPEC,	    /* User */
		pjsip_ARRAY_ELEMENTS,	    /* Array separator. */
		pjsip_NEWLINE_OR_EOF_SPEC,  /* For eating up header.*/
		pjsip_DISPLAY_SCAN_SPEC;    /* Used when searching for display name in URL. */


/*
 * Forward decl.
 */
static pjsip_msg *	  int_parse_msg( pj_scanner *scanner, 
					 pj_pool_t *pool, 
					 pjsip_parser_err_report *err_list);
static void		  int_parse_param( pj_scanner *scanner, 
					   pj_str_t *pname, 
					   pj_str_t *pvalue);
static void		  int_parse_req_line( pj_scanner *scanner, 
					      pj_pool_t *pool,
					      pjsip_request_line *req_line);
static int		  int_is_next_user( pj_scanner *scanner);
static void		  int_parse_status_line( pj_scanner *scanner, 
						 pjsip_status_line *line);
static void		  int_parse_user_pass( pj_scanner *scanner, 
					       pj_str_t *user, 
					       pj_str_t *pass);
static void		  int_parse_uri_host_port( pj_scanner *scanner, 
						   pj_str_t *p_host, 
						   int *p_port);
static pjsip_uri *	  int_parse_uri_or_name_addr( pj_scanner *scanner, 
					    pj_pool_t *pool, unsigned option);
static pjsip_url *	  int_parse_sip_url( pj_scanner *scanner, 
					     pj_pool_t *pool,
					     pj_bool_t parse_params);
static pjsip_name_addr*	  int_parse_name_addr( pj_scanner *scanner, 
					       pj_pool_t *pool );
static void		  parse_hdr_end( pj_scanner *scanner );
static pjsip_accept_hdr*  parse_hdr_accept( pj_scanner *scanner, 
					    pj_pool_t *pool);
static pjsip_allow_hdr*   parse_hdr_allow( pj_scanner *scanner, 
					   pj_pool_t *pool);
static pjsip_cid_hdr*	  parse_hdr_call_id( pj_scanner *scanner, 
					     pj_pool_t *pool);
static pjsip_contact_hdr* parse_hdr_contact( pj_scanner *scanner, 
					     pj_pool_t *pool);
static pjsip_clen_hdr*    parse_hdr_content_length( pj_scanner *scanner, 
						    pj_pool_t *pool);
static pjsip_ctype_hdr*   parse_hdr_content_type( pj_scanner *scanner, 
						  pj_pool_t *pool);
static pjsip_cseq_hdr*    parse_hdr_cseq( pj_scanner *scanner, 
					  pj_pool_t *pool);
static pjsip_expires_hdr* parse_hdr_expires( pj_scanner *scanner, 
					  pj_pool_t *pool);
static pjsip_from_hdr*    parse_hdr_from( pj_scanner *scanner, 
					  pj_pool_t *pool);
static pjsip_max_forwards_hdr*  parse_hdr_max_forwards( pj_scanner *scanner, 
				        pj_pool_t *pool);
static pjsip_min_expires_hdr*  parse_hdr_min_expires( pj_scanner *scanner, 
				        pj_pool_t *pool);
static pjsip_rr_hdr*      parse_hdr_rr( pj_scanner *scanner, 
				        pj_pool_t *pool);
static pjsip_route_hdr*   parse_hdr_route( pj_scanner *scanner, 
					   pj_pool_t *pool);
static pjsip_require_hdr* parse_hdr_require( pj_scanner *scanner, 
				        pj_pool_t *pool);
static pjsip_retry_after_hdr* parse_hdr_retry_after( pj_scanner *scanner, 
				        pj_pool_t *pool);
static pjsip_supported_hdr* parse_hdr_supported( pj_scanner *scanner, 
				        pj_pool_t *pool);
static pjsip_to_hdr*	  parse_hdr_to( pj_scanner *scanner, 
				        pj_pool_t *pool);
static pjsip_unsupported_hdr* parse_hdr_unsupported( pj_scanner *scanner, 
				        pj_pool_t *pool);
static pjsip_via_hdr*	  parse_hdr_via( pj_scanner *scanner, 
					 pj_pool_t *pool);
static pjsip_generic_string_hdr* parse_hdr_generic_string( pj_scanner *scanner, 
					     pj_pool_t *pool);

/* Convert non NULL terminated string to integer. */
static unsigned long pj_strtoul_mindigit(const pj_str_t *str, unsigned mindig)
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
#define parser_stricmp(str1, str2)  \
	    (str1.slen != str2.slen ? -1 : \
		!(memcmp(str1.ptr, str2.ptr, str1.slen)==0 || stricmp(str1.ptr, str2.ptr)==0))

/* Syntax error handler for parser. */
static void on_syntax_error(pj_scanner *scanner)
{
    PJ_UNUSED_ARG(scanner)
    PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);
}

/* Concatenate unrecognized params into single string. */
void pjsip_concat_param_imp( pj_str_t *param, pj_pool_t *pool, 
			     const pj_str_t *pname, const pj_str_t *pvalue, int sepchar)
{
    char *new_param, *p;
    int len;

    len = param->slen + pname->slen + pvalue->slen + 3;
    p = new_param = pj_pool_alloc(pool, len);
    
    if (param->slen) {
	int old_len = param->slen;
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

/* Concatenate unrecognized params into single string. */
static void concat_param( pj_str_t *param, pj_pool_t *pool, 
			  const pj_str_t *pname, const pj_str_t *pvalue )
{
    pjsip_concat_param_imp(param, pool, pname, pvalue, ';');
}

/* Initialize static properties of the parser. */
static void init_parser()
{
    static int initialized;

    if (initialized)
	return;

    initialized = 1;

    pj_cs_add_num( pjsip_DIGIT_SPEC );
    pj_cs_add_alpha( pjsip_ALPHA_SPEC );
    
    pj_cs_add_alpha( pjsip_ALNUM_SPEC );
    pj_cs_add_num( pjsip_ALNUM_SPEC );

    pj_cs_add_str(pjsip_NEWLINE_OR_EOF_SPEC, "\r\n");
    //pj_cs_set(pjsip_NEWLINE_OR_EOF_SPEC, 0);

    pj_cs_add_str( pjsip_ARRAY_ELEMENTS, ",\r\n");

    pj_memcpy(pjsip_TOKEN_SPEC, pjsip_ALNUM_SPEC, sizeof(pj_char_spec));
    pj_cs_add_str( pjsip_TOKEN_SPEC, TOKEN);

    pj_memcpy(pjsip_HOST_SPEC, pjsip_ALNUM_SPEC, sizeof(pj_char_spec));
    pj_cs_add_str( pjsip_HOST_SPEC, HOST);

    pj_memcpy(pjsip_HEX_SPEC, pjsip_DIGIT_SPEC, sizeof(pj_char_spec));
    pj_cs_add_str( pjsip_HEX_SPEC, HEX_DIGIT);

    pj_memcpy(pjsip_PARAM_CHAR_SPEC, pjsip_ALNUM_SPEC, sizeof(pj_char_spec));
    pj_cs_add_str( pjsip_PARAM_CHAR_SPEC, PARAM_CHAR);

    pj_memcpy(pjsip_USER_SPEC, pjsip_ALNUM_SPEC, sizeof(pj_char_spec));
    pj_cs_add_str( pjsip_USER_SPEC, MARK ESCAPED USER );

    pj_memcpy(pjsip_PASSWD_SPEC, pjsip_ALNUM_SPEC, sizeof(pj_char_spec));
    pj_cs_add_str( pjsip_PASSWD_SPEC, MARK ESCAPED PASS);

    pj_cs_add_str( pjsip_PROBE_USER_HOST_SPEC, "@ \n>");
    pj_cs_invert( pjsip_PROBE_USER_HOST_SPEC );

    pj_cs_add_str( pjsip_DISPLAY_SCAN_SPEC, ":\r\n<");

    pjsip_register_hdr_parser( "Accept", NULL, (pjsip_parse_hdr_func*) &parse_hdr_accept);
    pjsip_register_hdr_parser( "Allow", NULL, (pjsip_parse_hdr_func*) &parse_hdr_allow);
    pjsip_register_hdr_parser( "Call-ID", NULL, (pjsip_parse_hdr_func*) &parse_hdr_call_id);
    pjsip_register_hdr_parser( "Contact", "m", (pjsip_parse_hdr_func*) &parse_hdr_contact);
    pjsip_register_hdr_parser( "Content-Length", NULL, (pjsip_parse_hdr_func*) &parse_hdr_content_length);
    pjsip_register_hdr_parser( "Content-Type", NULL, (pjsip_parse_hdr_func*) &parse_hdr_content_type);
    pjsip_register_hdr_parser( "CSeq", NULL, (pjsip_parse_hdr_func*) &parse_hdr_cseq);
    pjsip_register_hdr_parser( "Expires", NULL, (pjsip_parse_hdr_func*) &parse_hdr_expires);
    pjsip_register_hdr_parser( "From", "f", (pjsip_parse_hdr_func*) &parse_hdr_from);
    pjsip_register_hdr_parser( "Max-Forwards", NULL, (pjsip_parse_hdr_func*) &parse_hdr_max_forwards);
    pjsip_register_hdr_parser( "Min-Expires", NULL, (pjsip_parse_hdr_func*) &parse_hdr_min_expires);
    pjsip_register_hdr_parser( "Record-Route", NULL, (pjsip_parse_hdr_func*) &parse_hdr_rr);
    pjsip_register_hdr_parser( "Route", NULL, (pjsip_parse_hdr_func*) &parse_hdr_route);
    pjsip_register_hdr_parser( "Require", NULL, (pjsip_parse_hdr_func*) &parse_hdr_require);
    pjsip_register_hdr_parser( "Retry-After", NULL, (pjsip_parse_hdr_func*) &parse_hdr_retry_after);
    pjsip_register_hdr_parser( "Supported", "k", (pjsip_parse_hdr_func*) &parse_hdr_supported);
    pjsip_register_hdr_parser( "To", "t", (pjsip_parse_hdr_func*) &parse_hdr_to);
    pjsip_register_hdr_parser( "Unsupported", NULL, (pjsip_parse_hdr_func*) &parse_hdr_unsupported);
    pjsip_register_hdr_parser( "Via", NULL, (pjsip_parse_hdr_func*) &parse_hdr_via);

    /* Register auth parser. */
    pjsip_auth_init_parser();

}

static void init_sip_parser()
{
    if (!parser_is_initialized) {
	/* Prevent race cond. */
	pj_enter_critical_section();
	if (!parser_is_initialized) {
	    init_parser();
	    parser_is_initialized = 1;
	}
	pj_leave_critical_section();
    }
}

/* Compare the handler record with header name, and return:
 * - 0  if handler match.
 * - <0 if handler is 'less' than the header name.
 * - >0 if handler is 'greater' than header name.
 */
static int compare_handler( const handler_rec *r1, 
			    const char *name, 
			    pj_size_t name_len,
			    pj_uint32_t hash )
{
    /* Compare length. */
    if (r1->hname_len < name_len)
	return -1;
    if (r1->hname_len > name_len)
	return 1;

    /* Length is equal, compare hashed value. */
    if (r1->hname_hash < hash)
	return -1;
    if (r1->hname_hash > hash)
	return 1;

    /* Equal length and equal hash. compare the strings. */
    return strcmp(r1->hname, name);
}

/* Register one handler for one header name. */
static int int_register_parser( const char *name, pjsip_parse_hdr_func *fptr )
{
    unsigned	pos;
    handler_rec rec;
    unsigned	i;

    if (handler_count >= PJ_ARRAY_SIZE(handler)) {
	return -1;
    }

    /* Initialize temporary handler. */
    rec.handler = fptr;
    rec.hname_len = strlen(name);
    if (rec.hname_len >= sizeof(rec.hname)) {
	return -1;
    }
    /* Name is copied in lowercase. */
    for (i=0; i<rec.hname_len; ++i) {
	rec.hname[i] = (char)tolower(name[i]);
    }
    rec.hname[i] = '\0';
    /* Hash value is calculated from the lowercase name. */
    rec.hname_hash = pj_hash_calc(0, rec.hname, PJ_HASH_KEY_STRING);

    /* Get the pos to insert the new handler. */
    for (pos=0; pos < handler_count; ++pos) {
	int d;
	d = compare_handler(&handler[pos], rec.hname, rec.hname_len, rec.hname_hash);
	if (d == 0) {
	    pj_assert(0);
	    return -1;
	}
	if (d > 0) {
	    break;
	}
    }

    /* Shift handlers. */
    if (pos != handler_count) {
	pj_memmove( &handler[pos+1], &handler[pos], (handler_count-pos)*sizeof(handler_rec));
    }
    /* Add new handler. */
    pj_memcpy( &handler[pos], &rec, sizeof(handler_rec));
    ++handler_count;

    return 0;
}

/* Register parser handler. If both header name and short name are valid,
 * then two instances of handler will be registered.
 */
PJ_DEF(pj_status_t) pjsip_register_hdr_parser( const char *hname,
					       const char *hshortname,
					       pjsip_parse_hdr_func *fptr)
{
    if (int_register_parser(hname, fptr)) {
	return -1;
    }
    if (hshortname && int_register_parser(hshortname, fptr)) {
	return -1;
    }
    return 0;
}

/* Find handler to parse the header name. */
static pjsip_parse_hdr_func * find_handler(const pj_str_t *hname)
{
    handler_rec *first;
    char	 hname_copy[PJSIP_MAX_HNAME_LEN];
    pj_uint32_t  hash;
    int		 comp;
    unsigned	 i, n;

    /* Calculate hash value while converting the header to lowercase. 
     * Don't assume that 'hname' is NULL terminated.
     */
    hash = 0;
    for (i=0; i<(unsigned)hname->slen; ++i) {
	hname_copy[i] = (char)tolower(hname->ptr[i]);
	hash = hash * PJ_HASH_MULTIPLIER + hname_copy[i];
    }
    hname_copy[i] = '\0';

    /* Binary search for the handler. */
    comp = -1;
    first = &handler[0];
    n = handler_count;
    for (; n > 0; ) {
	unsigned half = n / 2;
	handler_rec *mid = first + half;

	comp = compare_handler(mid, hname_copy, hname->slen, hash);
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

/* Public function to parse SIP message. */
PJ_DEF(pjsip_msg*) pjsip_parse_msg( pj_pool_t *pool, char *buf, pj_size_t size,
				    pjsip_parser_err_report *err_list)
{
    pjsip_msg *msg = NULL;
    pj_scanner scanner;
    PJ_USE_EXCEPTION;

    init_sip_parser();

    pj_scan_init(&scanner, buf, size, PJ_SCAN_AUTOSKIP_WS_HEADER, &on_syntax_error);

    PJ_TRY {
	msg = int_parse_msg(&scanner, pool, err_list);
    } 
    PJ_DEFAULT {
	msg = NULL;
    }
    PJ_END

    pj_scan_fini(&scanner);
    return msg;
}

/* Determine if a message has been received. */
PJ_DEF(pj_bool_t) pjsip_find_msg( const char *buf, pj_size_t size, 
				  pj_bool_t is_datagram, pj_size_t *msg_size)
{
#if PJ_HAS_TCP
    const char *hdr_end;
    const char *body_start;
    const char *pos;
    const char *line;
    int content_length = -1;

    *msg_size = size;

    /* For datagram, the whole datagram IS the message. */
    if (is_datagram) {
	return PJ_TRUE;
    }


    /* Find the end of header area by finding an empty line. */
    if ((pos = strstr(buf, "\n\r\n")) == NULL) {
	return PJ_FALSE;
    }

    hdr_end = pos+1;
    body_start = pos+3;

    /* Find "Content-Length" header the hard way. */
    line = strchr(buf, '\n');
    while (line && line < hdr_end-14) {
	++line;
	if ( ((*line=='C' || *line=='c') && strncasecmp(line, "Content-Length", 14) == 0) ||
	     ((*line=='l' || *line=='L') && (*(line+1)==' ' || *(line+1)=='\t' || *(line+1)==':')))
	{
	    /* Try to parse the header. */
	    pj_scanner scanner;
	    PJ_USE_EXCEPTION;

	    init_sip_parser();

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
		pj_scan_get(&scanner, pjsip_DIGIT_SPEC, &str_clen);

		/* Get newline. */
		pj_scan_get_newline(&scanner);

		/* Found a valid Content-Length header. */
		content_length = pj_strtoul(&str_clen);
	    } 
	    PJ_END

	    pj_scan_fini(&scanner);
	}

	/* Found valid Content-Length? */
	if (content_length != -1)
	    break;

	/* Go to next line. */
	line = strchr(line, '\n');
    }

    /* Found Content-Length? */
    if (content_length == -1) {
	PJ_LOG(4, ("sipparser", "pjsip_find_msg: incoming TCP packet has missing "
			     "Content-Length header, treated as incomplete."));
	return PJ_FALSE;
    }

    /* Enough packet received? */
    *msg_size = (body_start - buf) + content_length;
    return (*msg_size) <= size;
#else
    PJ_UNUSED_ARG(buf)
    PJ_UNUSED_ARG(is_datagram)
    *msg_size = size;
    return 1;
#endif
}

/* Public function to parse URI */
PJ_DEF(pjsip_uri*) pjsip_parse_uri( pj_pool_t *pool, 
					 char *buf, pj_size_t size,
					 unsigned option)
{
    PJ_USE_EXCEPTION;
    pj_scanner scanner;
    pjsip_uri *uri = NULL;

    if (!parser_is_initialized) {
	init_parser();
	parser_is_initialized = 1;
    }

    pj_scan_init(&scanner, buf, size, 0, &on_syntax_error);

    
    PJ_TRY {
	uri = int_parse_uri_or_name_addr(&scanner, pool, option);
    }
    PJ_END;

    /* Must have exhausted all inputs. */
    if (pj_scan_is_eof(&scanner) || *scanner.current=='\r' || *scanner.current=='\n') {
	/* Success. */
	pj_scan_fini(&scanner);
	return uri;
    }

    /* Still have some characters unparsed. */
    pj_scan_fini(&scanner);
    return NULL;
}

/* Generic function to print message body.
 * This assumes that the 'data' member points to a contigous memory where the 
 * actual body is laid.
 */
static int generic_print_body (pjsip_msg_body *msg_body, char *buf, pj_size_t size)
{
    pjsip_msg_body *body = msg_body;
    if (size < body->len)
	return 0;

    pj_memcpy (buf, body->data, body->len);
    return body->len;
}

/* Internal function to parse SIP message */
static pjsip_msg *int_parse_msg( pj_scanner *scanner, pj_pool_t *pool, 
				 pjsip_parser_err_report *err_list)
{
    PJ_USE_EXCEPTION;
    int ch;
    pjsip_msg *msg;
    pjsip_ctype_hdr *ctype_hdr = NULL;

    /* Skip leading newlines. */
    ch = *scanner->current;
    while (ch=='\r' || ch=='\n') {
	pj_scan_get_char(scanner);
	ch = *scanner->current;
    }

    msg = pjsip_msg_create(pool, PJSIP_REQUEST_MSG);

    /* Parse request or status line */
    if (pj_scan_stricmp( scanner, PJSIP_VERSION, 7) == 0) {
	msg->type = PJSIP_RESPONSE_MSG;
	int_parse_status_line( scanner, &msg->line.status );
    } else {
	msg->type = PJSIP_REQUEST_MSG;
	int_parse_req_line(scanner, pool, &msg->line.req );
    }

    /* Parse headers. */
    do {
	pj_str_t hname;
	pjsip_parse_hdr_func * handler;
	pjsip_hdr *hdr = NULL;

	/* Get hname. */
	pj_scan_get( scanner, pjsip_TOKEN_SPEC, &hname);
	ch = pj_scan_get_char( scanner );
	if (ch != ':') {
	    PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);
	}

	/* Find handler. */
	handler = find_handler(&hname);

	PJ_TRY {
	    /* Call the handler if found.
	     * If no handler is found, then treat the header as generic
	     * hname/hvalue pair.
	     */
	    if (handler) {
		hdr = (*handler)(scanner, pool);
	    } else {
		pjsip_generic_string_hdr *ghdr = parse_hdr_generic_string(scanner, pool);
		ghdr->type = PJSIP_H_OTHER;
		ghdr->name = ghdr->sname = hname;
		hdr = (pjsip_hdr*)ghdr;
	    }

	    /* Check if we've just parsed a Content-Type header. 
	     * We will check for a message body if we've got Content-Type header.
	     */
	    if (hdr->type == PJSIP_H_CONTENT_TYPE) {
		ctype_hdr = (pjsip_ctype_hdr*)hdr;
	    }

	}
	PJ_DEFAULT {
	    /* Exception was thrown during parsing. 
	     * Skip until newline, and parse next header. 
	     */
	    pj_str_t token;
	    hdr = NULL;

	    PJ_LOG(4,("sipparser", "Syntax error in line %d col %d (hname=%.*s)",
		      scanner->line, scanner->col, hname.slen, hname.ptr));

	    if (err_list) {
		pjsip_parser_err_report *err_info;
		
		err_info = pj_pool_alloc(pool, sizeof(*err_info));
		err_info->exception_code = PJ_GET_EXCEPTION();
		err_info->line = scanner->line;
		err_info->col = scanner->col;
		err_info->hname = hname;

		pj_list_insert_before(err_list, err_info);
	    }

	    if (!pj_scan_is_eof(scanner)) {
		pj_scan_get_until(scanner, pjsip_NEWLINE_OR_EOF_SPEC, &token);
		parse_hdr_end(scanner);
	    }
	}
	PJ_END;

	if (hdr) {
	    /* Single parse of header line can produce multiple headers.
	     * For example, if one Contact: header contains Contact list
	     * separated by comma, then these Contacts will be split into
	     * different Contact headers.
	     * So here we must insert list instead of just insert one header.
	     */
	    pj_list_insert_nodes_before(&msg->hdr, hdr);
	}

	/* Parse until EOF or an empty line is found. */
    } while (!pj_scan_is_eof(scanner) && 
	      *scanner->current != '\r' && *scanner->current != '\n');

    /* If empty line is found, eat it. */
    if (!pj_scan_is_eof(scanner)) {
	if (*scanner->current=='\r' || *scanner->current=='\n') {
	    pj_scan_get_newline(scanner);
	}
    }

    /* If we have Content-Type header, treat the rest of the message as body. */
    if (ctype_hdr) {
	pjsip_msg_body *body = pj_pool_alloc(pool, sizeof(pjsip_msg_body));
	pj_strdup (pool, &body->content_type.type, &ctype_hdr->media.type);
	pj_strdup (pool, &body->content_type.subtype, &ctype_hdr->media.subtype);
	pj_strdup (pool, &body->content_type.param, &ctype_hdr->media.param);
	body->data = scanner->current;
	body->len = scanner->end - scanner->current;
	body->print_body = &generic_print_body;

	msg->body = body;
    }

    return msg;
}

/* Parse parameter (pname ["=" pvalue]). */
void pjsip_parse_param_imp(  pj_scanner *scanner,
			     pj_str_t *pname, pj_str_t *pvalue,
			     unsigned option)
{
    /* pname */
    pj_scan_get(scanner, pjsip_PARAM_CHAR_SPEC, pname);

    /* pvalue, if any */
    if (*scanner->current == '=') {
	pj_scan_get_char(scanner);
	/* pvalue can be a quoted string. */
	if (*scanner->current == '"') {
	    pj_scan_get_quote( scanner, '"', '"', pvalue);
	    if (option & PJSIP_PARSE_REMOVE_QUOTE) {
		pvalue->ptr++;
		pvalue->slen -= 2;
	    }
	} else {
	    pj_scan_get(scanner, pjsip_PARAM_CHAR_SPEC, pvalue);
	}
    } else {
	pvalue->ptr = NULL;
	pvalue->slen = 0;
    }
}

/* Parse parameter (";" pname ["=" pvalue]). */
static void int_parse_param( pj_scanner *scanner,
			     pj_str_t *pname, pj_str_t *pvalue)
{
    /* Get ';' character */
    pj_scan_get_char(scanner);

    /* Get pname and optionally pvalue */
    pjsip_parse_param_imp(scanner, pname, pvalue, 0);
}

/* Parse host:port in URI. */
static void int_parse_uri_host_port( pj_scanner *scanner, 
				     pj_str_t *host, int *p_port)
{
    pj_scan_get( scanner, pjsip_HOST_SPEC, host);
    if (*scanner->current == ':') {
	pj_str_t port;
	pj_scan_get_char(scanner);
	pj_scan_get(scanner, pjsip_DIGIT_SPEC, &port);
	*p_port = pj_strtoul(&port);
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
    if (pj_scan_peek( scanner, pjsip_PROBE_USER_HOST_SPEC, &dummy) == '@')
	is_user = 1;
    else
	is_user = 0;

    return is_user;
}

/* Parse user:pass tokens in an URI. */
static void int_parse_user_pass( pj_scanner *scanner, 
				 pj_str_t *user, pj_str_t *pass)
{
    pj_scan_get( scanner, pjsip_USER_SPEC, user);
    if ( *scanner->current == ':') {
	pj_scan_get_char( scanner );
	pj_scan_get( scanner, pjsip_PASSWD_SPEC, pass);
    } else {
	pass->ptr = NULL;
	pass->slen = 0;
    }

    /* Get the '@' */
    pj_scan_get_char( scanner );
}

/* Parse all types of URI. */
static pjsip_uri *int_parse_uri_or_name_addr(pj_scanner *scanner, pj_pool_t *pool,
						  unsigned option)
{
    pjsip_uri *uri;
    int is_name_addr = 0;

    if (*scanner->current=='"' || *scanner->current=='<') {
	uri = (pjsip_uri*)int_parse_name_addr( scanner, pool );
	is_name_addr = 1;
    } else {
	pj_scan_state backtrack;
	pj_str_t scheme;
	int colon;

	pj_scan_save_state( scanner, &backtrack);
	pj_scan_get( scanner, pjsip_TOKEN_SPEC, &scheme);
	colon = pj_scan_get_char( scanner );
	pj_scan_restore_state( scanner, &backtrack);

	if (colon==':' && (parser_stricmp(scheme, pjsip_SIP_STR)==0 || parser_stricmp(scheme, pjsip_SIPS_STR)==0)) 
	{
	    uri = (pjsip_uri*)int_parse_sip_url( scanner, pool, 
					(option & PJSIP_PARSE_URI_IN_FROM_TO_HDR) == 0 );

	} else if (colon==':' && parser_stricmp( scheme, pjsip_TEL_STR)==0) {

	    /* Not supported. */
	    PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);
	    UNREACHED({return NULL; /* Not reached. */});

	} else {
	    uri = (pjsip_uri*)int_parse_name_addr( scanner, pool );
	    is_name_addr = 1;
	}
    }

    /* Should we return the URI object as name address? */
    if (option & PJSIP_PARSE_URI_AS_NAMEADDR) {
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
    if (*scanner->current=='"' || *scanner->current=='<') {
	return (pjsip_uri*)int_parse_name_addr( scanner, pool );
    } else {
	pj_str_t scheme;
	int colon;

	/* Get scheme. */
	colon = pj_scan_peek(scanner, pjsip_TOKEN_SPEC, &scheme);
	if (colon != ':') {
	    PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);
	}

	if ((parser_stricmp(scheme, pjsip_SIP_STR)==0 || parser_stricmp(scheme, pjsip_SIPS_STR)==0)) 
	{
	    return (pjsip_uri*)int_parse_sip_url( scanner, pool, parse_params );

	} else if (parser_stricmp(scheme, pjsip_TEL_STR)==0) {
	    PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);
	    UNREACHED({ return NULL; /* Not reached. */ })

	} else {
	    PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);
	    UNREACHED({ return NULL; /* Not reached. */ })
	}
    }
}

/* Parse "sip:" and "sips:" URI. */
static pjsip_url *int_parse_sip_url( pj_scanner *scanner, 
				     pj_pool_t *pool,
				     pj_bool_t parse_params)
{
    pj_str_t scheme;
    pjsip_url *url;
    int colon;
    int skip_ws = scanner->skip_ws;
    scanner->skip_ws = 0;

    pj_scan_get(scanner, pjsip_TOKEN_SPEC, &scheme);
    colon = pj_scan_get_char(scanner);
    if (colon != ':') {
	PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);
    }

    if (parser_stricmp(scheme, pjsip_SIP_STR)==0) {
	url = pjsip_url_create(pool, 0);

    } else if (parser_stricmp(scheme, pjsip_SIPS_STR)==0) {
	url = pjsip_url_create(pool, 1);

    } else {
	PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);
	/* should not reach here */
	UNREACHED({
	    pj_assert(0);
	    return 0;
	})
    }

    if (int_is_next_user(scanner)) {
	int_parse_user_pass(scanner, &url->user, &url->passwd);
    }

    /* Get host:port */
    int_parse_uri_host_port(scanner, &url->host, &url->port);

    /* Get URL parameters. */
    while ( parse_params && *scanner->current == ';' ) {
	pj_str_t pname, pvalue;

	int_parse_param( scanner, &pname, &pvalue);

	if (!parser_stricmp(pname, pjsip_USER_STR) && pvalue.slen) {
	    url->user_param = pvalue;

	} else if (!parser_stricmp(pname, pjsip_METHOD_STR) && pvalue.slen) {
	    url->method_param = pvalue;

	} else if (!parser_stricmp(pname, pjsip_TRANSPORT_STR) && pvalue.slen) {
	    url->transport_param = pvalue;

	} else if (!parser_stricmp(pname, pjsip_TTL_STR) && pvalue.slen) {
	    url->ttl_param = pj_strtoul(&pvalue);

	} else if (!parser_stricmp(pname, pjsip_MADDR_STR) && pvalue.slen) {
	    url->maddr_param = pvalue;

	} else if (!parser_stricmp(pname, pjsip_LR_STR)) {
	    url->lr_param = 1;

	} else {
	    concat_param(&url->other_param, pool, &pname, &pvalue);
	}
    }

    /* Get header params. */
    if (parse_params && *scanner->current == '?') {
	pj_scan_get_until(scanner, pjsip_NEWLINE_OR_EOF_SPEC, &url->header_param);
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

    if (*scanner->current == '"') {
	pj_scan_get_quote( scanner, '"', '"', &name_addr->display);

    } else if (*scanner->current != '<') {
	int next;
	pj_str_t dummy;

	/* This can be either the start of display name,
	 * the start of URL ("sip:", "sips:", "tel:", etc.), or '<' char.
	 * We're only interested in display name, because SIP URL
	 * will be parser later.
	 */
	next = pj_scan_peek_until(scanner, pjsip_DISPLAY_SCAN_SPEC, &dummy);
	if (next == '<') {
	    /* Ok, this is what we're looking for, a display name. */
	    pj_scan_get_until_ch( scanner, '<', &name_addr->display);
	    pj_strtrim(&name_addr->display);
	}
    }

    /* Manually skip whitespace. */
    pj_scan_skip_whitespace(scanner);

    /* Get the SIP-URL */
    has_bracket = (*scanner->current == '<');
    if (has_bracket)
	pj_scan_get_char(scanner);
    name_addr->uri = int_parse_uri( scanner, pool, PJ_TRUE );
    if (has_bracket)
	pj_scan_get_char(scanner);

    return name_addr;
}


/* Parse SIP request line. */
static void int_parse_req_line( pj_scanner *scanner, pj_pool_t *pool,
				pjsip_request_line *req_line)
{
    pj_str_t token;

    pj_scan_get( scanner, pjsip_TOKEN_SPEC, &token);
    pjsip_method_init_np( &req_line->method, &token);

    req_line->uri = int_parse_uri(scanner, pool, PJ_TRUE);
    if (pj_scan_stricmp( scanner, PJSIP_VERSION, 7) != 0)
	PJ_THROW( PJSIP_SYN_ERR_EXCEPTION);
    pj_scan_advance_n (scanner, 7, 1);
    pj_scan_get_newline( scanner );
}

/* Parse status line. */
static void int_parse_status_line( pj_scanner *scanner, 
				   pjsip_status_line *status_line)
{
    pj_str_t token;

    if (pj_scan_stricmp(scanner, PJSIP_VERSION, 7) != 0)
	PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);
    pj_scan_advance_n( scanner, 7, 1);

    pj_scan_get( scanner, pjsip_DIGIT_SPEC, &token);
    status_line->code = pj_strtoul(&token);
    pj_scan_get_until( scanner, pjsip_NEWLINE_OR_EOF_SPEC, &status_line->reason);
    pj_scan_get_newline( scanner );
}

/* Parse ending of header. */
static void parse_hdr_end( pj_scanner *scanner )
{
    if (pj_scan_is_eof(scanner)) {
	;   /* Do nothing. */
    } else if (*scanner->current == '&') {
	pj_scan_get_char(scanner);
    } else {
	pj_scan_get_newline(scanner);
    }
}

/* Parse ending of header. */
void pjsip_parse_end_hdr_imp( pj_scanner *scanner )
{
    parse_hdr_end(scanner);
}

/* Parse generic array header. */
static void parse_generic_array_hdr( pjsip_generic_array_hdr *hdr,
				     pj_scanner *scanner)
{
    pj_scan_get_until( scanner, pjsip_ARRAY_ELEMENTS, &hdr->values[0]);
    hdr->count++;

    while (*scanner->current == ',') {
	pj_scan_get_char(scanner);
	pj_scan_get_until( scanner, pjsip_ARRAY_ELEMENTS, &hdr->values[hdr->count]);
	hdr->count++;
    }
    parse_hdr_end(scanner);
}

/* Parse generic string header. */
static void parse_generic_string_hdr( pjsip_generic_string_hdr *hdr,
				      pj_scanner *scanner )
{
    pj_scan_get_until( scanner, pjsip_NEWLINE_OR_EOF_SPEC, &hdr->hvalue);
    parse_hdr_end(scanner);
}

/* Parse generic integer header. */
static void parse_generic_int_hdr( pjsip_generic_int_hdr *hdr,
				   pj_scanner *scanner )
{
    pj_str_t tmp;
    pj_scan_get_until( scanner, pjsip_NEWLINE_OR_EOF_SPEC, &tmp);
    hdr->ivalue = pj_strtoul(&tmp);
    parse_hdr_end(scanner);
}


/* Parse Accept header. */
static pjsip_accept_hdr* parse_hdr_accept( pj_scanner *scanner, 
					    pj_pool_t *pool)
{
    pjsip_accept_hdr *accept = pjsip_accept_hdr_create(pool);
    parse_generic_array_hdr(accept, scanner);
    return accept;
}

/* Parse Allow header. */
static pjsip_allow_hdr*   parse_hdr_allow( pj_scanner *scanner, 
					   pj_pool_t *pool)
{
    pjsip_allow_hdr *allow = pjsip_allow_hdr_create(pool);
    parse_generic_array_hdr(allow, scanner);
    return allow;
}

/* Parse Call-ID header. */
static pjsip_cid_hdr* parse_hdr_call_id( pj_scanner *scanner, 
					  pj_pool_t *pool)
{
    pjsip_cid_hdr *hdr = pjsip_cid_hdr_create(pool);
    pj_scan_get_until( scanner, pjsip_NEWLINE_OR_EOF_SPEC, &hdr->id);
    parse_hdr_end(scanner);
    return hdr;
}

/* Parse and interpret Contact param. */
static void int_parse_contact_param( pjsip_contact_hdr *hdr, 
				     pj_scanner *scanner,
				     pj_pool_t *pool)
{
    while ( *scanner->current == ';' ) {
	pj_str_t pname, pvalue;

	int_parse_param( scanner, &pname, &pvalue);
	if (!parser_stricmp(pname, pjsip_Q_STR) && pvalue.slen) {
	    char *dot_pos = memchr(pvalue.ptr, '.', pvalue.slen);
	    if (!dot_pos) {
		hdr->q1000 = pj_strtoul(&pvalue);
	    } else {
		pvalue.slen = (pvalue.ptr+pvalue.slen) - (dot_pos+1);
		pvalue.ptr = dot_pos + 1;
		hdr->q1000 = pj_strtoul_mindigit(&pvalue, 3);
	    }    
	} else if (!parser_stricmp(pname, pjsip_EXPIRES_STR) && pvalue.slen) {
	    hdr->expires = pj_strtoul(&pvalue);

	} else {
	    concat_param(&hdr->other_param, pool, &pname, &pvalue);
	}
    }
}

/* Parse Contact header. */
PJ_DEF(pjsip_contact_hdr*) parse_hdr_contact( pj_scanner *scanner, 
					      pj_pool_t *pool)
{
    pjsip_contact_hdr *first = NULL;
    
    do {
	pjsip_contact_hdr *hdr = pjsip_contact_hdr_create(pool);
	if (first == NULL)
	    first = hdr;
	else
	    pj_list_insert_before(first, hdr);

	if (*scanner->current == '*') {
	    pj_scan_get_char(scanner);
	    hdr->star = 1;

	} else {
	    hdr->star = 0;
	    hdr->uri = int_parse_uri_or_name_addr(scanner, pool, PJSIP_PARSE_URI_AS_NAMEADDR);

	    int_parse_contact_param(hdr, scanner, pool);
	}

	if (*scanner->current != ',')
	    break;

	pj_scan_get_char(scanner);

    } while (1);

    parse_hdr_end(scanner);
    return first;
}

/* Parse Content-Length header. */
PJ_DEF(pjsip_clen_hdr*) parse_hdr_content_length( pj_scanner *scanner, 
					          pj_pool_t *pool)
{
    pj_str_t digit;
    pjsip_clen_hdr *hdr;

    hdr = pjsip_clen_hdr_create(pool);
    pj_scan_get(scanner, pjsip_DIGIT_SPEC, &digit);
    hdr->len = pj_strtoul(&digit);
    parse_hdr_end(scanner);
    return hdr;
}

/* Parse Content-Type header. */
PJ_DEF(pjsip_ctype_hdr*) parse_hdr_content_type( pj_scanner *scanner, 
					         pj_pool_t *pool)
{
    pjsip_ctype_hdr *hdr;

    hdr = pjsip_ctype_hdr_create(pool);
    
    /* Parse media type and subtype. */
    pj_scan_get(scanner, pjsip_TOKEN_SPEC, &hdr->media.type);
    pj_scan_get_char(scanner);
    pj_scan_get(scanner, pjsip_TOKEN_SPEC, &hdr->media.subtype);

    /* Parse media parameters */
    while (*scanner->current == ';') {
	pj_str_t pname, pvalue;
	int_parse_param(scanner, &pname, &pvalue);
	concat_param(&hdr->media.param, pool, &pname, &pvalue);
    }

    parse_hdr_end(scanner);
    return hdr;
}

/* Parse CSeq header. */
PJ_DEF(pjsip_cseq_hdr*) parse_hdr_cseq( pj_scanner *scanner, 
				        pj_pool_t *pool)
{
    pj_str_t cseq, method;
    pjsip_cseq_hdr *hdr;

    hdr = pjsip_cseq_hdr_create(pool);
    pj_scan_get( scanner, pjsip_DIGIT_SPEC, &cseq);
    hdr->cseq = pj_strtoul(&cseq);

    pj_scan_get( scanner, pjsip_TOKEN_SPEC, &method);
    pjsip_method_init_np(&hdr->method, &method);

    parse_hdr_end( scanner );
    return hdr;
}

/* Parse Expires header. */
static pjsip_expires_hdr* parse_hdr_expires(pj_scanner *scanner, 
					    pj_pool_t *pool)
{
    pjsip_expires_hdr *hdr = pjsip_expires_hdr_create(pool);
    parse_generic_int_hdr(hdr, scanner);
    return hdr;
}

/* Parse From: or To: header. */
static void parse_hdr_fromto( pj_scanner *scanner, 
			      pj_pool_t *pool, 
			      pjsip_from_hdr *hdr)
{
    hdr->uri = int_parse_uri_or_name_addr(scanner, pool, 
					  PJSIP_PARSE_URI_AS_NAMEADDR |
					  PJSIP_PARSE_URI_IN_FROM_TO_HDR);

    while ( *scanner->current == ';' ) {
	pj_str_t pname, pvalue;

	int_parse_param( scanner, &pname, &pvalue);

	if (!parser_stricmp(pname, pjsip_TAG_STR)) {
	    hdr->tag = pvalue;
	    
	} else {
	    concat_param(&hdr->other_param, pool, &pname, &pvalue);
	}
    }

    parse_hdr_end(scanner);
}

/* Parse From: header. */
PJ_DEF(pjsip_from_hdr*) parse_hdr_from( pj_scanner *scanner, 
				        pj_pool_t *pool)
{
    pjsip_from_hdr *hdr = pjsip_from_hdr_create(pool);
    parse_hdr_fromto(scanner, pool, hdr);
    return hdr;
}

/* Parse Require: header. */
static pjsip_require_hdr* parse_hdr_require( pj_scanner *scanner, 
					     pj_pool_t *pool)
{
    pjsip_require_hdr *hdr = pjsip_require_hdr_create(pool);
    parse_generic_array_hdr(hdr, scanner);
    return hdr;
}

/* Parse Retry-After: header. */
static pjsip_retry_after_hdr* parse_hdr_retry_after(pj_scanner *scanner, 
						    pj_pool_t *pool)
{
    pjsip_retry_after_hdr *hdr;
    hdr = pjsip_retry_after_hdr_create(pool);
    parse_generic_int_hdr(hdr, scanner);
    return hdr;
}

/* Parse Supported: header. */
static pjsip_supported_hdr* parse_hdr_supported(pj_scanner *scanner, 
						pj_pool_t *pool)
{
    pjsip_supported_hdr *hdr = pjsip_supported_hdr_create(pool);
    parse_generic_array_hdr(hdr, scanner);
    return hdr;
}


/* Parse To: header. */
PJ_DEF(pjsip_to_hdr*) parse_hdr_to( pj_scanner *scanner, 
				    pj_pool_t *pool)
{
    pjsip_to_hdr *hdr = pjsip_to_hdr_create(pool);
    parse_hdr_fromto(scanner, pool, hdr);
    return hdr;
}

/* Parse Unsupported: header. */
static pjsip_unsupported_hdr* parse_hdr_unsupported(pj_scanner *scanner, 
						    pj_pool_t *pool)
{
    pjsip_unsupported_hdr *hdr = pjsip_unsupported_hdr_create(pool);
    parse_generic_array_hdr(hdr, scanner);
    return hdr;
}

/* Parse and interpret Via parameters. */
static void int_parse_via_param( pjsip_via_hdr *hdr, pj_scanner *scanner,
				 pj_pool_t *pool)
{
    while ( *scanner->current == ';' ) {
	pj_str_t pname, pvalue;

	int_parse_param( scanner, &pname, &pvalue);

	if (!parser_stricmp(pname, pjsip_BRANCH_STR) && pvalue.slen) {
	    hdr->branch_param = pvalue;

	} else if (!parser_stricmp(pname, pjsip_TTL_STR) && pvalue.slen) {
	    hdr->ttl_param = pj_strtoul(&pvalue);
	    
	} else if (!parser_stricmp(pname, pjsip_MADDR_STR) && pvalue.slen) {
	    hdr->maddr_param = pvalue;

	} else if (!parser_stricmp(pname, pjsip_PNAME_STR) && pvalue.slen) {
	    hdr->recvd_param = pvalue;

	} else if (!parser_stricmp(pname, pjsip_RPORT_STR)) {
	    if (pvalue.slen)
		hdr->rport_param = pj_strtoul(&pvalue);
	    else
		hdr->rport_param = 0;
	} else {
	    concat_param( &hdr->other_param, pool, &pname, &pvalue);
	}
    }

}

/* Parse Max-Forwards header. */
static pjsip_max_forwards_hdr* parse_hdr_max_forwards( pj_scanner *scanner, 
						       pj_pool_t *pool)
{
    pjsip_max_forwards_hdr *hdr;
    hdr = pjsip_max_forwards_hdr_create(pool);
    parse_generic_int_hdr(hdr, scanner);
    return hdr;
}

/* Parse Min-Expires header. */
static pjsip_min_expires_hdr*  parse_hdr_min_expires(pj_scanner *scanner, 
						     pj_pool_t *pool)
{
    pjsip_min_expires_hdr *hdr;
    hdr = pjsip_min_expires_hdr_create(pool);
    parse_generic_int_hdr(hdr, scanner);
    return hdr;
}


/* Parse Route: or Record-Route: header. */
static void parse_hdr_rr_route( pj_scanner *scanner, pj_pool_t *pool,
				pjsip_routing_hdr *hdr )
{
    pjsip_name_addr *temp=int_parse_name_addr(scanner, pool);

    pj_memcpy(&hdr->name_addr, temp, sizeof(*temp));
    if (*scanner->current == ';') {
	pj_scan_get_until(scanner, pjsip_NEWLINE_OR_EOF_SPEC, &hdr->other_param);
    }
}

/* Parse Record-Route header. */
PJ_DEF(pjsip_rr_hdr*) parse_hdr_rr( pj_scanner *scanner, pj_pool_t *pool)
{
    pjsip_rr_hdr *first = NULL;

    do {
	pjsip_rr_hdr *hdr = pjsip_rr_hdr_create(pool);
	if (!first) {
	    first = hdr;
	} else {
	    pj_list_insert_before(first, hdr);
	}
	parse_hdr_rr_route(scanner, pool, hdr);
	if (*scanner->current == ',') {
	    pj_scan_get_char(scanner);
	} else {
	    break;
	}
    } while (1);
    parse_hdr_end(scanner);
    return first;
}

/* Parse Route: header. */
PJ_DEF(pjsip_route_hdr*) parse_hdr_route( pj_scanner *scanner, 
					  pj_pool_t *pool)
{
    pjsip_route_hdr *first = NULL;

    do {
	pjsip_route_hdr *hdr = pjsip_route_hdr_create(pool);
	if (!first) {
	    first = hdr;
	} else {
	    pj_list_insert_before(first, hdr);
	}
	parse_hdr_rr_route(scanner, pool, hdr);
	if (*scanner->current == ',') {
	    pj_scan_get_char(scanner);
	} else {
	    break;
	}
    } while (1);
    parse_hdr_end(scanner);
    return first;
}

/* Parse Via: header. */
PJ_DEF(pjsip_via_hdr*) parse_hdr_via( pj_scanner *scanner, pj_pool_t *pool)
{
    pjsip_via_hdr *first = NULL;

    do {
	pjsip_via_hdr *hdr = pjsip_via_hdr_create(pool);
	if (!first)
	    first = hdr;
	else
	    pj_list_insert_before(first, hdr);

	if (pj_scan_stricmp( scanner, PJSIP_VERSION "/", 8) != 0)
	    PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);

	pj_scan_advance_n( scanner, 8, 1);

	pj_scan_get( scanner, pjsip_TOKEN_SPEC, &hdr->transport);
	pj_scan_get( scanner, pjsip_HOST_SPEC, &hdr->sent_by.host);

	if (*scanner->current==':') {
	    pj_str_t digit;
	    pj_scan_get_char(scanner);
	    pj_scan_get(scanner, pjsip_DIGIT_SPEC, &digit);
	    hdr->sent_by.port = pj_strtoul(&digit);
	} else {
	    hdr->sent_by.port = 5060;
	}
	
	int_parse_via_param(hdr, scanner, pool);

	if (*scanner->current == '(') {
	    pj_scan_get_char(scanner);
	    pj_scan_get_until_ch( scanner, ')', &hdr->comment);
	    pj_scan_get_char( scanner );
	}

	if (*scanner->current != ',')
	    break;

	pj_scan_get_char(scanner);

    } while (1);

    parse_hdr_end(scanner);
    return first;
}

/* Parse generic header. */
PJ_DEF(pjsip_generic_string_hdr*) parse_hdr_generic_string( pj_scanner *scanner, 
						pj_pool_t *pool)
{
    pjsip_generic_string_hdr *hdr;

    hdr = pjsip_generic_string_hdr_create(pool, NULL);
    parse_generic_string_hdr(hdr, scanner);
    return hdr;

}

/* Public function to parse a header value. */
PJ_DEF(void*) pjsip_parse_hdr( pj_pool_t *pool, const pj_str_t *hname,
			       char *buf, pj_size_t size, int *parsed_len )
{
    pj_scanner scanner;
    pjsip_hdr *hdr = NULL;
    PJ_USE_EXCEPTION;

    init_sip_parser();

    pj_scan_init(&scanner, buf, size, PJ_SCAN_AUTOSKIP_WS_HEADER, &on_syntax_error);

    PJ_TRY {
	pjsip_parse_hdr_func *handler = find_handler(hname);
	if (handler) {
	    hdr = (*handler)(&scanner, pool);
	} else {
	    pjsip_generic_string_hdr *ghdr = parse_hdr_generic_string(&scanner, pool);
	    ghdr->type = PJSIP_H_OTHER;
	    pj_strdup(pool, &ghdr->name, hname);
	    ghdr->sname = ghdr->name;
	    hdr = (pjsip_hdr*)ghdr;
	}

    } 
    PJ_DEFAULT {
	hdr = NULL;
    }
    PJ_END

    if (parsed_len) {
	*parsed_len = (scanner.current - scanner.begin);
    }

    pj_scan_fini(&scanner);

    return hdr;
}

