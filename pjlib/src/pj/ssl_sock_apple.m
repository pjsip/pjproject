/*
 * Copyright (C) 2019-2020 Teluu Inc. (http://www.teluu.com)
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

#include <pj/ssl_sock.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/rand.h>

/* Only build when PJ_HAS_SSL_SOCK and the implementation is Apple SSL. */
#if defined(PJ_HAS_SSL_SOCK) && PJ_HAS_SSL_SOCK != 0 && \
    (PJ_SSL_SOCK_IMP == PJ_SSL_SOCK_IMP_APPLE)

#define THIS_FILE               "ssl_sock_apple.m"

/* Set to 1 to enable debugging messages. */
#define SSL_DEBUG  0

#define SSL_SOCK_IMP_USE_CIRC_BUF
#define SSL_SOCK_IMP_USE_OWN_NETWORK

#include "ssl_sock_imp_common.h"
#include "ssl_sock_imp_common.c"

#include "TargetConditionals.h"

#include <err.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CommonCrypto/CommonDigest.h> 
#include <Foundation/NSLock.h>
#include <Network/Network.h>
#include <Security/Security.h>

/* IMPORTANT note from Apple's Concurrency Programming Guide doc:
 * "Because Grand Central Dispatch manages the relationship between the tasks
 * you provide and the threads on which those tasks run, you should generally
 * avoid calling POSIX thread routines from your task code"
 *
 * Since network events happen in a dispatch function block, we need to make
 * sure not to call any PJLIB functions there (not even pj_pool_alloc() nor
 * pj_log()). Instead, we will post those events to a singleton event manager
 * to be polled by ioqueue polling thread(s).
 */

/* Secure socket structure definition. */
typedef struct applessl_sock_t {
    pj_ssl_sock_t  		base;

    nw_listener_t 		listener;
    nw_listener_state_t		lis_state;
    nw_connection_t     	connection;
    nw_connection_state_t	con_state;
    dispatch_queue_t		queue;
    dispatch_semaphore_t 	ev_semaphore;

    SecTrustRef			trust;
    tls_ciphersuite_t		cipher;
    sec_identity_t		identity;
} applessl_sock_t;


/*
 *******************************************************************
 * Event manager
 *******************************************************************
 */
 
 typedef enum event_id
{
    EVENT_ACCEPT,
    EVENT_CONNECT,
    EVENT_VERIFY_CERT,
    EVENT_HANDSHAKE_COMPLETE,
    EVENT_DATA_READ,
    EVENT_DATA_SENT,
    EVENT_DISCARD
} event_id;

typedef struct event_t
{
    PJ_DECL_LIST_MEMBER(struct event_t);

    event_id 		 type;
    pj_ssl_sock_t 	*ssock;
    pj_bool_t		 async;

    union
    {
        struct
        {
            nw_connection_t	 newconn;
            pj_sockaddr		 src_addr;
            int			 src_addr_len;
            pj_status_t 	 status;
        } accept_ev;

        struct
        {
            pj_status_t 	 status;
        } connect_ev;

	struct
        {
            pj_status_t 	 status;
        } handshake_ev;

        struct
        {
            pj_ioqueue_op_key_t *send_key;
	    pj_ssize_t 		 sent;
        } data_sent_ev;

        struct
        {
	    void 		*data;
	    pj_size_t 		 size;
	    pj_status_t 	 status;
	    pj_size_t 		 remainder;
        } data_read_ev;

    } body;
} event_t;

typedef struct event_manager
{
    NSLock	*lock;
    event_t	 event_list;
    event_t	 free_event_list;
} event_manager;

static event_manager *event_mgr = NULL;

#if SSL_DEBUG
static pj_thread_desc queue_th_desc;
static pj_thread_t *queue_th;
#endif

/*
 *******************************************************************
 * Event manager's functions
 *******************************************************************
 */

static pj_status_t verify_cert(applessl_sock_t *assock, pj_ssl_cert_t *cert);

static void event_manager_destroy()
{
    event_manager *mgr = event_mgr;
    
    event_mgr = NULL;

    while (!pj_list_empty(&mgr->free_event_list)) {
    	event_t *event = mgr->free_event_list.next;
        pj_list_erase(event);
        free(event);
    }

    while (!pj_list_empty(&mgr->event_list)) {
    	event_t *event = mgr->event_list.next;
        pj_list_erase(event);
        free(event);
    }
    
    [mgr->lock release];

    free(mgr);
}

static pj_status_t event_manager_create()
{
    event_manager *mgr;
    
    if (event_mgr)
    	return PJ_SUCCESS;
    
    mgr = malloc(sizeof(event_manager));
    if (!mgr) return PJ_ENOMEM;

    mgr->lock = [[NSLock alloc]init];
    pj_list_init(&mgr->event_list);
    pj_list_init(&mgr->free_event_list);

    event_mgr = mgr;
    pj_atexit(&event_manager_destroy);

    return PJ_SUCCESS;
}

/* Post event to the event manager. If the event is posted
 * synchronously, the function will wait until the event is processed.
 */
static pj_status_t event_manager_post_event(pj_ssl_sock_t *ssock,
					    event_t *event_item,
					    pj_bool_t async)
{
    event_manager *mgr = event_mgr;
    event_t *event;

#if SSL_DEBUG
    if (!pj_thread_is_registered()) {
    	pj_bzero(queue_th_desc, sizeof(pj_thread_desc));
    	pj_thread_register("sslq", queue_th_desc, &queue_th);
    }
    PJ_LOG(3, (THIS_FILE, "Posting event %p %d", ssock, event_item->type));
#endif

    if (ssock->is_closing || !ssock->pool || !mgr)
    	return PJ_EGONE;

#if SSL_DEBUG
    PJ_LOG(3,(THIS_FILE, "Post event success %p %d",ssock, event_item->type));
#endif
    
    [mgr->lock lock];

    if (pj_list_empty(&mgr->free_event_list)) {
        event = malloc(sizeof(event_t));
    } else {
        event = mgr->free_event_list.next;
        pj_list_erase(event);
    }
    
    pj_memcpy(event, event_item, sizeof(event_t));
    event->ssock = ssock;
    event->async = async;
    pj_list_push_back(&mgr->event_list, event);
    
    [mgr->lock unlock];
    
    if (!async) {
	dispatch_semaphore_wait(((applessl_sock_t *)ssock)->ev_semaphore,
			        DISPATCH_TIME_FOREVER);
    }

    return PJ_SUCCESS;
}

/* Remove all events associated with the socket. */
static void event_manager_remove_events(pj_ssl_sock_t *ssock)
{
    event_t *event;
    	
    [event_mgr->lock lock];
    event = event_mgr->event_list.next;
    while (event != &event_mgr->event_list) {
    	event_t *event_ = event;

    	event = event->next;
    	if (event_->ssock == ssock) {
    	    pj_list_erase(event_);
	    /* If not async, signal the waiting socket */
	    if (!event_->async) {
	        applessl_sock_t * assock;
	        assock = (applessl_sock_t *)event_->ssock;
	    	dispatch_semaphore_signal(assock->ev_semaphore);
	    }
    	}
    }
    [event_mgr->lock unlock];
}

pj_status_t ssl_network_event_poll()
{
    if (!event_mgr)
    	return PJ_SUCCESS;

    while (!pj_list_empty(&event_mgr->event_list)) {
        pj_ssl_sock_t *ssock;
    	applessl_sock_t * assock;
    	event_t *event;
    	pj_bool_t ret = PJ_TRUE, add_ref = PJ_FALSE;
    	
    	[event_mgr->lock lock];
    	/* Check again, this time by holding the lock */
    	if (pj_list_empty(&event_mgr->event_list)) {
    	    [event_mgr->lock unlock];
    	    break;
    	}
    	event = event_mgr->event_list.next;
    	ssock = event->ssock;
	assock = (applessl_sock_t *)ssock;
    	pj_list_erase(event);

	if (ssock->is_closing || !ssock->pool ||
	    (!ssock->is_server && !assock->connection) ||
	    (ssock->is_server && !assock->listener))
	{
            PJ_LOG(3, (THIS_FILE, "Warning: Discarding SSL event type %d of "
            	       "a closing socket %p", event->type, ssock));
            event->type = EVENT_DISCARD;
	} else if (ssock->param.grp_lock) {
            if (pj_grp_lock_get_ref(ssock->param.grp_lock) > 0) {
                /* Prevent ssock from being destroyed while
                 * we are calling the callback.
                 */
                add_ref = PJ_TRUE;
                pj_grp_lock_add_ref(ssock->param.grp_lock);
            } else {
            	PJ_LOG(3, (THIS_FILE, "Warning: Discarding SSL event type %d "
            		   " of a destroyed socket %p", event->type, ssock));
                event->type = EVENT_DISCARD;
            }
        }

    	[event_mgr->lock unlock];

	switch (event->type) {
	    case EVENT_ACCEPT:
		ret = ssock_on_accept_complete(event->ssock,
		    PJ_INVALID_SOCKET,
		    event->body.accept_ev.newconn,
	    	    &event->body.accept_ev.src_addr,
	    	    event->body.accept_ev.src_addr_len,
	    	    event->body.accept_ev.status);
	    	break;
	    case EVENT_CONNECT:
	    	ret = ssock_on_connect_complete(event->ssock,
	    	    event->body.connect_ev.status);
	    	break;
	    case EVENT_VERIFY_CERT:
	    	verify_cert(assock, event->ssock->cert);
	    	break;
	    case EVENT_HANDSHAKE_COMPLETE:
	        event->ssock->ssl_state = SSL_STATE_ESTABLISHED;
	    	ret = on_handshake_complete(event->ssock,
	    	    event->body.handshake_ev.status);
	    	break;
	    case EVENT_DATA_SENT:
	    	ret = ssock_on_data_sent(event->ssock,
	    	    event->body.data_sent_ev.send_key,
	    	    event->body.data_sent_ev.sent);
	    	break;
	    case EVENT_DATA_READ:
	    	ret = ssock_on_data_read(event->ssock,
	    	    event->body.data_read_ev.data,
	    	    event->body.data_read_ev.size,
	    	    event->body.data_read_ev.status,
	    	    &event->body.data_read_ev.remainder);
	    	break;
	    default:
	    	break;
	}

	/* If not async and not destroyed, signal the waiting socket */
	if (event->type != EVENT_DISCARD && ret && !event->async && ret) {
	    dispatch_semaphore_signal(assock->ev_semaphore);
	}
	
    	/* Put the event into the free list to be reused */
    	[event_mgr->lock lock];
        if (add_ref) {
            pj_grp_lock_dec_ref(ssock->param.grp_lock);
        }
    	pj_list_push_back(&event_mgr->free_event_list, event);
    	[event_mgr->lock unlock];	
    }

    return 0;
}

