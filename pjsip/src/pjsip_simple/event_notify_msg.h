/* $Header: /pjproject/pjsip/src/pjsip_simple/event_notify_msg.h 4     8/24/05 10:33a Bennylp $ */
#ifndef __PJSIP_SIMPLE_EVENT_NOTIFY_MSG_H__
#define __PJSIP_SIMPLE_EVENT_NOTIFY_MSG_H__

/**
 * @file event_notify_msg.h
 * @brief SIP Event Notification Headers (RFC 3265)
 */
#include <pjsip/sip_msg.h>

/**
 * @ingroup PJSIP_EVENT_NOT
 * @{
 */

PJ_BEGIN_DECL


/** Max events in Allow-Events header. */
#define PJSIP_MAX_ALLOW_EVENTS	16

/**
 * This structure describes Event header.
 */
typedef struct pjsip_event_hdr
{
    PJSIP_DECL_HDR_MEMBER(struct pjsip_event_hdr)
    pj_str_t	    event_type;	    /**< Event name. */
    pj_str_t	    id_param;	    /**< Optional event ID parameter. */
    pj_str_t	    other_param;    /**< Other parameter, concatenated together. */
} pjsip_event_hdr;

/**
 * Create an Event header.
 *
 * @param pool	    The pool.
 *
 * @return	    New Event header instance.
 */
PJ_DECL(pjsip_event_hdr*) pjsip_event_hdr_create(pj_pool_t *pool);


/**
 * This structure describes Allow-Events header.
 */
typedef struct pjsip_allow_events_hdr
{
    PJSIP_DECL_HDR_MEMBER(struct pjsip_allow_events_hdr)
    int		    event_cnt;			    /**< Number of event names. */
    pj_str_t	    events[PJSIP_MAX_ALLOW_EVENTS]; /**< Event names. */
} pjsip_allow_events_hdr;


/**
 * Create a new Allow-Events header.
 *
 * @param pool.	    The pool.
 *
 * @return	    Allow-Events header.
 */
PJ_DECL(pjsip_allow_events_hdr*) pjsip_allow_events_hdr_create(pj_pool_t *pool);


/**
 * This structure describes Subscription-State header.
 */
typedef struct pjsip_sub_state_hdr
{
    PJSIP_DECL_HDR_MEMBER(struct pjsip_sub_state_hdr)
    pj_str_t	    sub_state;		/**< Subscription state. */
    pj_str_t	    reason_param;	/**< Optional termination reason. */
    int		    expires_param;	/**< Expires param, or -1. */
    int		    retry_after;	/**< Retry after param, or -1. */
    pj_str_t	    other_param;	/**< Other parameter, concatenated together. */
} pjsip_sub_state_hdr;

/**
 * Create new Subscription-State header.
 *
 * @param pool	    The pool.
 *
 * @return	    Subscription-State header.
 */
PJ_DECL(pjsip_sub_state_hdr*) pjsip_sub_state_hdr_create(pj_pool_t *pool);

/**
 * Initialize parser for event notify module.
 */
PJ_DEF(void) pjsip_event_notify_init_parser(void);


PJ_END_DECL


/**
 * @}
 */

#endif	/* __PJSIP_SIMPLE_EVENT_NOTIFY_MSG_H__ */

