/* $Id$
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
#ifndef __PJSIP_SIP_PARSER_H__
#define __PJSIP_SIP_PARSER_H__

/**
 * @file sip_parser.h
 * @brief SIP Message Parser
 */

#include <pjsip/sip_types.h>
#include <pjlib-util/scanner.h>
#include <pj/list.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_PARSER SIP Message Parser
 * @ingroup PJSIP
 * @{
 */

/**
 * URI Parsing options.
 */
enum
{
    /** If this option is specified, function #pjsip_parse_uri will return
     *  the URI object as pjsip_name_addr instead of the corresponding
     *  URI object.
     */
    PJSIP_PARSE_URI_AS_NAMEADDR = 1,

    /** If this option is specified, function #pjsip_parse_uri and other
     *  internal functions that this function calls will parse URI according
     *  to convention for parsing From/To header. For example, when the URI
     *  is not enclosed in brackets ("<" and ">"), all parameters will not
     *  be stored to the URI (it will be stored to the header).
     */
    PJSIP_PARSE_URI_IN_FROM_TO_HDR = 2,
};

/**
 * Parser syntax error exception value.
 */
#define PJSIP_SYN_ERR_EXCEPTION	1

/**
 * This structure is used to get error reporting from parser.
 */
typedef struct pjsip_parser_err_report
{
    PJ_DECL_LIST_MEMBER(struct pjsip_parser_err_report);
    int		exception_code;	/**< Error exception (e.g. PJSIP_SYN_ERR_EXCEPTION) */
    int		line;		/**< Line number. */
    int		col;		/**< Column number. */
    pj_str_t	hname;		/**< Header name, if any. */
} pjsip_parser_err_report;


/**
 * Parsing context, the default argument for parsing functions.
 */
typedef struct pjsip_parse_ctx
{
    pj_scanner      *scanner;   /**< The scanner.       */
    pj_pool_t       *pool;      /**< The pool.          */
    pjsip_rx_data   *rdata;     /**< Optional rdata.    */
} pjsip_parse_ctx;


/**
 * Type of function to parse header. The parsing function must follow these
 * specification:
 *   - It must not modify the input text.
 *   - The hname and HCOLON has been parsed prior to invoking the handler.
 *   - It returns the header instance on success.
 *   - For error reporting, it must throw PJSIP_SYN_ERR_EXCEPTION exception 
 *     instead of just returning NULL. 
 *     When exception is thrown, the return value is ignored.
 *   - It must read the header separator after finished reading the header
 *     body. The separator types are described below, and if they don't exist,
 *     exception must be thrown. Header separator can be a:
 *	- newline, such as when the header is part of a SIP message.
 *	- ampersand, such as when the header is part of an URI.
 *	- for the last header, these separator is optional since parsing
 *        can be terminated when seeing EOF.
 */
typedef pjsip_hdr* (pjsip_parse_hdr_func)(pjsip_parse_ctx *context);

/**
 * Type of function to parse URI scheme.
 * Most of the specification of header parser handler (pjsip_parse_hdr_func)
 * also applies here (except the separator part).
 */
typedef void* (pjsip_parse_uri_func)(pj_scanner *scanner, pj_pool_t *pool);

/**
 * Register header parser handler. The parser handler MUST follow the 
 * specification of header parser handler function. New registration 
 * overwrites previous registration with the same name.
 *
 * @param hname		The header name.
 * @param hshortname	The short header name or NULL.
 * @param fptr		The pointer to function to parser the header.
 *
 * @return		PJ_SUCCESS if success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_register_hdr_parser( const char *hname,
						const char *hshortname,
						pjsip_parse_hdr_func *fptr);

/**
 * Unregister previously registered header parser handler.
 * All the arguments MUST exactly equal to the value specified upon 
 * registration of the handler.
 *
 * @param hname		The header name registered.
 * @param hshortname	The short header name registered, or NULL.
 *
 * @return		zero if unregistration was successfull.
 */
PJ_DECL(pj_status_t) pjsip_unregister_hdr_parser( const char *hname,
						  const char *hshortname,
						  pjsip_parse_hdr_func *fptr);

/**
 * Register URI scheme parser handler.
 *
 * @param scheme	The URI scheme registered.
 * @param func		The URI parser function.
 *
 * @return		zero on success.
 */
PJ_DECL(pj_status_t) pjsip_register_uri_parser( const char *scheme,
					        pjsip_parse_uri_func *func);

/**
 * Unregister URI scheme parser handler.
 * All the arguments MUST exactly equal to the value specified upon 
 * registration of the handler.
 *
 * @param scheme	The URI scheme as registered previously.
 * @param func		The function handler as registered previously.
 *
 * @return		zero if the registration was successfull.
 */
PJ_DECL(pj_status_t) pjsip_unregister_uri_parser( const char *scheme,
						  pjsip_parse_uri_func *func);

/**
 * Parse an URI in the input and return the correct instance of URI.
 *
 * @param pool		The pool to get memory allocations.
 * @param buf		The input buffer, which size must be at least (size+1)
 *			because the function will temporarily put NULL 
 *			termination at the end of the buffer during parsing.
 * @param size		The length of the string (not counting NULL terminator).
 * @param options	If no options are given (value is zero), the object 
 *			returned is dependent on the syntax of the URI, 
 *			eg. basic SIP URL, TEL URL, or name address. 
 *			If option PJSIP_PARSE_URI_AS_NAMEADDR is given,
 *			then the returned object is always name address object,
 *			with the relevant URI object contained in the name 
 *			address object.
 * @return		The URI or NULL when failed. No exception is thrown by 
 *			this function (or any public parser functions).
 */
PJ_DECL(pjsip_uri*) pjsip_parse_uri( pj_pool_t *pool, 
				     char *buf, pj_size_t size,
				     unsigned option);

/**
 * Parse a packet buffer and build a full SIP message from the packet. This
 * function parses all parts of the message, including request/status line,
 * all headers, and the message body. The message body however is only 
 * treated as a text block, ie. the function will not try to parse the content
 * of the body.
 *
 * @param pool		The pool to allocate memory.
 * @param buf		The input buffer, which size must be at least (size+1)
 *			because the function will temporarily put NULL 
 *			termination at the end of the buffer during parsing.
 * @param size		The length of the string (not counting NULL terminator).
 * @param err_list	If this parameter is not NULL, then the parser will
 *			put error messages during parsing in this list.
 *
 * @return		The message or NULL when failed. No exception is thrown
 *			by this function (or any public parser functions).
 */
PJ_DECL(pjsip_msg *) pjsip_parse_msg( pj_pool_t *pool, 
				      char *buf, pj_size_t size,
				      pjsip_parser_err_report *err_list);


/**
 * Parse a packet buffer and build a rdata. The resulting message will be
 * stored in \c msg field in the \c rdata. This behaves pretty much like
 * #pjsip_parse_msg(), except that it will also initialize the header fields
 * in the \c rdata.
 *
 * This function is normally called by the transport layer.
 *
 * @param buf           The input buffer
 * @param buf		The input buffer, which size must be at least (size+1)
 *			because the function will temporarily put NULL 
 *			termination at the end of the buffer during parsing.
 * @param size		The length of the string (not counting NULL terminator).
 * @param rdata         The receive data buffer to store the message and
 *                      its elements.
 *
 * @return              The message inside the rdata if successfull, or NULL.
 */
PJ_DECL(pjsip_msg *) pjsip_parse_rdata( char *buf, pj_size_t size,
                                        pjsip_rx_data *rdata );

/**
 * Check incoming packet to see if a (probably) valid SIP message has been 
 * received.
 *
 * @param buf		The input buffer, which must be NULL terminated.
 * @param size		The buffer size.
 * @param msg_size	[out] If message is valid, this parameter will contain
 *			the size of the SIP message (including body, if any).
 *
 * @return		PJ_SUCCESS if a message is found, or an error code.
 */
PJ_DECL(pj_status_t) pjsip_find_msg(const char *buf, 
                                    pj_size_t size, 
				    pj_bool_t is_datagram, 
                                    pj_size_t *msg_size);

/**
 * Parse the content of a header and return the header instance.
 * This function parses the content of a header (ie. part after colon) according
 * to the expected name, and will return the correct instance of header.
 *
 * @param pool		Pool to allocate memory for the header.
 * @param hname		Header name which is used to find the correct function
 *			to parse the header.
 * @param line		Header content, which size must be at least size+1.
 * @param size		The length of the string (not counting NULL terminator,
 *			if any).
 * @param parsed_len	If the value is not NULL, then upon return the function
 *			will fill the pointer with the length of the string
 *			that has been parsed. This is usefull for two purposes,
 *			one is when the string may contain more than one header
 *			lines, and two when an error happen the value can
 *			pinpoint the location of the error in the buffer.
 *
 * @return		The instance of the header if parsing was successfull,
 *			or otherwise a NULL pointer will be returned.
 */
PJ_DECL(void*) pjsip_parse_hdr( pj_pool_t *pool, const pj_str_t *hname,
				char *line, pj_size_t size,
				int *parsed_len);

/**
 * Parse header line(s). Multiple headers can be parsed by this function.
 * When there are multiple headers, the headers MUST be separated by either
 * a newline (as in SIP message) or ampersand mark (as in URI). This separator
 * however is optional for the last header.
 *
 * @param pool the pool.
 * @param buf the input text to parse.
 * @param size the text length.
 * @param hlist the header list to store the parsed headers. This list must
 *              have been initialized before calling this function.
 * @return zero if successfull, or -1 if error is encountered. Upon error,
 *              the \a hlist argument MAY contain successfully parsed headers.
 */
PJ_DECL(pj_status_t) pjsip_parse_headers( pj_pool_t *pool,
					  char *input, pj_size_t size,
					  pj_list *hlist );


/*
 * Various specification used in parsing, exported here as extern for other
 * parsers.
 */
extern
pj_cis_t	pjsip_HOST_SPEC,	    /* For scanning host part. */
		pjsip_DIGIT_SPEC,	    /* Decimal digits */
		pjsip_ALPHA_SPEC,	    /* Alpha (A-Z, a-z) */
		pjsip_ALNUM_SPEC,	    /* Decimal + Alpha. */
		pjsip_TOKEN_SPEC,	    /* Token. */
		pjsip_HEX_SPEC,		    /* Hexadecimal digits. */
		pjsip_PARAM_CHAR_SPEC,	    /* For scanning pname (or pvalue when it's not quoted.) */
		pjsip_PROBE_USER_HOST_SPEC, /* Hostname characters. */
		pjsip_PASSWD_SPEC,	    /* Password. */
		pjsip_USER_SPEC,	    /* User */
		pjsip_NEWLINE_OR_EOF_SPEC,  /* For eating up header.*/
		pjsip_DISPLAY_SCAN_SPEC;    /* Used when searching for display name in URL. */

/*
 * Various string constants.
 */
extern const pj_str_t pjsip_USER_STR,
		      pjsip_METHOD_STR,
		      pjsip_TRANSPORT_STR,
		      pjsip_MADDR_STR,
		      pjsip_LR_STR,
		      pjsip_SIP_STR,
		      pjsip_SIPS_STR,
		      pjsip_TEL_STR,
		      pjsip_BRANCH_STR,
		      pjsip_TTL_STR,
		      pjsip_PNAME_STR,
		      pjsip_Q_STR,
		      pjsip_EXPIRES_STR,
		      pjsip_TAG_STR;

/*
 * Parser utilities.
 */
enum
{
    PJSIP_PARSE_REMOVE_QUOTE = 1,
};

void pjsip_parse_param_imp(  pj_scanner *scanner,
			     pj_str_t *pname, pj_str_t *pvalue,
			     unsigned opt);
void pjsip_concat_param_imp( pj_str_t *param, pj_pool_t *pool, 
			 const pj_str_t *pname, const pj_str_t *pvalue, int sepchar);
void pjsip_parse_end_hdr_imp ( pj_scanner *scanner );

/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJSIP_SIP_PARSER_H__ */

