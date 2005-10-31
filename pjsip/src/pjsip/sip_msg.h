/* $Header: /pjproject/pjsip/src/pjsip/sip_msg.h 13    6/22/05 12:27a Bennylp $ */
#ifndef __PJSIP_SIP_MSG_H__
#define __PJSIP_SIP_MSG_H__

/**
 * @file sip_msg.h
 * @brief SIP Message Structure.
 */

#include <pjsip/sip_types.h>
#include <pjsip/sip_uri.h>
#include <pj/list.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_MSG SIP Message Structure
 * @ingroup PJSIP
 * @{
 */

///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJSIP_MSG_METHOD Methods
 * @brief Method names and manipulation.
 * @ingroup PJSIP_MSG
 * @{
 */

/**
 * This enumeration declares SIP methods as described by RFC3261. Additional
 * methods do exist, and they are described by corresponding RFCs for the SIP
 * extentensions. Since they won't alter the characteristic of the processing
 * of the message, they don't need to be explicitly mentioned here.
 */
typedef enum pjsip_method_e
{
    /** INVITE method, for establishing dialogs. */
    PJSIP_INVITE_METHOD,

    /** CANCEL method, for cancelling request. */
    PJSIP_CANCEL_METHOD,

    /** ACK method, for acknowledging final response to INVITE. */
    PJSIP_ACK_METHOD,

    /** BYE method, for terminating dialog. */
    PJSIP_BYE_METHOD,

    /** REGISTER method. */
    PJSIP_REGISTER_METHOD,

    /** OPTIONS method, for querying remote capabilities. */
    PJSIP_OPTIONS_METHOD,

    /** Other method, which means that the method name itself will be stored
        elsewhere. */
    PJSIP_OTHER_METHOD,

} pjsip_method_e;



/**
 * This structure represents a SIP method.
 * Application must always use either #pjsip_method_init or #pjsip_method_set
 * to make sure that method name is initialized correctly. This way, the name
 * member will always contain a valid method string regardless whether the ID
 * is recognized or not.
 */
typedef struct pjsip_method
{
    pjsip_method_e id;	    /**< Method ID, from \a pjsip_method_e. */
    pj_str_t	   name;    /**< Method name, which will always contain the 
			         method string. */
} pjsip_method;


/** 
 * Initialize the method structure from a string. 
 * This function will check whether the method is a known method then set
 * both the id and name accordingly.
 *
 * @param m	The method to initialize.
 * @param pool	Pool where memory allocation will be allocated from, if required.
 * @param str	The method string.
 */
PJ_DECL(void) pjsip_method_init( pjsip_method *m, 
				 pj_pool_t *pool, 
				 const pj_str_t *str);

/** 
 * Initialize the method structure from a string, without cloning the string.
 * See #pjsip_method_init.
 *
 * @param m	The method structure to be initialized.
 * @param str	The method string.
 */
PJ_DECL(void) pjsip_method_init_np( pjsip_method *m,
				    pj_str_t *str);

/** 
 * Set the method with the predefined method ID. 
 * This function will also set the name member of the structure to the correct
 * string according to the method.
 *
 * @param m	The method structure.
 * @param id	The method ID.
 */
PJ_DECL(void) pjsip_method_set( pjsip_method *m, pjsip_method_e id );


/** 
 * Copy one method structure to another. If the method is of the known methods,
 * then memory allocation is not required.
 *
 * @param pool	    Pool to allocate memory from, if required.
 * @param method    The destination method to copy to.
 * @param rhs	    The source method to copy from.
 */
PJ_DECL(void) pjsip_method_copy( pj_pool_t *pool,
				 pjsip_method *method,
				 const pjsip_method *rhs );

/** 
 * Compare one method with another, and conveniently determine whether the 
 * first method is equal, less than, or greater than the second method.
 *
 * @param m1	The first method.
 * @param m2	The second method.
 *
 * @return	Zero if equal, otherwise will return -1 if less or +1 if greater.
 */
PJ_DECL(int) pjsip_method_cmp( const pjsip_method *m1, const pjsip_method *m2);

/**
 * @}
 */

///////////////////////////////////////////////////////////////////////////////
/** 
 * @defgroup PJSIP_MSG_HDR Header Fields General Structure.
 * @brief General Header Fields Structure.
 * @ingroup PJSIP_MSG
 * @{
 */

/**
 * Header types, as defined by RFC3261.
 */
typedef enum pjsip_hdr_e
{
    /*
     * These are the headers documented in RFC3261. Headers not documented
     * there must have type PJSIP_H_OTHER, and the header type itself is 
     * recorded in the header name string.
     *
     * DO NOT CHANGE THE VALUE/ORDER OF THE HEADER IDs!!!.
     */
    PJSIP_H_ACCEPT,
    PJSIP_H_ACCEPT_ENCODING_UNIMP,
    PJSIP_H_ACCEPT_LANGUAGE_UNIMP,
    PJSIP_H_ALERT_INFO_UNIMP,
    PJSIP_H_ALLOW,
    PJSIP_H_AUTHENTICATION_INFO_UNIMP,
    PJSIP_H_AUTHORIZATION,
    PJSIP_H_CALL_ID,
    PJSIP_H_CALL_INFO_UNIMP,
    PJSIP_H_CONTACT,
    PJSIP_H_CONTENT_DISPOSITION_UNIMP,
    PJSIP_H_CONTENT_ENCODING_UNIMP,
    PJSIP_H_CONTENT_LANGUAGE_UNIMP,
    PJSIP_H_CONTENT_LENGTH,
    PJSIP_H_CONTENT_TYPE,
    PJSIP_H_CSEQ,
    PJSIP_H_DATE_UNIMP,
    PJSIP_H_ERROR_INFO_UNIMP,
    PJSIP_H_EXPIRES,
    PJSIP_H_FROM,
    PJSIP_H_IN_REPLY_TO_UNIMP,
    PJSIP_H_MAX_FORWARDS,
    PJSIP_H_MIME_VERSION_UNIMP,
    PJSIP_H_MIN_EXPIRES,
    PJSIP_H_ORGANIZATION_UNIMP,
    PJSIP_H_PRIORITY_UNIMP,
    PJSIP_H_PROXY_AUTHENTICATE,
    PJSIP_H_PROXY_AUTHORIZATION,
    PJSIP_H_PROXY_REQUIRE_UNIMP,
    PJSIP_H_RECORD_ROUTE,
    PJSIP_H_REPLY_TO_UNIMP,
    PJSIP_H_REQUIRE,
    PJSIP_H_RETRY_AFTER,
    PJSIP_H_ROUTE,
    PJSIP_H_SERVER_UNIMP,
    PJSIP_H_SUBJECT_UNIMP,
    PJSIP_H_SUPPORTED,
    PJSIP_H_TIMESTAMP_UNIMP,
    PJSIP_H_TO,
    PJSIP_H_UNSUPPORTED,
    PJSIP_H_USER_AGENT_UNIMP,
    PJSIP_H_VIA,
    PJSIP_H_WARNING_UNIMP,
    PJSIP_H_WWW_AUTHENTICATE,

    PJSIP_H_OTHER,

} pjsip_hdr_e;

/**
 * This structure provides the pointer to basic functions that are needed
 * for generic header operations. All header fields will have pointer to
 * this structure, so that they can be manipulated uniformly.
 */
typedef struct pjsip_hdr_vptr
{
    /** 
     * Function to clone the header. 
     *
     * @param pool  Memory pool to allocate the new header.
     * @param hdr   Header to clone.
     *
     * @return A new instance of the header.
     */
    void *(*clone)(pj_pool_t *pool, const void *hdr);

    /** 
     * Pointer to function to shallow clone the header. 
     * Shallow cloning will just make a memory copy of the original header,
     * thus all pointers in original header will be kept intact. Because the
     * function does not need to perform deep copy, the operation should be
     * faster, but the application must make sure that the original header
     * is still valid throughout the lifetime of new header.
     *
     * @param pool  Memory pool to allocate the new header.
     * @param hdr   The header to clone.
     */
    void *(*shallow_clone)(pj_pool_t *pool, const void *hdr);

    /** Pointer to function to print the header to the specified buffer.
     *	Returns the length of string written, or -1 if the remaining buffer
     *	is not enough to hold the header.
     *
     *  @param hdr  The header to print.
     *  @param buf  The buffer.
     *  @param len  The size of the buffer.
     *
     *  @return	    The size copied to buffer, or -1 if there's not enough space.
     */
    int (*print_on)(void *hdr, char *buf, pj_size_t len);

} pjsip_hdr_vptr;


/**
 * Generic fields for all SIP headers are declared using this macro, to make
 * sure that all headers will have exactly the same layout in their start of
 * the storage. This behaves like C++ inheritance actually.
 */
#define PJSIP_DECL_HDR_MEMBER(hdr)   \
    /** List members. */	\
    PJ_DECL_LIST_MEMBER(hdr)	\
    /** Header type */		\
    pjsip_hdr_e	    type;	\
    /** Header name. */		\
    pj_str_t	    name;	\
    /** Header short name version. */	\
    pj_str_t	    sname;		\
    /** Virtual function table. */	\
    pjsip_hdr_vptr *vptr;


/**
 * Generic SIP header structure, for generic manipulation for headers in the
 * message. All header fields can be typecasted to this type.
 */
typedef struct pjsip_hdr
{
    PJSIP_DECL_HDR_MEMBER(struct pjsip_hdr)
} pjsip_hdr;


/**
 * This generic function will clone any header, by calling "clone" function
 * in header's virtual function table.
 *
 * @param pool	    The pool to allocate memory from.
 * @param hdr	    The header to clone.
 *
 * @return	    A new instance copied from the original header.
 */
PJ_DECL(void*) pjsip_hdr_clone( pj_pool_t *pool, const void *hdr );


/**
 * This generic function will clone any header, by calling "shallow_clone" 
 * function in header's virtual function table.
 *
 * @param pool	    The pool to allocate memory from.
 * @param hdr	    The header to clone.
 *
 * @return	    A new instance copied from the original header.
 */
PJ_DECL(void*) pjsip_hdr_shallow_clone( pj_pool_t *pool, const void *hdr );

/**
 * This generic function will print any header, by calling "print" 
 * function in header's virtual function table.
 *
 * @param hdr  The header to print.
 * @param buf  The buffer.
 * @param len  The size of the buffer.
 *
 * @return	The size copied to buffer, or -1 if there's not enough space.
 */
PJ_DECL(int) pjsip_hdr_print_on( void *hdr, char *buf, pj_size_t len);

/**
 * @}
 */

///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJSIP_MSG_LINE Request and Status Line.
 * @brief Request and status line structures and manipulation.
 * @ingroup PJSIP_MSG
 * @{
 */

/**
 * This structure describes SIP request line.
 */
typedef struct pjsip_request_line 
{
    pjsip_method    method; /**< Method for this request line. */
    pjsip_uri *uri;    /**< URI for this request line. */
} pjsip_request_line;


/**
 * This structure describes SIP status line.
 */
typedef struct pjsip_status_line 
{
    int		code;	    /**< Status code. */
    pj_str_t	reason;	    /**< Reason string. */
} pjsip_status_line;


/**
 * This enumeration lists standard SIP status codes according to RFC 3261.
 * In addition, it also declares new status class 7xx for errors generated
 * by the stack. This status class however should not get transmitted on the 
 * wire.
 */
typedef enum pjsip_status_code
{
    PJSIP_SC_TRYING = 100,
    PJSIP_SC_RINGING = 180,
    PJSIP_SC_CALL_BEING_FORWARDED = 181,
    PJSIP_SC_QUEUED = 182,
    PJSIP_SC_PROGRESS = 183,

    PJSIP_SC_OK = 200,

    PJSIP_SC_MULTIPLE_CHOICES = 300,
    PJSIP_SC_MOVED_PERMANENTLY = 301,
    PJSIP_SC_MOVED_TEMPORARILY = 302,
    PJSIP_SC_USE_PROXY = 305,
    PJSIP_SC_ALTERNATIVE_SERVICE = 380,

    PJSIP_SC_BAD_REQUEST = 400,
    PJSIP_SC_UNAUTHORIZED = 401,
    PJSIP_SC_PAYMENT_REQUIRED = 402,
    PJSIP_SC_FORBIDDEN = 403,
    PJSIP_SC_NOT_FOUND = 404,
    PJSIP_SC_METHOD_NOT_ALLOWED = 405,
    PJSIP_SC_NOT_ACCEPTABLE = 406,
    PJSIP_SC_PROXY_AUTHENTICATION_REQUIRED = 407,
    PJSIP_SC_REQUEST_TIMEOUT = 408,
    PJSIP_SC_GONE = 410,
    PJSIP_SC_REQUEST_ENTITY_TOO_LARGE = 413,
    PJSIP_SC_REQUEST_URI_TOO_LONG = 414,
    PJSIP_SC_UNSUPPORTED_MEDIA_TYPE = 415,
    PJSIP_SC_UNSUPPORTED_URI_SCHEME = 416,
    PJSIP_SC_BAD_EXTENSION = 420,
    PJSIP_SC_EXTENSION_REQUIRED = 421,
    PJSIP_SC_INTERVAL_TOO_BRIEF = 423,
    PJSIP_SC_TEMPORARILY_UNAVAILABLE = 480,
    PJSIP_SC_CALL_TSX_DOES_NOT_EXIST = 481,
    PJSIP_SC_LOOP_DETECTED = 482,
    PJSIP_SC_TOO_MANY_HOPS = 483,
    PJSIP_SC_ADDRESS_INCOMPLETE = 484,
    PJSIP_AC_AMBIGUOUS = 485,
    PJSIP_SC_BUSY_HERE = 486,
    PJSIP_SC_REQUEST_TERMINATED = 487,
    PJSIP_SC_NOT_ACCEPTABLE_HERE = 488,
    PJSIP_SC_REQUEST_PENDING = 491,
    PJSIP_SC_UNDECIPHERABLE = 493,

    PJSIP_SC_INTERNAL_SERVER_ERROR = 500,
    PJSIP_SC_NOT_IMPLEMENTED = 501,
    PJSIP_SC_BAD_GATEWAY = 502,
    PJSIP_SC_SERVICE_UNAVAILABLE = 503,
    PJSIP_SC_SERVER_TIMEOUT = 504,
    PJSIP_SC_VERSION_NOT_SUPPORTED = 505,
    PJSIP_SC_MESSAGE_TOO_LARGE = 513,

    PJSIP_SC_BUSY_EVERYWHERE = 600,
    PJSIP_SC_DECLINE = 603,
    PJSIP_SC_DOES_NOT_EXIST_ANYWHERE = 604,
    PJSIP_SC_NOT_ACCEPTABLE_ANYWHERE = 606,

    PJSIP_SC_TSX_TIMEOUT = 701,
    PJSIP_SC_TSX_RESOLVE_ERROR = 702,
    PJSIP_SC_TSX_TRANSPORT_ERROR = 703,

} pjsip_status_code;

/**
 * Get the default status text for the status code.
 *
 * @param status_code	    SIP Status Code
 *
 * @return		    textual message for the status code.
 */ 
PJ_DECL(const pj_str_t*) pjsip_get_status_text(int status_code);

/**
 * This macro returns non-zero (TRUE) if the specified status_code is
 * in the same class as the code_class.
 *
 * @param status_code	The status code.
 * @param code_class	The status code in the class (for example 100, 200).
 */
#define PJSIP_IS_STATUS_IN_CLASS(status_code, code_class)    \
	    (status_code/100 == code_class/100)

/**
 * @}
 */

///////////////////////////////////////////////////////////////////////////////
/**
 * @addtogroup PJSIP_MSG_MEDIA Media Type
 * @brief Media type definitions and manipulations.
 * @ingroup PJSIP_MSG
 * @{
 */

/**
 * This structure describes SIP media type, as used for example in 
 * Accept and Content-Type header..
 */
typedef struct pjsip_media_type
{
    pj_str_t type;	    /**< Media type. */
    pj_str_t subtype;	    /**< Media subtype. */
    pj_str_t param;	    /**< Media type parameters (concatenated). */
} pjsip_media_type;

/**
 * @}
 */

///////////////////////////////////////////////////////////////////////////////
/**
 * @addtogroup PJSIP_MSG_BODY Message Body
 * @brief SIP message body structures and manipulation.
 * @ingroup PJSIP_MSG
 * @{
 */

/**
 * Generic abstraction to message body.
 * When an incoming message is parsed (pjsip_parse_msg()), the parser fills in
 * all members with the appropriate value. The 'data' and 'len' member will
 * describe portion of incoming packet which denotes the message body.
 * When application needs to attach message body to outgoing SIP message, it
 * must fill in all members of this structure. 
 */
typedef struct pjsip_msg_body
{
    /** MIME content type. 
     *  For incoming messages, the parser will fill in this member with the
     *  content type found in Content-Type header.
     *
     *  For outgoing messages, application must fill in this member with
     *  appropriate value, because the stack will generate Content-Type header
     *  based on the value specified here.
     */
    pjsip_media_type content_type;

    /** Pointer to buffer which holds the message body data. 
     *  For incoming messages, the parser will fill in this member with the
     *  pointer to the body string.
     *
     *  When sending outgoing message, this member doesn't need to point to the
     *  actual message body string. It can be assigned with arbitrary pointer,
     *  because the value will only need to be understood by the print_body()
     *  function. The stack itself will not try to interpret this value, but
     *  instead will always call the print_body() whenever it needs to get the
     *  actual body string.
     */
    void *data;

    /** The length of the data. 
     *  For incoming messages, the parser will fill in this member with the
     *  actual length of message body.
     *
     *  When sending outgoing message, again just like the "data" member, the
     *  "len" member doesn't need to point to the actual length of the body 
     *  string.
     */
    unsigned len;

    /** Pointer to function to print this message body. 
     *  Application must set a proper function here when sending outgoing 
     *  message.
     *
     *  @param msg_body	    This structure itself.
     *  @param buf	    The buffer.
     *  @param size	    The buffer size.
     *
     *  @return		    The length of the string printed, or -1 if there is
     *			    not enough space in the buffer to print the whole
     *			    message body.
     */
    int (*print_body)(struct pjsip_msg_body *msg_body, 
		      char *buf, pj_size_t size);

} pjsip_msg_body;

/**
 * General purpose function to textual data in a SIP body. Attach this function
 * in a SIP message body only if the data in pjsip_msg_body is a textual 
 * message ready to be embedded in a SIP message. If the data in the message
 * body is not a textual body, then application must supply a custom function
 * to print that body.
 *
 * @param msg_body	The message body.
 * @param buf		Buffer to copy the message body to.
 * @param size		The size of the buffer.
 *
 * @return		The length copied to the buffer, or -1.
 */
PJ_DECL(int) pjsip_print_text_body( pjsip_msg_body *msg_body, 
				    char *buf, pj_size_t size);

/**
 * @}
 */

///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJSIP_MSG_MSG Message Structure
 * @brief Message structure and operations.
 * @ingroup PJSIP_MSG
 * @{
 */

/**
 * Message type (request or response).
 */
typedef enum pjsip_msg_type_e
{
    PJSIP_REQUEST_MSG,	    /**< Indicates request message. */
    PJSIP_RESPONSE_MSG,	    /**< Indicates response message. */
} pjsip_msg_type_e;


/**
 * This structure describes a SIP message.
 */
struct pjsip_msg
{
    /** Message type (ie request or response). */
    pjsip_msg_type_e  type;

    /** The first line of the message can be either request line for request
     *	messages, or status line for response messages. It is represented here
     *  as a union.
     */
    union
    {
	/** Request Line. */
	struct pjsip_request_line   req;

	/** Status Line. */
	struct pjsip_status_line    status;
    } line;

    /** List of message headers. */
    pjsip_hdr hdr;

    /** Pointer to message body, or NULL if no message body is attached to
     *	this mesage. 
     */
    pjsip_msg_body *body;
};


/** 
 * Create new request or response message.
 *
 * @param pool	    The pool.
 * @param type	    Message type.
 * @return	    New message, or THROW exception if failed.
 */
PJ_DECL(pjsip_msg*)  pjsip_msg_create( pj_pool_t *pool, pjsip_msg_type_e type);

/** 
 * Find a header in the message by the header type.
 *
 * @param msg	    The message.
 * @param type	    The header type to find.
 * @param start	    The first header field where the search should begin.
 *		    If NULL is specified, then the search will begin from the
 *		    first header, otherwise the search will begin at the
 *		    specified header.
 *
 * @return	    The header field, or NULL if no header with the specified 
 *		    type is found.
 */
PJ_DECL(void*)  pjsip_msg_find_hdr( pjsip_msg *msg, 
				    pjsip_hdr_e type, void *start);

/** 
 * Find a header in the message by its name.
 *
 * @param msg	    The message.
 * @param name	    The header name to find.
 * @param start	    The first header field where the search should begin.
 *		    If NULL is specified, then the search will begin from the
 *		    first header, otherwise the search will begin at the
 *		    specified header.
 *
 * @return	    The header field, or NULL if no header with the specified 
 *		    type is found.
 */
PJ_DECL(void*)  pjsip_msg_find_hdr_by_name( pjsip_msg *msg, 
					    const pj_str_t *name, void *start);

/** 
 * Find and remove a header in the message. 
 *
 * @param msg	    The message.
 * @param hdr	    The header type to find.
 * @param start	    The first header field where the search should begin,
 *		    or NULL to search from the first header in the message.
 *
 * @return	    The header field, or NULL if not found.
 */
PJ_DECL(void*)  pjsip_msg_find_remove_hdr( pjsip_msg *msg, 
					   pjsip_hdr_e hdr, void *start);

/** 
 * Add a header to the message, putting it last in the header list.
 *
 * @param msg	    The message.
 * @param hdr	    The header to add.
 *
 * @bug Once the header is put in a list (or message), it can not be put in 
 *      other list (or message). Otherwise Real Bad Thing will happen.
 */
PJ_IDECL(void) pjsip_msg_add_hdr( pjsip_msg *msg, pjsip_hdr *hdr );

/** 
 * Add header field to the message, putting it in the front of the header list.
 *
 * @param msg	The message.
 * @param hdr	The header to add.
 *
 * @bug Once the header is put in a list (or message), it can not be put in 
 *      other list (or message). Otherwise Real Bad Thing will happen.
 */
PJ_IDECL(void) pjsip_msg_insert_first_hdr( pjsip_msg *msg, pjsip_hdr *hdr );

/** 
 * Print the message to the specified buffer. 
 *
 * @param msg	The message to print.
 * @param buf	The buffer
 * @param size	The size of the buffer.
 *
 * @return	The length of the printed characters (in bytes), or NEGATIVE
 *		value if the message is too large for the specified buffer.
 */
PJ_DECL(int) pjsip_msg_print( pjsip_msg *msg, char *buf, pj_size_t size);

/**
 * @}
 */

///////////////////////////////////////////////////////////////////////////////
/**
 * @addtogroup PJSIP_MSG_HDR_GEN Header Field: Generic
 * @brief Generic header field which contains header name and value.
 * @ingroup PJSIP_MSG
 * @{
 */

/**
 * Generic SIP header, which contains hname and a string hvalue.
 * Note that this header is not supposed to be used as 'base' class for headers.
 */
typedef struct pjsip_generic_string_hdr
{
    PJSIP_DECL_HDR_MEMBER(struct pjsip_generic_string_hdr) /**< Standard header field. */
    pj_str_t hvalue;				    /**< hvalue */
} pjsip_generic_string_hdr;


/**
 * Create a new instance of generic header. A generic header can have an
 * arbitrary header name.
 *
 * @param pool	    The pool.
 * @param hname	    The header name to be assigned to the header, or NULL to
 *		    assign the header name with some string.
 *
 * @return	    The header, or THROW exception.
 */
PJ_DECL(pjsip_generic_string_hdr*) pjsip_generic_string_hdr_create( pj_pool_t *pool, 
						      const pj_str_t *hname );

/**
 * Create a generic header along with the text content.
 *
 * @param pool	    The pool.
 * @param hname	    The header name.
 * @param hvalue    The header text content.
 *
 * @return	    The header instance.
 */
PJ_DECL(pjsip_generic_string_hdr*) 
pjsip_generic_string_hdr_create_with_text( pj_pool_t *pool,
					   const pj_str_t *hname,
					   const pj_str_t *hvalue);

/**
 * @}
 */

///////////////////////////////////////////////////////////////////////////////
/**
 * @addtogroup PJSIP_MSG_HDR_GEN_INT Header Field: Generic Integer
 * @brief Generic header field which contains header name and value.
 * @ingroup PJSIP_MSG
 * @{
 */

/**
 * Generic SIP header, which contains hname and a string hvalue.
 */
typedef struct pjsip_generic_int_hdr
{
    PJSIP_DECL_HDR_MEMBER(struct pjsip_generic_int_hdr) /**< Standard header field. */
    pj_int32_t ivalue;				    /**< ivalue */
} pjsip_generic_int_hdr;


/**
 * Create a new instance of generic header. A generic header can have an
 * arbitrary header name.
 *
 * @param pool	    The pool.
 * @param hname	    The header name to be assigned to the header, or NULL to
 *		    assign the header name with some string.
 *
 * @return	    The header, or THROW exception.
 */
PJ_DECL(pjsip_generic_int_hdr*) pjsip_generic_int_hdr_create( pj_pool_t *pool, 
						      const pj_str_t *hname );

/**
 * Create a generic header along with the value.
 *
 * @param pool	    The pool.
 * @param hname	    The header name.
 * @param value     The header value content.
 *
 * @return	    The header instance.
 */
PJ_DECL(pjsip_generic_int_hdr*) 
pjsip_generic_int_hdr_create_with_value( pj_pool_t *pool,
					 const pj_str_t *hname,
					 int value);

/**
 * @}
 */

///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJSIP_MSG_HDR_GENERIC_LIST Header Field: Generic string list.
 * @brief Header with list of strings separated with comma
 * @ingroup PJSIP_MSG
 * @{
 */

/** Maximum elements in the header array. */
#define PJSIP_GENERIC_ARRAY_MAX_COUNT	32

typedef struct pjsip_generic_array_hdr
{
    PJSIP_DECL_HDR_MEMBER(struct pjsip_generic_array_hdr)
    unsigned	count;					/**< Number of elements. */
    pj_str_t	values[PJSIP_GENERIC_ARRAY_MAX_COUNT];	/**< Elements.		 */
} pjsip_generic_array_hdr;

/**
 * Create generic array header.
 *
 * @param pool	    Pool to allocate memory from.
 *
 * @return	    New generic array header.
 */
PJ_DECL(pjsip_generic_array_hdr*) pjsip_generic_array_create(pj_pool_t *pool,
							     const pj_str_t *hnames);

/**
 * @}
 */

///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJSIP_MSG_HDR_ACCEPT Header Field: Accept
 * @brief Accept header field.
 * @ingroup PJSIP_MSG
 * @{
 */
/** Accept header. */
typedef pjsip_generic_array_hdr pjsip_accept_hdr;

/** Maximum fields in Accept header. */
#define PJSIP_MAX_ACCEPT_COUNT	PJSIP_GENERIC_ARRAY_MAX_COUNT

/**
 * Create new Accept header instance.
 *
 * @param pool	    The pool.
 *
 * @return	    New Accept header instance.
 */
PJ_DECL(pjsip_accept_hdr*) pjsip_accept_hdr_create(pj_pool_t *pool);


/**
 * @}
 */

///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJSIP_MSG_HDR_ALLOW Header Field: Allow
 * @brief Allow header field.
 * @ingroup PJSIP_MSG
 * @{
 */
typedef pjsip_generic_array_hdr pjsip_allow_hdr;

/**
 * Create new Allow header instance.
 *
 * @param pool	    The pool.
 *
 * @return	    New Allow header instance.
 */
PJ_DECL(pjsip_allow_hdr*) pjsip_allow_hdr_create(pj_pool_t *pool);


/**
 * @}
 */

///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJSIP_MSG_HDR_CID Header Field: Call-ID
 * @brief Call-ID header field.
 * @ingroup PJSIP_MSG
 * @{
 */
/**
 * Call-ID header.
 */
typedef struct pjsip_cid_hdr
{
    PJSIP_DECL_HDR_MEMBER(struct pjsip_cid_hdr)
    pj_str_t id;	    /**< Call-ID string. */
} pjsip_cid_hdr;


/**
 * Create new Call-ID header.
 *
 * @param pool	The pool.
 *
 * @return	new Call-ID header.
 */
PJ_DECL(pjsip_cid_hdr*) pjsip_cid_hdr_create( pj_pool_t *pool );


/**
 * @}
 */

///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJSIP_MSG_HDR_CLEN Header Field: Content-Length
 * @brief Content-Length header field.
 * @ingroup PJSIP_MSG
 * @{
 */
/**
 * Content-Length header.
 */
typedef struct pjsip_clen_hdr
{
    PJSIP_DECL_HDR_MEMBER(struct pjsip_clen_hdr)
    int len;	/**< Content length. */
} pjsip_clen_hdr;

/**
 * Create new Content-Length header.
 *
 * @param pool	the pool.
 * @return	A new Content-Length header instance.
 */
PJ_DECL(pjsip_clen_hdr*) pjsip_clen_hdr_create( pj_pool_t *pool );

/**
 * @}
 */

///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJSIP_MSG_HDR_CSEQ Header Field: CSeq
 * @brief CSeq header field.
 * @ingroup PJSIP_MSG
 * @{
 */
/**
 * CSeq header.
 */
typedef struct pjsip_cseq_hdr
{
    PJSIP_DECL_HDR_MEMBER(struct pjsip_cseq_hdr)
    int		    cseq;	/**< CSeq number. */
    pjsip_method    method;	/**< CSeq method. */
} pjsip_cseq_hdr;


/** Create new  CSeq header. 
 *
 *  @param pool	The pool.
 *  @return A new CSeq header instance.
 */
PJ_DECL(pjsip_cseq_hdr*) pjsip_cseq_hdr_create( pj_pool_t *pool );

/**
 * @}
 */

///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJSIP_MSG_HDR_CONTACT Header Field: Contact
 * @brief Contact header field.
 * @ingroup PJSIP_MSG
 * @{
 */
/**
 * Contact header.
 * In this library, contact header only contains single URI. If a message has
 * multiple URI in the Contact header, the URI will be put in separate Contact
 * headers.
 */
typedef struct pjsip_contact_hdr
{
    PJSIP_DECL_HDR_MEMBER(struct pjsip_contact_hdr)
    int		    star;	    /**< The contact contains only a '*' character */
    pjsip_uri *uri;	    /**< URI in the contact. */
    int		    q1000;	    /**< The "q" value times 1000 (to avoid float) */
    pj_int32_t	    expires;	    /**< Expires parameter, otherwise -1 if not present. */
    pj_str_t	    other_param;    /**< Other parameters, concatenated in a single string. */
} pjsip_contact_hdr;


/**
 * Create a new Contact header.
 *
 * @param pool	The pool.
 * @return	A new instance of Contact header.
 */
PJ_DECL(pjsip_contact_hdr*) pjsip_contact_hdr_create( pj_pool_t *pool );

/**
 * @}
 */

///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJSIP_MSG_HDR_CTYPE Header Field: Content-Type
 * @brief Content-Type header field.
 * @ingroup PJSIP_MSG
 * @{
 */
/**
 * Content-Type.
 */
typedef struct pjsip_ctype_hdr
{
    PJSIP_DECL_HDR_MEMBER(struct pjsip_ctype_hdr)
    pjsip_media_type media; /**< Media type. */
} pjsip_ctype_hdr;


/**
 * Create a nwe Content Type header.
 *
 * @param pool	The pool.
 * @return	A new Content-Type header.
 */
PJ_DECL(pjsip_ctype_hdr*) pjsip_ctype_hdr_create( pj_pool_t *pool );

/**
 * @}
 */

///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJSIP_MSG_HDR_EXPIRES Header Field: Expires
 * @brief Expires header field.
 * @ingroup PJSIP_MSG
 * @{
 */
/** Expires header. */
typedef pjsip_generic_int_hdr pjsip_expires_hdr;

/**
 * Create a new Expires header.
 *
 * @param pool	The pool.
 * @return	A new Expires header.
 */
PJ_DECL(pjsip_expires_hdr*) pjsip_expires_hdr_create( pj_pool_t *pool );

/**
 * @}
 */

///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJSIP_MSG_HDR_FROMTO Header Field: From/To
 * @brief From and To header field.
 * @ingroup PJSIP_MSG
 * @{
 */
/**
 * To or From header.
 */
typedef struct pjsip_fromto_hdr
{
    PJSIP_DECL_HDR_MEMBER(struct pjsip_fromto_hdr)
    pjsip_uri  *uri;	    /**< URI in From/To header. */
    pj_str_t	     tag;	    /**< Header "tag" parameter. */
    pj_str_t	     other_param;   /**< Other params, concatenated as a single string. */
} pjsip_fromto_hdr;

/** Alias for From header. */
typedef pjsip_fromto_hdr pjsip_from_hdr;

/** Alias for To header. */
typedef pjsip_fromto_hdr pjsip_to_hdr;

/**
 * Create a From header.
 *
 * @param pool	The pool.
 * @return	New instance of From header.
 */
PJ_DECL(pjsip_from_hdr*) pjsip_from_hdr_create( pj_pool_t *pool );

/**
 * Create a To header.
 *
 * @param pool	The pool.
 * @return	New instance of To header.
 */
PJ_DECL(pjsip_to_hdr*)   pjsip_to_hdr_create( pj_pool_t *pool );

/**
 * Convert the header to a From header.
 *
 * @param pool	The pool.
 * @return	"From" header.
 */
PJ_DECL(pjsip_from_hdr*) pjsip_fromto_set_from( pjsip_fromto_hdr *hdr );

/**
 * Convert the header to a To header.
 *
 * @param pool	The pool.
 * @return	"To" header.
 */
PJ_DECL(pjsip_to_hdr*)   pjsip_fromto_set_to( pjsip_fromto_hdr *hdr );

/**
 * @}
 */


///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJSIP_MSG_HDR_MAX_FORWARDS Header Field: Max-Forwards
 * @brief Max-Forwards header field.
 * @ingroup PJSIP_MSG
 * @{
 */
typedef pjsip_generic_int_hdr pjsip_max_forwards_hdr;

/**
 * Create new Max-Forwards header instance.
 *
 * @param pool	    The pool.
 *
 * @return	    New Max-Forwards header instance.
 */
PJ_DECL(pjsip_max_forwards_hdr*) pjsip_max_forwards_hdr_create(pj_pool_t *pool);


/**
 * @}
 */


///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJSIP_MSG_HDR_MIN_EXPIRES Header Field: Min-Expires
 * @brief Min-Expires header field.
 * @ingroup PJSIP_MSG
 * @{
 */
typedef pjsip_generic_int_hdr pjsip_min_expires_hdr;

/**
 * Create new Max-Forwards header instance.
 *
 * @param pool	    The pool.
 *
 * @return	    New Max-Forwards header instance.
 */
PJ_DECL(pjsip_min_expires_hdr*) pjsip_min_expires_hdr_create(pj_pool_t *pool);


/**
 * @}
 */


///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJSIP_MSG_HDR_ROUTING Header Field: Record-Route/Route
 * @brief Record-Route and Route header fields.
 * @ingroup PJSIP_MSG
 * @{
 */
/**
 * Record-Route and Route headers.
 */
typedef struct pjsip_routing_hdr
{
    PJSIP_DECL_HDR_MEMBER(struct pjsip_routing_hdr)  /**< Generic header fields. */
    pjsip_name_addr  name_addr;	  /**< The URL in the Route/Record-Route header. */
    pj_str_t	     other_param; /** Other parameter. */
} pjsip_routing_hdr;

/** Alias for Record-Route header. */
typedef pjsip_routing_hdr pjsip_rr_hdr;

/** Alias for Route header. */
typedef pjsip_routing_hdr pjsip_route_hdr;


/** 
 * Create new Record-Route header from the pool. 
 *
 * @param pool	The pool.
 * @return	A new instance of Record-Route header.
 */
PJ_DECL(pjsip_rr_hdr*)	    pjsip_rr_hdr_create( pj_pool_t *pool );

/** 
 * Create new Route header from the pool. 
 *
 * @param pool	The pool.
 * @return	A new instance of "Route" header.
 */
PJ_DECL(pjsip_route_hdr*)   pjsip_route_hdr_create( pj_pool_t *pool );

/** 
 * Convert generic routing header to Record-Route header. 
 *
 * @param r	The generic routing header, or a "Routing" header.
 * @return	Record-Route header.
 */
PJ_DECL(pjsip_rr_hdr*)	    pjsip_routing_hdr_set_rr( pjsip_routing_hdr *r );

/** 
 * Convert generic routing header to "Route" header. 
 *
 * @param r	The generic routing header, or a "Record-Route" header.
 * @return	"Route" header.
 */
PJ_DECL(pjsip_route_hdr*)   pjsip_routing_hdr_set_route( pjsip_routing_hdr *r );

/**
 * @}
 */

///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJSIP_MSG_HDR_REQUIRE Header Field: Require
 * @brief Require header field.
 * @ingroup PJSIP_MSG
 * @{
 */
typedef pjsip_generic_array_hdr pjsip_require_hdr;

/**
 * Create new Require header instance.
 *
 * @param pool	    The pool.
 *
 * @return	    New Require header instance.
 */
PJ_DECL(pjsip_require_hdr*) pjsip_require_hdr_create(pj_pool_t *pool);


/**
 * @}
 */


///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJSIP_MSG_HDR_RETRY_AFTER Header Field: Retry-After
 * @brief Retry-After header field.
 * @ingroup PJSIP_MSG
 * @{
 */
typedef pjsip_generic_int_hdr pjsip_retry_after_hdr;

/**
 * Create new Retry-After header instance.
 *
 * @param pool	    The pool.
 *
 * @return	    New Retry-After header instance.
 */
PJ_DECL(pjsip_retry_after_hdr*) pjsip_retry_after_hdr_create(pj_pool_t *pool);


/**
 * @}
 */

///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJSIP_MSG_HDR_SUPPORTED Header Field: Supported
 * @brief Supported header field.
 * @ingroup PJSIP_MSG
 * @{
 */
typedef pjsip_generic_array_hdr pjsip_supported_hdr;

/**
 * Create new Supported header instance.
 *
 * @param pool	    The pool.
 *
 * @return	    New Supported header instance.
 */
PJ_DECL(pjsip_supported_hdr*) pjsip_supported_hdr_create(pj_pool_t *pool);


/**
 * @}
 */

///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJSIP_MSG_HDR_UNSUPPORTED Header Field: Unsupported
 * @brief Unsupported header field.
 * @ingroup PJSIP_MSG
 * @{
 */
typedef pjsip_generic_array_hdr pjsip_unsupported_hdr;

/**
 * Create new Unsupported header instance.
 *
 * @param pool	    The pool.
 *
 * @return	    New Unsupported header instance.
 */
PJ_DECL(pjsip_unsupported_hdr*) pjsip_unsupported_hdr_create(pj_pool_t *pool);


/**
 * @}
 */


///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJSIP_MSG_HDR_VIA Header Field: Via
 * @brief Via header field.
 * @ingroup PJSIP_MSG
 * @{
 */
/**
 * SIP Via header.
 * In this implementation, Via header can only have one element in each header.
 * If a message arrives with multiple elements in a single Via, then they will
 * be split up into multiple Via headers.
 */
typedef struct pjsip_via_hdr
{
    PJSIP_DECL_HDR_MEMBER(struct pjsip_via_hdr)
    pj_str_t	     transport;	    /**< Transport type. */
    pjsip_host_port  sent_by;	    /**< Host and optional port */
    int		     ttl_param;	    /**< TTL parameter, or -1 if it's not specified. */
    int		     rport_param;   /**< "rport" parameter, 0 to specify without
					 port number, -1 means doesn't exist. */
    pj_str_t	     maddr_param;   /**< "maddr" parameter. */
    pj_str_t	     recvd_param;   /**< "received" parameter. */
    pj_str_t	     branch_param;  /**< "branch" parameter. */
    pj_str_t	     other_param;   /**< Other parameters, concatenated as single string. */
    pj_str_t	     comment;	    /**< Comment. */
} pjsip_via_hdr;

/**
 * Create a new Via header.
 *
 * @param pool	    The pool.
 * @return	    A new "Via" header instance.
 */
PJ_DECL(pjsip_via_hdr*) pjsip_via_hdr_create( pj_pool_t *pool );

/**
 * @}
 */

/**
 * @bug Once a header is put in the message, the header CAN NOT be put in
 *      other list. Solution:
 *	- always clone header in the message.
 *	- create a list node for each header in the message.
 */


///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJSIP_MSG_HDR_UNIMP Unimplemented Header Fields
 * @brief Unimplemented header fields.
 * @ingroup PJSIP_MSG
 * @{
 */
/** Accept-Encoding header. */
typedef pjsip_generic_string_hdr pjsip_accept_encoding_hdr;

/** Create Accept-Encoding header. */
#define pjsip_accept_encoding_hdr_create pjsip_generic_string_hdr_create

/** Accept-Language header. */
typedef pjsip_generic_string_hdr pjsip_accept_lang_hdr;

/** Create Accept-Language header. */
#define pjsip_accept_lang_hdr_create pjsip_generic_string_hdr_create

/** Alert-Info header. */
typedef pjsip_generic_string_hdr pjsip_alert_info_hdr;

/** Create Alert-Info header. */
#define pjsip_alert_info_hdr_create pjsip_generic_string_hdr_create

/** Authentication-Info header. */
typedef pjsip_generic_string_hdr pjsip_auth_info_hdr;

/** Create Authentication-Info header. */
#define pjsip_auth_info_hdr_create pjsip_generic_string_hdr_create

/** Call-Info header. */
typedef pjsip_generic_string_hdr pjsip_call_info_hdr;

/** Create Call-Info header. */
#define pjsip_call_info_hdr_create pjsip_generic_string_hdr_create

/** Content-Disposition header. */
typedef pjsip_generic_string_hdr pjsip_content_disposition_hdr;

/** Create Content-Disposition header. */
#define pjsip_content_disposition_hdr_create pjsip_generic_string_hdr_create

/** Content-Encoding header. */
typedef pjsip_generic_string_hdr pjsip_content_encoding_hdr;

/** Create Content-Encoding header. */
#define pjsip_content_encoding_hdr_create pjsip_generic_string_hdr_create

/** Content-Language header. */
typedef pjsip_generic_string_hdr pjsip_content_lang_hdr;

/** Create Content-Language header. */
#define pjsip_content_lang_hdr_create pjsip_generic_string_hdr_create

/** Date header. */
typedef pjsip_generic_string_hdr pjsip_date_hdr;

/** Create Date header. */
#define pjsip_date_hdr_create pjsip_generic_string_hdr_create

/** Error-Info header. */
typedef pjsip_generic_string_hdr pjsip_err_info_hdr;

/** Create Error-Info header. */
#define pjsip_err_info_hdr_create pjsip_generic_string_hdr_create

/** In-Reply-To header. */
typedef pjsip_generic_string_hdr pjsip_in_reply_to_hdr;

/** Create In-Reply-To header. */
#define pjsip_in_reply_to_hdr_create pjsip_generic_string_hdr_create

/** MIME-Version header. */
typedef pjsip_generic_string_hdr pjsip_mime_version_hdr;

/** Create MIME-Version header. */
#define pjsip_mime_version_hdr_create pjsip_generic_string_hdr_create

/** Organization header. */
typedef pjsip_generic_string_hdr pjsip_organization_hdr;

/** Create Organization header. */
#define pjsip_organization_hdr_create pjsip_genric_string_hdr_create

/** Priority header. */
typedef pjsip_generic_string_hdr pjsip_priority_hdr;

/** Create Priority header. */
#define pjsip_priority_hdr_create pjsip_generic_string_hdr_create

/** Proxy-Require header. */
typedef pjsip_generic_string_hdr pjsip_proxy_require_hdr;

/** Reply-To header. */
typedef pjsip_generic_string_hdr pjsip_reply_to_hdr;

/** Create Reply-To header. */
#define pjsip_reply_to_hdr_create pjsip_generic_string_hdr_create

/** Server header. */
typedef pjsip_generic_string_hdr pjsip_server_hdr;

/** Create Server header. */
#define pjsip_server_hdr_create pjsip_generic_string_hdr_create

/** Subject header. */
typedef pjsip_generic_string_hdr pjsip_subject_hdr;

/** Create Subject header. */
#define pjsip_subject_hdr_create pjsip_generic_string_hdr_create

/** Timestamp header. */
typedef pjsip_generic_string_hdr pjsip_timestamp_hdr;

/** Create Timestamp header. */
#define pjsip_timestamp_hdr_create pjsip_generic_string_hdr_create

/** User-Agent header. */
typedef pjsip_generic_string_hdr pjsip_user_agent_hdr;

/** Create User-Agent header. */
#define pjsip_user_agent_hdr_create pjsip_generic_string_hdr_create

/** Warning header. */
typedef pjsip_generic_string_hdr pjsip_warning_hdr;

/** Create Warning header. */
#define pjsip_warning_hdr_create pjsip_generic_string_hdr_create

/**
 * @}
 */

/**
 * @}  // PJSIP_MSG
 */

/*
 * Include inline definitions.
 */
#if PJ_FUNCTIONS_ARE_INLINED
#  include <pjsip/sip_msg_i.h>
#endif


PJ_END_DECL

#endif	/* __PJSIP_SIP_MSG_H__ */