/*
 *******************************************************************
 * Static/internal functions.
 *******************************************************************
 */
 
#define PJ_SSL_ERRNO_START		(PJ_ERRNO_START_USER + \
					 PJ_ERRNO_SPACE_SIZE*6)

#define PJ_SSL_ERRNO_SPACE_SIZE		PJ_ERRNO_SPACE_SIZE

/* Convert from Apple SSL error to pj_status_t. */
static pj_status_t pj_status_from_err(applessl_sock_t *assock,
				      const char *msg,
				      OSStatus err)
{
    pj_status_t status = (pj_status_t)-err;
    CFStringRef errmsg;
    
    errmsg = SecCopyErrorMessageString(err, NULL);
    PJ_LOG(3, (THIS_FILE, "Apple SSL error %s [%d]: %s",
    	       (msg? msg: ""), err,
    	       CFStringGetCStringPtr(errmsg, kCFStringEncodingUTF8)));
    CFRelease(errmsg);

    if (status > PJ_SSL_ERRNO_SPACE_SIZE)
	status = PJ_SSL_ERRNO_SPACE_SIZE;
    status += PJ_SSL_ERRNO_START;

    if (assock)
        assock->base.last_err = err;

    return status;
}

/* Read cert or key file */
static pj_status_t create_data_from_file(CFDataRef *data,
					 pj_str_t *fname, pj_str_t *path)
{
    CFURLRef file;
    CFReadStreamRef read_stream;
    UInt8 data_buf[8192];
    CFIndex nbytes = 0;
    
    if (path) {
        CFURLRef filepath;
        CFStringRef path_str;
        
        path_str = CFStringCreateWithBytes(NULL, (const UInt8 *)path->ptr,
        				   path->slen,
        				   kCFStringEncodingUTF8, false);
        if (!path_str) return PJ_ENOMEM;
    
        filepath = CFURLCreateWithFileSystemPath(NULL, path_str,
        					 kCFURLPOSIXPathStyle, true);
        CFRelease(path_str);
        if (!filepath) return PJ_ENOMEM;
    
        path_str = CFStringCreateWithBytes(NULL, (const UInt8 *)fname->ptr,
        				   fname->slen,
    		   			   kCFStringEncodingUTF8, false);
        if (!path_str) {
    	    CFRelease(filepath);
    	    return PJ_ENOMEM;
        }
    
        file = CFURLCreateCopyAppendingPathComponent(NULL, filepath,
        					     path_str, false);
        CFRelease(path_str);
        CFRelease(filepath);
    } else {
        file = CFURLCreateFromFileSystemRepresentation(NULL,
    	       (const UInt8 *)fname->ptr, fname->slen, false);
    }
    
    if (!file)
        return PJ_ENOMEM;
    
    read_stream = CFReadStreamCreateWithFile(NULL, file);
    CFRelease(file);
    
    if (!read_stream)
        return PJ_ENOTFOUND;
    
    if (!CFReadStreamOpen(read_stream)) {
        PJ_LOG(2, (THIS_FILE, "Failed opening file"));
        CFRelease(read_stream);
        return PJ_EINVAL;
    }
    
    nbytes = CFReadStreamRead(read_stream, data_buf,
    			      sizeof(data_buf));
    if (nbytes > 0)
        *data = CFDataCreate(NULL, data_buf, nbytes);
    else
    	*data = NULL;
    
    CFReadStreamClose(read_stream);
    CFRelease(read_stream);
    
    return (*data? PJ_SUCCESS: PJ_EINVAL);
}

static pj_status_t create_identity_from_cert(applessl_sock_t *assock,
			    		     pj_ssl_cert_t *cert,
			    		     sec_identity_t *p_identity)
{
    CFStringRef password = NULL;
    CFDataRef cert_data = NULL;
    void *keys[1] = {NULL};
    void *values[1] = {NULL};
    CFDictionaryRef options;
    CFArrayRef items;
    CFIndex i, count;
    SecIdentityRef identity = NULL;
    OSStatus err;
    pj_status_t status;

    /* Init */
    *p_identity = NULL;

    if (cert->privkey_file.slen || cert->privkey_buf.slen ||
    	cert->privkey_pass.slen)
    {
    	PJ_LOG(5, (THIS_FILE, "Ignoring supplied private key. Private key "
    			      "must be placed in the keychain instead."));
    }

    if (cert->cert_file.slen) {
    	status = create_data_from_file(&cert_data, &cert->cert_file, NULL);
    	if (status != PJ_SUCCESS) {
    	    PJ_PERROR(2, (THIS_FILE, status, "Failed reading cert file"));
    	    return status;
    	}
    } else if (cert->cert_buf.slen) {
    	cert_data = CFDataCreate(NULL, (const UInt8 *)cert->cert_buf.ptr,
    				 cert->cert_buf.slen);
    	if (!cert_data)
    	    return PJ_ENOMEM;
    }

    if (cert_data) {
        if (cert->privkey_pass.slen) {
	    password = CFStringCreateWithBytes(NULL,
		           (const UInt8 *)cert->privkey_pass.ptr,
		           cert->privkey_pass.slen,
		           kCFStringEncodingUTF8,
		           false);
	    keys[0] = (void *)kSecImportExportPassphrase;
	    values[0] = (void *)password;
        }
    
        options = CFDictionaryCreate(NULL, (const void **)keys,
    				     (const void **)values,
    				     (password? 1: 0), NULL, NULL);
        if (!options)
    	    return PJ_ENOMEM;
        
#if TARGET_OS_IPHONE
        err = SecPKCS12Import(cert_data, options, &items);
#else
        {
    	    SecExternalFormat ext_format[3] = {kSecFormatPKCS12,
    	    				       kSecFormatPEMSequence,
    	    				       kSecFormatX509Cert/* DER */};
    	    SecExternalItemType ext_type = kSecItemTypeCertificate;
    	    SecItemImportExportKeyParameters key_params;
    
    	    pj_bzero(&key_params, sizeof(key_params));
    	    key_params.version = SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION;
    	    key_params.passphrase = password;
    
    	    for (i = 0; i < PJ_ARRAY_SIZE(ext_format); i++) {
    	    	items = NULL;
    		err = SecItemImport(cert_data, NULL, &ext_format[i],
    				    &ext_type, 0, &key_params, NULL, &items);
    		if (err == noErr && items) {
    		    break;
    		}
    	    }
        }
#endif
    
        CFRelease(options);
        if (password)
    	    CFRelease(password);
        CFRelease(cert_data);
        if (err != noErr || !items) {
    	    return pj_status_from_err(assock, "SecItemImport", err);
        }
    
        count = CFArrayGetCount(items);
    
        for (i = 0; i < count; i++) {
    	    CFTypeRef item;
    	    CFTypeID item_id;
    	
    	    item = (CFTypeRef) CFArrayGetValueAtIndex(items, i);
    	    item_id = CFGetTypeID(item);
    
    	    if (item_id == CFDictionaryGetTypeID()) {
    	    	identity = (SecIdentityRef)
    		           CFDictionaryGetValue((CFDictionaryRef) item,
    					        kSecImportItemIdentity);
    	        break;
    	    }
#if !TARGET_OS_IPHONE
    	    else if (item_id == SecCertificateGetTypeID()) {
    	    	err = SecIdentityCreateWithCertificate(NULL,
    		          (SecCertificateRef) item, &identity);
    	    	if (err != noErr) {
    		    pj_status_from_err(assock, "SecIdentityCreate", err);
    	    	    if (err == errSecItemNotFound) {
    	    	    	PJ_LOG(2, (THIS_FILE, "Private key must be placed in "
    	    	    			      "the keychain"));
    	    	    }
    	    	} else {
    	    	    break;
    	    	}
    	    }
#endif
        }
    
        CFRelease(items);
    
        if (!identity) {
    	    PJ_LOG(2, (THIS_FILE, "Failed extracting identity from "
    			      	  "the cert file"));
    	    return PJ_EINVAL;
        }

	*p_identity = sec_identity_create(identity);
    
        CFRelease(identity);
    }

    return PJ_SUCCESS;
}

static pj_status_t verify_cert(applessl_sock_t *assock, pj_ssl_cert_t *cert)
{
    CFDataRef ca_data = NULL;
    SecTrustRef trust = assock->trust;
    bool result;
    CFErrorRef error;
    pj_status_t status = PJ_SUCCESS;
    OSStatus err = noErr;
    
    if (trust && cert && cert->CA_file.slen) {
    	status = create_data_from_file(&ca_data, &cert->CA_file,
    				       (cert->CA_path.slen? &cert->CA_path:
    				        NULL));
    	if (status != PJ_SUCCESS)
    	    PJ_LOG(2, (THIS_FILE, "Failed reading CA file"));
    } else if (trust && cert && cert->CA_buf.slen) {
    	ca_data = CFDataCreate(NULL, (const UInt8 *)cert->CA_buf.ptr,
    			       cert->CA_buf.slen);
    	if (!ca_data)
    	    PJ_LOG(2, (THIS_FILE, "Not enough memory for CA buffer"));
    }
    
    if (ca_data) {
        SecCertificateRef ca_cert;
        CFMutableArrayRef ca_array;

    	ca_cert = SecCertificateCreateWithData(NULL, ca_data);
    	CFRelease(ca_data);	
    	if (!ca_cert) {
    	    PJ_LOG(2, (THIS_FILE, "Failed creating certificate from "
    	    		      	  "CA file/buffer. It has to be "
    	    		      	  "in DER format."));
    	    status = PJ_EINVAL;
    	    goto on_return;
    	}
    
    	ca_array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
   	if (!ca_array) {
    	    PJ_LOG(2, (THIS_FILE, "Not enough memory for CA array"));
    	    CFRelease(ca_cert);
    	    status = PJ_ENOMEM;
    	    goto on_return;
    	}

    	CFArrayAppendValue(ca_array, ca_cert);
    	CFRelease(ca_cert);
    	
    	err = SecTrustSetAnchorCertificates(trust, ca_array);
    	CFRelease(ca_array);
    	if (err != noErr)
  	    pj_status_from_err(assock, "SetAnchorCerts", err);

    	err = SecTrustSetAnchorCertificatesOnly(trust, true);
    	if (err != noErr)
  	    pj_status_from_err(assock, "SetAnchorCertsOnly", err);
    }

    result = SecTrustEvaluateWithError(trust, &error);
    if (!result) {
        pj_ssl_sock_t *ssock = &assock->base;
	SecTrustResultType trust_result;
        
        err = SecTrustGetTrustResult(trust, &trust_result);
        if (err == noErr) {
    	    switch (trust_result) {
    		case kSecTrustResultInvalid:
		    ssock->verify_status |= PJ_SSL_CERT_EINVALID_FORMAT;
		    break;

    		case kSecTrustResultDeny:
    		case kSecTrustResultFatalTrustFailure:
    		    ssock->verify_status |= PJ_SSL_CERT_EUNTRUSTED;
    		    break;

    		case kSecTrustResultRecoverableTrustFailure:
    		    /* Doc: "If you receive this result, you can retry
    		     * after changing settings. For example, if trust is
    		     * denied because the certificate has expired, ..."
    		     * But this error can also mean another (recoverable)
    		     * failure, though.
    		     */
    		    ssock->verify_status |= PJ_SSL_CERT_EVALIDITY_PERIOD;
    		    break;
    	
    		case kSecTrustResultOtherError:
		    ssock->verify_status |= PJ_SSL_CERT_EUNKNOWN;
		    break;

		default:
		    break;
	    }
	}

        if (error) 
            CFRelease(error);

        /* Evaluation failed */
        status = PJ_EEOF;
    }

on_return:
    if (status != PJ_SUCCESS && assock->base.verify_status == 0)
    	assock->base.verify_status |= PJ_SSL_CERT_EUNKNOWN;

    return status;
}


/*
 *******************************************************************
 * Network functions.
 *******************************************************************
 */

/* Send data. */
static pj_status_t network_send(pj_ssl_sock_t *ssock,
				pj_ioqueue_op_key_t *send_key,
				const void *data,
				pj_ssize_t *size,
				unsigned flags)
{
    applessl_sock_t *assock = (applessl_sock_t *)ssock;
    dispatch_data_t content;

    if (!assock->connection)
    	return PJ_EGONE;
    
    content = dispatch_data_create(data, *size, assock->queue,
    				   DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    if (!content)
    	return PJ_ENOMEM;

    nw_connection_send(assock->connection, content,
    		       NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT, true,
    		       ^(nw_error_t error)
    {
    	event_t event;

	if (error != NULL) {
    	    errno = nw_error_get_error_code(error);
    	    if (errno == 89) {
    	    	/* Error 89 is network cancelled, not a send error. */
    	    	return;
    	    } else {
    	    	warn("Send error");
    	    }
        }

	event.type = EVENT_DATA_SENT;
	event.body.data_sent_ev.send_key = send_key;
	if (error != NULL) {
	    event.body.data_sent_ev.sent = (errno > 0)? -errno: errno;
	} else {
	    event.body.data_sent_ev.sent = dispatch_data_get_size(content);
	}

	event_manager_post_event(ssock, &event, PJ_TRUE);
    });
    dispatch_release(content);
    
    return PJ_EPENDING;
}

static pj_status_t network_start_read(pj_ssl_sock_t *ssock,
				      unsigned async_count,
				      unsigned buff_size,
				      void *readbuf[],
				      pj_uint32_t flags)
{
    applessl_sock_t *assock = (applessl_sock_t *)ssock;
    unsigned i;

    if (!assock->connection)
    	return PJ_EGONE;

    for (i = 0; i < async_count; i++) {
	nw_connection_receive(assock->connection, 1, buff_size,
	    ^(dispatch_data_t content, nw_content_context_t context,
	      bool is_complete, nw_error_t error)
	{
	    pj_status_t status = PJ_SUCCESS;
	    
	    /* If the context is marked as complete, and is the final context,
	     * we're read-closed.
	     */
	    if (is_complete &&
	        (context == NULL || nw_content_context_get_is_final(context)))
	    {
		return;
	    }

	    if (error != NULL) {
    	    	errno = nw_error_get_error_code(error);
    	    	if (errno == 89) {
    	    	    /* Since error 89 is network intentionally cancelled by
    	    	     * us, we immediately return.
    	    	     */
    	    	    return;
    	    	} else {
    	    	    warn("Read error, stopping further receives");
    	    	    status = PJ_EEOF;
    	    	}
            }

	    dispatch_block_t schedule_next_receive = 
	    ^{
	    	/* If there was no error in receiving, request more data. */
	    	if (!error && !is_complete && assock->connection) {
		    network_start_read(ssock, async_count, buff_size,
				       readbuf, flags);
	    	}
	    };

            if (content) {
            	dispatch_data_apply(content, 
                    ^(dispatch_data_t region, size_t offset,
                      const void *buffer, size_t inSize)
            	{
            	    /* This block can be invoked multiple times,
            	     * each for every contiguous memory region in the content.
            	     */
            	    event_t event;

		    memcpy(ssock->asock_rbuf[i], buffer, inSize);

		    event.type = EVENT_DATA_READ;
		    event.body.data_read_ev.data = ssock->asock_rbuf[i];
		    event.body.data_read_ev.size = inSize;
		    event.body.data_read_ev.status = status;
		    event.body.data_read_ev.remainder = 0;

		    event_manager_post_event(ssock, &event, PJ_FALSE);

                    return (bool)true;
                });
                
		schedule_next_receive();

            } else {
            	if (status != PJ_SUCCESS) {
            	    event_t event;

            	    /* Report read error to application */
		    event.type = EVENT_DATA_READ;
		    event.body.data_read_ev.data = NULL;
		    event.body.data_read_ev.size = 0;
		    event.body.data_read_ev.status = status;
		    event.body.data_read_ev.remainder = 0;

		    event_manager_post_event(ssock, &event, PJ_TRUE);
            	}
            
	    	schedule_next_receive();
	    }
        });
    }
	
    return PJ_SUCCESS;
}

/* Get address of local endpoint */
static pj_status_t network_get_localaddr(pj_ssl_sock_t *ssock,
					 pj_sockaddr_t *addr,
					 int *namelen)
{
    applessl_sock_t *assock = (applessl_sock_t *)ssock;
    nw_path_t path;
    nw_endpoint_t endpoint;
    const struct sockaddr *address;
    
    path = nw_connection_copy_current_path(assock->connection);
    if (!path)
    	return PJ_EINVALIDOP;
    
    endpoint = nw_path_copy_effective_local_endpoint(path);
    nw_release(path);
    if (!endpoint)
    	return PJ_EINVALIDOP;

    address = nw_endpoint_get_address(endpoint);
    if (address) {
    	pj_sockaddr_cp(addr, address);
    	*namelen = pj_sockaddr_get_addr_len(addr);
    }
    nw_release(endpoint);
    
    return PJ_SUCCESS;
}

static pj_status_t network_create_params(pj_ssl_sock_t * ssock,
				     	 const pj_sockaddr_t *localaddr,
				     	 pj_uint16_t port_range,
				     	 nw_parameters_t *p_params)
{
    applessl_sock_t *assock = (applessl_sock_t *)ssock;
    char ip_addr[PJ_INET6_ADDRSTRLEN];
    unsigned port;
    char port_str[PJ_INET6_ADDRSTRLEN];
    nw_endpoint_t local_endpoint;
    nw_parameters_t parameters;
    nw_parameters_configure_protocol_block_t configure_tls;
    nw_protocol_stack_t protocol_stack;
    nw_protocol_options_t ip_options;
    tls_protocol_version_t min_proto = tls_protocol_version_TLSv10;
    tls_protocol_version_t max_proto = tls_protocol_version_TLSv13;

    /* Set min and max protocol version */
    if (ssock->param.proto == PJ_SSL_SOCK_PROTO_DEFAULT) {
	ssock->param.proto = PJ_SSL_SOCK_PROTO_TLS1 |
	    		     PJ_SSL_SOCK_PROTO_TLS1_1 |
	    		     PJ_SSL_SOCK_PROTO_TLS1_2 |
	    		     PJ_SSL_SOCK_PROTO_TLS1_3;
    }

    if (ssock->param.proto & PJ_SSL_SOCK_PROTO_TLS1_3) {
	max_proto = tls_protocol_version_TLSv13;
    } else if (ssock->param.proto & PJ_SSL_SOCK_PROTO_TLS1_2) {
	max_proto = tls_protocol_version_TLSv12;
    } else if (ssock->param.proto & PJ_SSL_SOCK_PROTO_TLS1_1) {
	max_proto = tls_protocol_version_TLSv11;
    } else if (ssock->param.proto & PJ_SSL_SOCK_PROTO_TLS1) {
	max_proto = tls_protocol_version_TLSv10;
    } else {
	PJ_LOG(3, (THIS_FILE, "Unsupported TLS protocol"));
	return PJ_EINVAL;
    }

    if (ssock->param.proto & PJ_SSL_SOCK_PROTO_TLS1) {
	min_proto = tls_protocol_version_TLSv10;
    } else if (ssock->param.proto & PJ_SSL_SOCK_PROTO_TLS1_1) {
	min_proto = tls_protocol_version_TLSv11;
    } else if (ssock->param.proto & PJ_SSL_SOCK_PROTO_TLS1_2) {
	min_proto = tls_protocol_version_TLSv12;
    } else if (ssock->param.proto & PJ_SSL_SOCK_PROTO_TLS1_3) {
	min_proto = tls_protocol_version_TLSv13;
    }

    /* Set certificate */
    if (ssock->cert)  {
	pj_status_t status = create_identity_from_cert(assock, ssock->cert,
						       &assock->identity);
	if (status != PJ_SUCCESS)
    	    return status;
    }

    configure_tls = ^(nw_protocol_options_t tls_options)
    {
	sec_protocol_options_t sec_options;

	sec_options = nw_tls_copy_sec_protocol_options(tls_options);

    	/* Set identity */
    	if (ssock->cert && assock->identity) {
	    sec_protocol_options_set_local_identity(sec_options,
	    					    assock->identity);
	}

	sec_protocol_options_set_min_tls_protocol_version(sec_options,
	    						  min_proto);
	sec_protocol_options_set_max_tls_protocol_version(sec_options,
							  max_proto);

    	/* Set cipher list */
    	if (ssock->param.ciphers_num > 0) {
    	    unsigned i;    	
    	    for (i = 0; i < ssock->param.ciphers_num; i++) {
    	    	sec_protocol_options_append_tls_ciphersuite(sec_options,
	    	    (tls_ciphersuite_t)ssock->param.ciphers[i]);
	    }
	}
    
	if (!ssock->is_server && ssock->param.server_name.slen) {
	    sec_protocol_options_set_tls_server_name(sec_options,
	    	ssock->param.server_name.ptr);
	}
	
	sec_protocol_options_set_tls_renegotiation_enabled(sec_options,
							   true);
	/* This must be disabled, otherwise server may think this is
	 * a resumption of a previously closed connection, and our
	 * verify block may never be invoked!
	 */
    	sec_protocol_options_set_tls_resumption_enabled(sec_options, false);
	
    	/* SSL verification options */
    	sec_protocol_options_set_peer_authentication_required(sec_options,
    	    true);

	/* Handshake flow:
	 * 1. Server's challenge block, provide server's trust
	 * 2. Client's verify block, to verify server's trust
	 * 3. Client's challenge block, provide client's trust
	 * 4. Only if client's trust is not NULL, server's verify block,
	 *    to verify client's trust.
	 */
        sec_protocol_options_set_challenge_block(sec_options,
	    ^(sec_protocol_metadata_t metadata,
	      sec_protocol_challenge_complete_t complete)
	{
	    complete(assock->identity);
	}, assock->queue);
	
	sec_protocol_options_set_verify_block(sec_options,
	    ^(sec_protocol_metadata_t metadata, sec_trust_t trust_ref,
	      sec_protocol_verify_complete_t complete)
	{
	    event_t event;
	    pj_status_t status;
	    bool result = true;

    	    assock->trust = trust_ref? sec_trust_copy_ref(trust_ref): nil;

	    assock->cipher =
	      sec_protocol_metadata_get_negotiated_tls_ciphersuite(metadata);

	    /* For client, call on_connect_complete() callback first. */
	    if (!ssock->is_server) {
	    	if (!assock->connection)
	    	    complete(false);

	    	event.type = EVENT_CONNECT;
	    	event.body.connect_ev.status = PJ_SUCCESS;
	    	status = event_manager_post_event(ssock, &event, PJ_FALSE);
	    	if (status == PJ_EGONE)
	    	    complete(false);
	    }

	    event.type = EVENT_VERIFY_CERT;
	    status = event_manager_post_event(ssock, &event, PJ_FALSE);
	    if (status == PJ_EGONE)
	    	complete(false);

	    /* Check the result of cert verification. */
	    if (ssock->verify_status != PJ_SSL_CERT_ESUCCESS) {
		if (ssock->param.verify_peer) {
        	    /* Verification failed. */
        	    result = false;
    		} else {
    		    /* When verification is not requested just return ok here,
    		     * however application can still get the verification status.
     		     */
    	    	    result = true;
    	    	}
    	    }

	    complete(result);
        }, assock->queue);

	nw_release(sec_options);
    };

    parameters = nw_parameters_create_secure_tcp(configure_tls,
    		     NW_PARAMETERS_DEFAULT_CONFIGURATION);

    protocol_stack = nw_parameters_copy_default_protocol_stack(parameters);
    ip_options = nw_protocol_stack_copy_internet_protocol(protocol_stack);
    if (ssock->param.sock_af == pj_AF_INET()) {
	nw_ip_options_set_version(ip_options, nw_ip_version_4);
    } else if (ssock->param.sock_af == pj_AF_INET6()) {
	nw_ip_options_set_version(ip_options, nw_ip_version_6);
    }
    nw_release(ip_options);
    nw_release(protocol_stack);
    
    if (ssock->is_server && ssock->param.reuse_addr) {
    	nw_parameters_set_reuse_local_address(parameters, true);
    }

    /* Create local endpoint.
     * Currently we ignore QoS and socket options.
     */
    pj_sockaddr_print(localaddr, ip_addr,sizeof(ip_addr),0);

    if (port_range) {
	pj_uint16_t max_try = MAX_BIND_RETRY;

	if (port_range && port_range < max_try) {
	    max_try = port_range;
	}
	for (; max_try; --max_try) {
	    pj_uint16_t base_port;

	    base_port = pj_sockaddr_get_port(localaddr);
	    port = (pj_uint16_t)(base_port + pj_rand() % (port_range + 1));
	    pj_utoa(port, port_str);
	    
	    local_endpoint = nw_endpoint_create_host(ip_addr, port_str);
	    if (local_endpoint)
	    	break;
	}
    } else {
    	port = pj_sockaddr_get_port(localaddr);
    	pj_utoa(port, port_str);

    	local_endpoint = nw_endpoint_create_host(ip_addr, port_str);
    }
    
    if (!local_endpoint) {
        PJ_LOG(2, (THIS_FILE, "Failed creating local endpoint"));
    	return PJ_EINVALIDOP;
    }

    nw_parameters_set_local_endpoint(parameters, local_endpoint);
    nw_release(local_endpoint);
    
    *p_params = parameters;
    return PJ_SUCCESS;
}

/* Setup assock's connection state callback and start the connection */
static pj_status_t network_setup_connection(pj_ssl_sock_t *ssock,
					    void *connection)
{
    applessl_sock_t *assock = (applessl_sock_t *)ssock;
    assock->connection = (nw_connection_t)connection;
    pj_status_t status;

    /* Initialize input circular buffer */
    status = circ_init(ssock->pool->factory, &ssock->circ_buf_input, 8192);
    if (status != PJ_SUCCESS)
        return status;

    /* Initialize output circular buffer */
    status = circ_init(ssock->pool->factory, &ssock->circ_buf_output, 8192);
    if (status != PJ_SUCCESS)
        return status;

    nw_connection_set_queue(assock->connection, assock->queue);

    assock->con_state = nw_connection_state_invalid;
    nw_connection_set_state_changed_handler(assock->connection,
    	^(nw_connection_state_t state, nw_error_t error)
    {
    	pj_status_t status = PJ_SUCCESS;
    	pj_bool_t call_cb = PJ_FALSE;
#if SSL_DEBUG
    	if (!pj_thread_is_registered()) {
    	    pj_bzero(queue_th_desc, sizeof(pj_thread_desc));
    	    pj_thread_register("sslq", queue_th_desc, &queue_th);
        }
        PJ_LOG(3, (THIS_FILE, "SSL state change %p %d", assock, state));
#endif

	if (error && state != nw_connection_state_cancelled) {
    	    errno = nw_error_get_error_code(error);
            warn("Connection failed %p", assock);
            status = PJ_STATUS_FROM_OS(errno);
#if SSL_DEBUG
            PJ_LOG(3, (THIS_FILE, "SSL state and errno %d %d", state, errno));
#endif
            call_cb = PJ_TRUE;	
	}

	if (state == nw_connection_state_ready) {
	    if (ssock->is_server) {
    		nw_protocol_definition_t tls_def;
    		nw_protocol_metadata_t prot_meta;
    		sec_protocol_metadata_t meta;
    		
    		tls_def = nw_protocol_copy_tls_definition();
    		prot_meta = nw_connection_copy_protocol_metadata(connection,
    								 tls_def);
		meta = nw_tls_copy_sec_protocol_metadata(prot_meta);
		assock->cipher =
	      	    sec_protocol_metadata_get_negotiated_tls_ciphersuite(meta);

		if (ssock->param.require_client_cert &&
		    !sec_protocol_metadata_access_peer_certificate_chain(
		    	meta, ^(sec_certificate_t certificate) {} ))
		{
		    status = PJ_EEOF;
		}
		nw_release(tls_def);
		nw_release(prot_meta);
		nw_release(meta);
	    }
	    call_cb = PJ_TRUE;
	} else if (state == nw_connection_state_cancelled) {
	    /* We release the reference in ssl_destroy() */
	    // nw_release(assock->connection);
	    // assock->connection = nil;
	}

	if (call_cb) {
	    event_t event;

	    event.type = EVENT_HANDSHAKE_COMPLETE;
	    event.body.handshake_ev.status = status;
	    event_manager_post_event(ssock, &event, PJ_TRUE);

	    if (ssock->is_server && status == PJ_SUCCESS) {
    	        status = network_start_read(ssock, ssock->param.async_cnt,
    		             (unsigned)ssock->param.read_buffer_size,
			     ssock->asock_rbuf, 0);
	    }
	}

	assock->con_state = state;
    });

    nw_connection_start(assock->connection);
    
    return PJ_SUCCESS;
}

static pj_status_t network_start_accept(pj_ssl_sock_t *ssock,
			 		pj_pool_t *pool,
			  		const pj_sockaddr_t *localaddr,
			  		int addr_len,
			  		const pj_ssl_sock_param *newsock_param)
{
    applessl_sock_t *assock = (applessl_sock_t *)ssock;
    pj_status_t status;
    nw_parameters_t parameters = NULL;

    status = network_create_params(ssock, localaddr, 0, &parameters);
    if (status != PJ_SUCCESS)
    	return status;

    /* Create listener */
    assock->listener = nw_listener_create(parameters);
    nw_release(parameters);
    if (!assock->listener) {
        PJ_LOG(2, (THIS_FILE, "Failed creating listener"));
    	return PJ_EINVALIDOP;
    }

    nw_listener_set_queue(assock->listener, assock->queue);
    /* Hold a reference until cancelled */
    nw_retain(assock->listener);

    assock->lis_state = nw_listener_state_invalid;
    nw_listener_set_state_changed_handler(assock->listener, 
        ^(nw_listener_state_t state, nw_error_t error)
    {
	errno = error ? nw_error_get_error_code(error) : 0;

	if (state == nw_listener_state_failed) {
	    warn("listener failed\n");
	    pj_sockaddr_set_port(&ssock->local_addr, 0);
	    dispatch_semaphore_signal(assock->ev_semaphore);
	} else if (state == nw_listener_state_ready) {
    	    /* Update local port */
    	    pj_sockaddr_set_port(&ssock->local_addr,
    			         nw_listener_get_port(assock->listener));
	    dispatch_semaphore_signal(assock->ev_semaphore);
	} else if (state == nw_listener_state_cancelled) {
	    /* We release the reference in ssl_destroy() */
	    // nw_release(assock->listener);
	    // assock->listener = nil;
	}
    	assock->lis_state = state;
    });

    nw_listener_set_new_connection_handler(assock->listener,
    	^(nw_connection_t connection)
    {
	nw_endpoint_t endpoint = nw_connection_copy_endpoint(connection);
	const struct sockaddr *address;
	event_t event;
	
	address = nw_endpoint_get_address(endpoint);

	event.type = EVENT_ACCEPT;
	event.body.accept_ev.newconn = connection;
	pj_sockaddr_cp(&event.body.accept_ev.src_addr, address);
	event.body.accept_ev.src_addr_len = pj_sockaddr_get_addr_len(address);
	event.body.accept_ev.status = PJ_SUCCESS;
	
	nw_retain(connection);
	event_manager_post_event(ssock, &event, PJ_TRUE);
	
	nw_release(endpoint);
    });

    /* Update local address */
    ssock->addr_len = addr_len;
    pj_sockaddr_cp(&ssock->local_addr, localaddr);

    /* Start accepting */
    pj_ssl_sock_param_copy(pool, &ssock->newsock_param, newsock_param);
    ssock->newsock_param.grp_lock = NULL;

    /* Start listening to the address */
    nw_listener_start(assock->listener);
    /* Wait until it's ready */
    dispatch_semaphore_wait(assock->ev_semaphore, DISPATCH_TIME_FOREVER);
    
    if (pj_sockaddr_get_port(&ssock->local_addr) == 0) {
    	/* Failed. */
    	status = PJ_EEOF;
    	goto on_error;
    }

    return PJ_SUCCESS;

on_error:
    ssl_reset_sock_state(ssock);
    return status;
}


static pj_status_t network_start_connect(pj_ssl_sock_t *ssock,
		       pj_ssl_start_connect_param *connect_param)
{
    char ip_addr[PJ_INET6_ADDRSTRLEN];
    unsigned port;
    char port_str[PJ_INET6_ADDRSTRLEN];
    nw_endpoint_t endpoint;
    nw_parameters_t parameters;
    nw_connection_t connection;
    pj_status_t status;

    pj_pool_t *pool = connect_param->pool;
    const pj_sockaddr_t *localaddr = connect_param->localaddr;
    pj_uint16_t port_range = connect_param->local_port_range;
    const pj_sockaddr_t *remaddr = connect_param->remaddr;
    int addr_len = connect_param->addr_len;

    PJ_ASSERT_RETURN(ssock && pool && localaddr && remaddr && addr_len,
		     PJ_EINVAL);

    status = network_create_params(ssock, localaddr, port_range,
    				   &parameters);
    if (status != PJ_SUCCESS)
    	return status;

    /* Create remote endpoint */
    pj_sockaddr_print(remaddr, ip_addr,sizeof(ip_addr),0);
    port = pj_sockaddr_get_port(remaddr);
    pj_utoa(port, port_str);

    endpoint = nw_endpoint_create_host(ip_addr, port_str);
    if (!endpoint) {
        PJ_LOG(2, (THIS_FILE, "Failed creating remote endpoint"));
    	nw_release(parameters);
    	return PJ_EINVALIDOP;
    }

    connection = nw_connection_create(endpoint, parameters);
    nw_release(endpoint);
    nw_release(parameters);
    if (!connection) {
        PJ_LOG(2, (THIS_FILE, "Failed creating connection"));
    	return PJ_EINVALIDOP;
    }

    /* Hold a reference until cancelled */
    nw_retain(connection);

    status = network_setup_connection(ssock, connection);
    if (status != PJ_SUCCESS)
    	return status;
    
    /* Save remote address */
    pj_sockaddr_cp(&ssock->rem_addr, remaddr);

    /* Update local address */
    ssock->addr_len = addr_len;
    pj_sockaddr_cp(&ssock->local_addr, localaddr);
    
    return PJ_EPENDING;
}


/*
 *******************************************************************
 * SSL functions.
 *******************************************************************
 */

static pj_ssl_sock_t *ssl_alloc(pj_pool_t *pool)
{
    applessl_sock_t *assock;
    
    /* Create event manager */
    if (event_manager_create() != PJ_SUCCESS)
    	return NULL;
    
    assock = PJ_POOL_ZALLOC_T(pool, applessl_sock_t);    

    assock->queue = dispatch_queue_create("ssl_queue", DISPATCH_QUEUE_SERIAL);
    assock->ev_semaphore = dispatch_semaphore_create(0);    
    if (!assock->queue || !assock->ev_semaphore) {
    	ssl_destroy(&assock->base);
    	return NULL;
    }
        
    return (pj_ssl_sock_t *)assock;
}

static pj_status_t ssl_create(pj_ssl_sock_t *ssock)
{
    /* Nothing to do here. SSL has been configured before connection
     * is started.
     */
    return PJ_SUCCESS;
}

static void close_connection(applessl_sock_t *assock)
{
    if (assock->connection) {
    	unsigned i;
	nw_connection_t conn = assock->connection;
	
	assock->connection = nil;
    	nw_connection_force_cancel(conn);
    	nw_release(conn);

    	/* We need to wait until the connection is at cancelled state,
    	 * otherwise events will still be delivered even though we
    	 * already force cancel and release the connection.
    	 */
    	for (i = 0; i < 40; i++) {
    	    if (assock->con_state == nw_connection_state_cancelled) break;
    	    pj_thread_sleep(50);
    	}

    	event_manager_remove_events(&assock->base);

    	if (assock->con_state != nw_connection_state_cancelled) {
    	    PJ_LOG(3, (THIS_FILE, "Warning: Failed to cancel SSL connection "
    	    			  "%p %d", assock, assock->con_state));
    	}

#if SSL_DEBUG
	PJ_LOG(3, (THIS_FILE, "SSL connection %p closed", assock));
#endif

    }
}

/* Close sockets */
static void ssl_close_sockets(pj_ssl_sock_t *ssock)
{
    applessl_sock_t *assock = (applessl_sock_t *)ssock;

    if (assock->identity) {
   	nw_release(assock->identity);
   	assock->identity = nil;
    }

    if (assock->trust) {
	nw_release(assock->trust);
	assock->trust = nil;
    }

    /* This can happen when pj_ssl_sock_create() fails. */
    if (!ssock->write_mutex)
    	return;

    pj_lock_acquire(ssock->write_mutex);
    close_connection(assock);
    pj_lock_release(ssock->write_mutex);
}

/* Destroy Apple SSL. */
static void ssl_destroy(pj_ssl_sock_t *ssock)
{
    applessl_sock_t *assock = (applessl_sock_t *)ssock;

    close_connection(assock);

    if (assock->listener) {
    	unsigned i;
	
	nw_listener_set_new_connection_handler(assock->listener, nil);
    	nw_listener_cancel(assock->listener);

    	for (i = 0; i < 20; i++) {
    	    if (assock->lis_state == nw_listener_state_cancelled) break;
    	    pj_thread_sleep(50);
    	}
    	if (assock->lis_state != nw_listener_state_cancelled) {
    	    PJ_LOG(3, (THIS_FILE, "Warning: Failed to cancel SSL listener "
    	    			  "%p %d", assock, assock->lis_state));
    	}
	nw_release(assock->listener);
  	assock->listener = nil;
    }

    event_manager_remove_events(ssock);

    /* Important: if we are called from a blocking dispatch block,
     * we need to signal it before destroying ourselves.
     */
    if (assock->ev_semaphore) {
    	dispatch_semaphore_signal(assock->ev_semaphore);
    }

    if (assock->queue) {
    	dispatch_release(assock->queue);
    	assock->queue = NULL;
    }

    if (assock->ev_semaphore) {
    	dispatch_release(assock->ev_semaphore);
    	assock->ev_semaphore = nil;
    }

    /* Destroy circular buffers */
    circ_deinit(&ssock->circ_buf_input);
    circ_deinit(&ssock->circ_buf_output);
    
    PJ_LOG(4, (THIS_FILE, "SSL %p destroyed", ssock));
}


/* Reset socket state. */
static void ssl_reset_sock_state(pj_ssl_sock_t *ssock)
{
    pj_lock_acquire(ssock->circ_buf_output_mutex);
    ssock->ssl_state = SSL_STATE_NULL;
    pj_lock_release(ssock->circ_buf_output_mutex);

#if SSL_DEBUG
    PJ_LOG(3, (THIS_FILE, "SSL reset sock state %p", ssock));
#endif

    ssl_close_sockets(ssock);
}


/* This function is taken from Apple's sslAppUtils.cpp (version 58286.41.2),
 * with some modifications.
 */
const char *sslGetCipherSuiteString(SSLCipherSuite cs)
{
    switch (cs) {
        /* TLS addenda using AES-CBC, RFC 3268 */
        case TLS_RSA_WITH_AES_128_CBC_SHA:          
            return "TLS_RSA_WITH_AES_128_CBC_SHA";
        case TLS_DH_DSS_WITH_AES_128_CBC_SHA:       
            return "TLS_DH_DSS_WITH_AES_128_CBC_SHA";
        case TLS_DH_RSA_WITH_AES_128_CBC_SHA:       
            return "TLS_DH_RSA_WITH_AES_128_CBC_SHA";
        case TLS_DHE_DSS_WITH_AES_128_CBC_SHA:      
            return "TLS_DHE_DSS_WITH_AES_128_CBC_SHA";
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:      
            return "TLS_DHE_RSA_WITH_AES_128_CBC_SHA";
        case TLS_DH_anon_WITH_AES_128_CBC_SHA:      
            return "TLS_DH_anon_WITH_AES_128_CBC_SHA";
        case TLS_RSA_WITH_AES_256_CBC_SHA:          
            return "TLS_RSA_WITH_AES_256_CBC_SHA";
        case TLS_DH_DSS_WITH_AES_256_CBC_SHA:       
            return "TLS_DH_DSS_WITH_AES_256_CBC_SHA";
        case TLS_DH_RSA_WITH_AES_256_CBC_SHA:       
            return "TLS_DH_RSA_WITH_AES_256_CBC_SHA";
        case TLS_DHE_DSS_WITH_AES_256_CBC_SHA:      
            return "TLS_DHE_DSS_WITH_AES_256_CBC_SHA";
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:      
            return "TLS_DHE_RSA_WITH_AES_256_CBC_SHA";
        case TLS_DH_anon_WITH_AES_256_CBC_SHA:      
            return "TLS_DH_anon_WITH_AES_256_CBC_SHA";

        /* ECDSA addenda, RFC 4492 */
        case TLS_ECDH_ECDSA_WITH_NULL_SHA:          
            return "TLS_ECDH_ECDSA_WITH_NULL_SHA";
        case TLS_ECDH_ECDSA_WITH_RC4_128_SHA:       
            return "TLS_ECDH_ECDSA_WITH_RC4_128_SHA";
        case TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA:  
            return "TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA";
        case TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA:   
            return "TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA";
        case TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA:   
            return "TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA";
        case TLS_ECDHE_ECDSA_WITH_NULL_SHA:         
            return "TLS_ECDHE_ECDSA_WITH_NULL_SHA";
        case TLS_ECDHE_ECDSA_WITH_RC4_128_SHA:      
            return "TLS_ECDHE_ECDSA_WITH_RC4_128_SHA";
        case TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA: 
            return "TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA";
        case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA:  
            return "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA";
        case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA:  
            return "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA";
        case TLS_ECDH_RSA_WITH_NULL_SHA:            
            return "TLS_ECDH_RSA_WITH_NULL_SHA";
        case TLS_ECDH_RSA_WITH_RC4_128_SHA:         
            return "TLS_ECDH_RSA_WITH_RC4_128_SHA";
        case TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA:    
            return "TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA";
        case TLS_ECDH_RSA_WITH_AES_128_CBC_SHA:     
            return "TLS_ECDH_RSA_WITH_AES_128_CBC_SHA";
        case TLS_ECDH_RSA_WITH_AES_256_CBC_SHA:     
            return "TLS_ECDH_RSA_WITH_AES_256_CBC_SHA";
        case TLS_ECDHE_RSA_WITH_NULL_SHA:           
            return "TLS_ECDHE_RSA_WITH_NULL_SHA";
        case TLS_ECDHE_RSA_WITH_RC4_128_SHA:        
            return "TLS_ECDHE_RSA_WITH_RC4_128_SHA";
        case TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA:   
            return "TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA";
        case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA:    
            return "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA";
        case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA:    
            return "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA";
        case TLS_ECDH_anon_WITH_NULL_SHA:           
            return "TLS_ECDH_anon_WITH_NULL_SHA";
        case TLS_ECDH_anon_WITH_RC4_128_SHA:        
            return "TLS_ECDH_anon_WITH_RC4_128_SHA";
        case TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA:   
            return "TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA";
        case TLS_ECDH_anon_WITH_AES_128_CBC_SHA:    
            return "TLS_ECDH_anon_WITH_AES_128_CBC_SHA";
        case TLS_ECDH_anon_WITH_AES_256_CBC_SHA:    
            return "TLS_ECDH_anon_WITH_AES_256_CBC_SHA";

        /* TLS 1.2 addenda, RFC 5246 */
        case TLS_RSA_WITH_AES_128_CBC_SHA256:       
            return "TLS_RSA_WITH_AES_128_CBC_SHA256";
        case TLS_RSA_WITH_AES_256_CBC_SHA256:       
            return "TLS_RSA_WITH_AES_256_CBC_SHA256";
        case TLS_DH_DSS_WITH_AES_128_CBC_SHA256:    
            return "TLS_DH_DSS_WITH_AES_128_CBC_SHA256";
        case TLS_DH_RSA_WITH_AES_128_CBC_SHA256:    
            return "TLS_DH_RSA_WITH_AES_128_CBC_SHA256";
        case TLS_DHE_DSS_WITH_AES_128_CBC_SHA256:   
            return "TLS_DHE_DSS_WITH_AES_128_CBC_SHA256";
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA256:   
            return "TLS_DHE_RSA_WITH_AES_128_CBC_SHA256";
        case TLS_DH_DSS_WITH_AES_256_CBC_SHA256:    
            return "TLS_DH_DSS_WITH_AES_256_CBC_SHA256";
        case TLS_DH_RSA_WITH_AES_256_CBC_SHA256:    
            return "TLS_DH_RSA_WITH_AES_256_CBC_SHA256";
        case TLS_DHE_DSS_WITH_AES_256_CBC_SHA256:   
            return "TLS_DHE_DSS_WITH_AES_256_CBC_SHA256";
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA256:   
            return "TLS_DHE_RSA_WITH_AES_256_CBC_SHA256";
        case TLS_DH_anon_WITH_AES_128_CBC_SHA256:   
            return "TLS_DH_anon_WITH_AES_128_CBC_SHA256";
        case TLS_DH_anon_WITH_AES_256_CBC_SHA256:   
            return "TLS_DH_anon_WITH_AES_256_CBC_SHA256";

        /* TLS addenda using AES-GCM, RFC 5288 */
        case TLS_RSA_WITH_AES_128_GCM_SHA256:       
            return "TLS_RSA_WITH_AES_128_GCM_SHA256";
        case TLS_RSA_WITH_AES_256_GCM_SHA384:       
            return "TLS_DHE_RSA_WITH_AES_128_GCM_SHA256";
        case TLS_DHE_RSA_WITH_AES_128_GCM_SHA256:   
            return "TLS_DHE_RSA_WITH_AES_128_GCM_SHA256";
        case TLS_DHE_RSA_WITH_AES_256_GCM_SHA384:   
            return "TLS_DHE_RSA_WITH_AES_256_GCM_SHA384";
        case TLS_DH_RSA_WITH_AES_128_GCM_SHA256:    
            return "TLS_DH_RSA_WITH_AES_128_GCM_SHA256";
        case TLS_DH_RSA_WITH_AES_256_GCM_SHA384:    
            return "TLS_DH_RSA_WITH_AES_256_GCM_SHA384";
        case TLS_DHE_DSS_WITH_AES_128_GCM_SHA256:   
            return "TLS_DHE_DSS_WITH_AES_128_GCM_SHA256";
        case TLS_DHE_DSS_WITH_AES_256_GCM_SHA384:   
            return "TLS_DHE_DSS_WITH_AES_256_GCM_SHA384";
        case TLS_DH_DSS_WITH_AES_128_GCM_SHA256:    
            return "TLS_DH_DSS_WITH_AES_128_GCM_SHA256";
        case TLS_DH_DSS_WITH_AES_256_GCM_SHA384:    
            return "TLS_DH_DSS_WITH_AES_256_GCM_SHA384";
        case TLS_DH_anon_WITH_AES_128_GCM_SHA256:   
            return "TLS_DH_anon_WITH_AES_128_GCM_SHA256";
        case TLS_DH_anon_WITH_AES_256_GCM_SHA384:   
            return "TLS_DH_anon_WITH_AES_256_GCM_SHA384";

        /* ECDSA addenda, RFC 5289 */
        case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256:   
            return "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256";
        case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384:   
            return "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384";
        case TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256:    
            return "TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256";
        case TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384:    
            return "TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384";
        case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256:     
            return "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256";
        case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384:     
            return "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384";
        case TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256:      
            return "TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256";
        case TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384:      
            return "TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384";
        case TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256:   
            return "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256";
        case TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384:   
            return "TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384";
        case TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256:    
            return "TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256";
        case TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384:    
            return "TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384";
        case TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256:     
            return "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256";
        case TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384:     
            return "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384";
        case TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256:      
            return "TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256";
        case TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384:      
            return "TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384";

	case TLS_AES_128_GCM_SHA256:
	    return "TLS_AES_128_GCM_SHA256";	
	case TLS_AES_256_GCM_SHA384:
	    return "TLS_AES_256_GCM_SHA384";
	case TLS_CHACHA20_POLY1305_SHA256:
	    return "TLS_CHACHA20_POLY1305_SHA256";
	case TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256:
	    return "TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256";
	case TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256:
	    return "TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256";
	case TLS_RSA_WITH_3DES_EDE_CBC_SHA:
	    return "TLS_RSA_WITH_3DES_EDE_CBC_SHA";
            
        default:
            return "TLS_CIPHER_STRING_UNKNOWN";
    }
}

static void ssl_ciphers_populate(void)
{
    /* SSLGetSupportedCiphers() is deprecated and we can't find
     * the replacement API, so we just list the valid ciphers here
     * taken from the tls_ciphersuite_t doc.
     */
    tls_ciphersuite_t ciphers[] = {
    	tls_ciphersuite_AES_128_GCM_SHA256,
	tls_ciphersuite_AES_256_GCM_SHA384,
	tls_ciphersuite_CHACHA20_POLY1305_SHA256,
    	tls_ciphersuite_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,
	tls_ciphersuite_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
	tls_ciphersuite_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
	tls_ciphersuite_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
	tls_ciphersuite_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
	tls_ciphersuite_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
	tls_ciphersuite_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
	tls_ciphersuite_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
	tls_ciphersuite_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,
	tls_ciphersuite_ECDHE_RSA_WITH_AES_128_CBC_SHA,
	tls_ciphersuite_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
	tls_ciphersuite_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
	tls_ciphersuite_ECDHE_RSA_WITH_AES_256_CBC_SHA,
	tls_ciphersuite_ECDHE_RSA_WITH_AES_256_CBC_SHA384,
	tls_ciphersuite_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
	tls_ciphersuite_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
	tls_ciphersuite_RSA_WITH_3DES_EDE_CBC_SHA,
	tls_ciphersuite_RSA_WITH_AES_128_CBC_SHA,
	tls_ciphersuite_RSA_WITH_AES_128_CBC_SHA256,
	tls_ciphersuite_RSA_WITH_AES_128_GCM_SHA256,
	tls_ciphersuite_RSA_WITH_AES_256_CBC_SHA,
	tls_ciphersuite_RSA_WITH_AES_256_CBC_SHA256,
	tls_ciphersuite_RSA_WITH_AES_256_GCM_SHA384
    };
    if (!ssl_cipher_num) {
    	unsigned i;
    	
    	ssl_cipher_num = sizeof(ciphers)/sizeof(ciphers[0]);
	for (i = 0; i < ssl_cipher_num; i++) {
	    ssl_ciphers[i].id = (pj_ssl_cipher)ciphers[i];
	    ssl_ciphers[i].name = sslGetCipherSuiteString(ciphers[i]);
	}
    }
}


static pj_ssl_cipher ssl_get_cipher(pj_ssl_sock_t *ssock)
{
    applessl_sock_t *assock = (applessl_sock_t *)ssock;

    return (pj_ssl_cipher) assock->cipher;
}


#if !TARGET_OS_IPHONE
static void get_info_and_cn(CFArrayRef array, CFMutableStringRef info,
			    CFStringRef *cn)
{
    const void *keys[] = {kSecOIDOrganizationalUnitName, kSecOIDCountryName,
    			  kSecOIDStateProvinceName, kSecOIDLocalityName,
    			  kSecOIDOrganizationName, kSecOIDCommonName};
    const char *labels[] = { "OU=", "C=", "ST=", "L=", "O=", "CN="};
    pj_bool_t add_separator = PJ_FALSE;
    int i, n;

    *cn = NULL;
    for(i = 0; i < sizeof(keys)/sizeof(keys[0]);  i++) {
        for (n = 0 ; n < CFArrayGetCount(array); n++) {
            CFDictionaryRef dict;
            CFTypeRef dictkey;
            CFStringRef str;

            dict = CFArrayGetValueAtIndex(array, n);
            if (CFGetTypeID(dict) != CFDictionaryGetTypeID())
                continue;
            dictkey = CFDictionaryGetValue(dict, kSecPropertyKeyLabel);
            if (!CFEqual(dictkey, keys[i]))
                continue;
            str = (CFStringRef) CFDictionaryGetValue(dict,
            					     kSecPropertyKeyValue);
            					     
            if (CFStringGetLength(str) > 0) {
            	if (add_separator) {
                    CFStringAppendCString(info, "/", kCFStringEncodingUTF8);
                }
                CFStringAppendCString(info, labels[i], kCFStringEncodingUTF8);
                CFStringAppend(info, str);
		add_separator = PJ_TRUE;
		
		if (CFEqual(keys[i], kSecOIDCommonName))
		    *cn = str;
            }
        }
    }
}

static CFDictionaryRef get_cert_oid(SecCertificateRef cert, CFStringRef oid,
				    CFTypeRef *value)
{
    void *key[1];
    CFArrayRef key_arr;
    CFDictionaryRef vals, dict;

    key[0] = (void *)oid;
    key_arr = CFArrayCreate(NULL, (const void **)key, 1,
    			    &kCFTypeArrayCallBacks);

    vals = SecCertificateCopyValues(cert, key_arr, NULL);
    dict = CFDictionaryGetValue(vals, key[0]);
    if (!dict) {
    	CFRelease(key_arr);
    	CFRelease(vals);
    	return NULL;
    }

    *value = CFDictionaryGetValue(dict, kSecPropertyKeyValue);

    CFRelease(key_arr);

    return vals;
}

#endif

/* Get certificate info; in case the certificate info is already populated,
 * this function will check if the contents need updating by inspecting the
 * issuer and the serial number.
 */
static void get_cert_info(pj_pool_t *pool, pj_ssl_cert_info *ci,
			  SecCertificateRef cert)
{
    pj_bool_t update_needed;
    char buf[512];
    size_t bufsize = sizeof(buf);
    const pj_uint8_t *serial_no = NULL;
    size_t serialsize = 0;
    CFMutableStringRef issuer_info;
    CFStringRef str;
    CFDataRef serial = NULL;
#if !TARGET_OS_IPHONE
    CFStringRef issuer_cn = NULL;
    CFDictionaryRef dict;
#endif

    pj_assert(pool && ci && cert);

    /* Get issuer */
    issuer_info = CFStringCreateMutable(NULL, 0);
#if !TARGET_OS_IPHONE
{
    /* Unfortunately, unlike on Mac, on iOS we don't have these APIs
     * to query the certificate info such as the issuer, version,
     * validity, and alt names.
     */
    CFArrayRef issuer_vals;
    
    dict = get_cert_oid(cert, kSecOIDX509V1IssuerName,
    			(CFTypeRef *)&issuer_vals);
    if (dict) {
    	get_info_and_cn(issuer_vals, issuer_info, &issuer_cn);
    	if (issuer_cn)
    	    issuer_cn = CFStringCreateCopy(NULL, issuer_cn);
    	CFRelease(dict);
    }
}
#endif
    CFStringGetCString(issuer_info, buf, bufsize, kCFStringEncodingUTF8);

    /* Get serial no */
    if (__builtin_available(macOS 10.13, iOS 11.0, *)) {
	serial = SecCertificateCopySerialNumberData(cert, NULL);
    	if (serial) {
    	    serial_no = CFDataGetBytePtr(serial);
    	    serialsize = CFDataGetLength(serial);
    	}
    }

    /* Check if the contents need to be updated */
    update_needed = pj_strcmp2(&ci->issuer.info, buf) ||
                    pj_memcmp(ci->serial_no, serial_no, serialsize);
    if (!update_needed) {
        CFRelease(issuer_info);
        return;
    }

    /* Update cert info */

    pj_bzero(ci, sizeof(pj_ssl_cert_info));

    /* Version */
#if !TARGET_OS_IPHONE
{
    CFStringRef version;
    
    dict = get_cert_oid(cert, kSecOIDX509V1Version,
    			(CFTypeRef *)&version);
    if (dict) {
    	ci->version = CFStringGetIntValue(version);
    	CFRelease(dict);
    }
}
#endif

    /* Issuer */
    pj_strdup2(pool, &ci->issuer.info, buf);
#if !TARGET_OS_IPHONE
    if (issuer_cn) {
    	CFStringGetCString(issuer_cn, buf, bufsize, kCFStringEncodingUTF8);
    	pj_strdup2(pool, &ci->issuer.cn, buf);
    	CFRelease(issuer_cn);
    }
#endif
    CFRelease(issuer_info);

    /* Serial number */
    if (serial) {
    	if (serialsize > sizeof(ci->serial_no))
    	    serialsize = sizeof(ci->serial_no);
    	pj_memcpy(ci->serial_no, serial_no, serialsize);
    	CFRelease(serial);
    }

    /* Subject */
    str = SecCertificateCopySubjectSummary(cert);
    CFStringGetCString(str, buf, bufsize, kCFStringEncodingUTF8);
    pj_strdup2(pool, &ci->subject.cn, buf);
    CFRelease(str);
#if !TARGET_OS_IPHONE
{
    CFArrayRef subject;
    CFMutableStringRef subject_info;
    
    dict = get_cert_oid(cert, kSecOIDX509V1SubjectName,
    			(CFTypeRef *)&subject);
    if (dict) {
     	subject_info = CFStringCreateMutable(NULL, 0);

    	get_info_and_cn(subject, subject_info, &str);
    
    	CFStringGetCString(subject_info, buf, bufsize, kCFStringEncodingUTF8);
    	pj_strdup2(pool, &ci->subject.info, buf);
    
    	CFRelease(dict);
    	CFRelease(subject_info);
    }
}
#endif

    /* Validity */
#if !TARGET_OS_IPHONE
{
    CFNumberRef validity;
    double interval;
    
    dict = get_cert_oid(cert, kSecOIDX509V1ValidityNotBefore,
    			(CFTypeRef *)&validity);
    if (dict) {
        if (CFNumberGetValue(validity, CFNumberGetType(validity),
        		     &interval))
        {
            /* Darwin's absolute reference date is 1 Jan 2001 00:00:00 GMT */
    	    ci->validity.start.sec = (unsigned long)interval + 978278400L;
    	}
    	CFRelease(dict);
    }

    dict = get_cert_oid(cert, kSecOIDX509V1ValidityNotAfter,
    			(CFTypeRef *)&validity);
    if (dict) {
    	if (CFNumberGetValue(validity, CFNumberGetType(validity),
    			     &interval))
    	{
    	    ci->validity.end.sec = (unsigned long)interval + 978278400L;
    	}
    	CFRelease(dict);
    }
}
#endif

    /* Subject Alternative Name extension */
#if !TARGET_OS_IPHONE
{
    CFArrayRef altname;
    CFIndex i;

    dict = get_cert_oid(cert, kSecOIDSubjectAltName, (CFTypeRef *)&altname);
    if (!dict || !CFArrayGetCount(altname))
    	return;

    ci->subj_alt_name.entry = pj_pool_calloc(pool, CFArrayGetCount(altname),
					     sizeof(*ci->subj_alt_name.entry));

    for (i = 0; i < CFArrayGetCount(altname); ++i) {
    	CFDictionaryRef item;
    	CFStringRef label, value;
	pj_ssl_cert_name_type type = PJ_SSL_CERT_NAME_UNKNOWN;
        
        item = CFArrayGetValueAtIndex(altname, i);
        if (CFGetTypeID(item) != CFDictionaryGetTypeID())
            continue;
        
        label = (CFStringRef)CFDictionaryGetValue(item, kSecPropertyKeyLabel);
	if (CFGetTypeID(label) != CFStringGetTypeID())
	    continue;

        value = (CFStringRef)CFDictionaryGetValue(item, kSecPropertyKeyValue);

	if (!CFStringCompare(label, CFSTR("DNS Name"),
			     kCFCompareCaseInsensitive))
	{
	    if (CFGetTypeID(value) != CFStringGetTypeID())
	    	continue;
	    CFStringGetCString(value, buf, bufsize, kCFStringEncodingUTF8);
	    type = PJ_SSL_CERT_NAME_DNS;
	} else if (!CFStringCompare(label, CFSTR("IP Address"),
			     	    kCFCompareCaseInsensitive))
	{
	    if (CFGetTypeID(value) != CFStringGetTypeID())
	    	continue;
	    CFStringGetCString(value, buf, bufsize, kCFStringEncodingUTF8);
	    type = PJ_SSL_CERT_NAME_IP;
	} else if (!CFStringCompare(label, CFSTR("Email Address"),
			     	    kCFCompareCaseInsensitive))
	{
	    if (CFGetTypeID(value) != CFStringGetTypeID())
	    	continue;
	    CFStringGetCString(value, buf, bufsize, kCFStringEncodingUTF8);
	    type = PJ_SSL_CERT_NAME_RFC822;
	} else if (!CFStringCompare(label, CFSTR("URI"),
			     	    kCFCompareCaseInsensitive))
	{
	    CFStringRef uri;

	    if (CFGetTypeID(value) != CFURLGetTypeID())
	    	continue;
	    uri = CFURLGetString((CFURLRef)value);
	    CFStringGetCString(uri, buf, bufsize, kCFStringEncodingUTF8);
	    type = PJ_SSL_CERT_NAME_URI;
	}

	if (type != PJ_SSL_CERT_NAME_UNKNOWN) {
	    ci->subj_alt_name.entry[ci->subj_alt_name.cnt].type = type;
	    if (type == PJ_SSL_CERT_NAME_IP) {
	    	char ip_buf[PJ_INET6_ADDRSTRLEN+10];
	    	int len = CFStringGetLength(value);
		int af = pj_AF_INET();

		if (len == sizeof(pj_in6_addr)) af = pj_AF_INET6();
		pj_inet_ntop2(af, buf, ip_buf, sizeof(ip_buf));
		pj_strdup2(pool,
		    &ci->subj_alt_name.entry[ci->subj_alt_name.cnt].name,
		    ip_buf);
	    } else {
		pj_strdup2(pool,
	 	    &ci->subj_alt_name.entry[ci->subj_alt_name.cnt].name,
		    buf);
	    }
	    ci->subj_alt_name.cnt++;
	}
    }

    CFRelease(dict);
}
#endif
}

/* Update local & remote certificates info. This function should be
 * called after handshake successfully completed.
 */
static void ssl_update_certs_info(pj_ssl_sock_t *ssock)
{
    applessl_sock_t *assock = (applessl_sock_t *)ssock;
    SecTrustRef trust = assock->trust;
    CFIndex count;
    SecCertificateRef cert;

    pj_assert(ssock->ssl_state == SSL_STATE_ESTABLISHED);
    
    /* Get active local certificate */
    if (assock->identity) {
        CFArrayRef cert_arr;
        
    	cert_arr = sec_identity_copy_certificates_ref(assock->identity);
    	if (cert_arr) {
    	    count = CFArrayGetCount(cert_arr);
    	    if (count > 0) {
		CFTypeRef elmt;

    	    	elmt = (CFTypeRef) CFArrayGetValueAtIndex(cert_arr, 0);
    	    	if (CFGetTypeID(elmt) == SecCertificateGetTypeID()) {
    	    	    cert = (SecCertificateRef)elmt;
      	    	    get_cert_info(ssock->pool, &ssock->local_cert_info, cert);
      	    	}
    	    }    	    
    	    CFRelease(cert_arr);
    	}
    }

    /* Get active remote certificate */
    if (trust) {
    	count = SecTrustGetCertificateCount(trust);
    	if (count > 0) {
	    cert = SecTrustGetCertificateAtIndex(trust, 0);
      	    get_cert_info(ssock->pool, &ssock->remote_cert_info, cert);
    	}
    }
}

static void ssl_set_state(pj_ssl_sock_t *ssock, pj_bool_t is_server)
{
    PJ_UNUSED_ARG(ssock);
    PJ_UNUSED_ARG(is_server);
}

static void ssl_set_peer_name(pj_ssl_sock_t *ssock)
{
    /* Setting server name is done when configuring tls before connection
     * is started.
     */
    PJ_UNUSED_ARG(ssock);
}

static pj_status_t ssl_do_handshake(pj_ssl_sock_t *ssock)
{
    /* Nothing to do here, just return EPENDING. Handshake has
     * automatically been performed when starting a connection.
     */

    return PJ_EPENDING;
}

static pj_status_t ssl_read(pj_ssl_sock_t *ssock, void *data, int *size)
{
    pj_size_t circ_buf_size, read_size;

    pj_lock_acquire(ssock->circ_buf_input_mutex);

    if (circ_empty(&ssock->circ_buf_input)) {
        pj_lock_release(ssock->circ_buf_input_mutex);
        *size = 0;
	return PJ_SUCCESS;
    }

    circ_buf_size = circ_size(&ssock->circ_buf_input);
    read_size = PJ_MIN(circ_buf_size, *size);

    circ_read(&ssock->circ_buf_input, data, read_size);

    pj_lock_release(ssock->circ_buf_input_mutex);

    *size = read_size;

    return PJ_SUCCESS;
}

/*
 * Write the plain data to buffer. It will be encrypted later during
 * sending.
 */
static pj_status_t ssl_write(pj_ssl_sock_t *ssock, const void *data,
			     pj_ssize_t size, int *nwritten)
{
    pj_status_t status;

    status = circ_write(&ssock->circ_buf_output, data, size);
    *nwritten = (status == PJ_SUCCESS)? (int)size: 0;
    
    return status;
}

static pj_status_t ssl_renegotiate(pj_ssl_sock_t *ssock)
{
    PJ_UNUSED_ARG(ssock);

    /* According to the doc,
     * sec_protocol_options_set_tls_renegotiation_enabled() should
     * enable TLS session renegotiation for versions 1.2 and earlier.
     * But we can't trigger renegotiation manually, or can we?
     */
    return PJ_ENOTSUP;
}


#endif /* PJ_SSL_SOCK_IMP_APPLE */
