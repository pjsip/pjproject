/* $Id$ */
/* 
 * Copyright (C) 2011-2011 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJMEDIA_EVENT_H__
#define __PJMEDIA_EVENT_H__

/**
 * @file pjmedia/event.h
 * @brief Event framework
 */
#include <pjmedia/format.h>
#include <pj/list.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJMEDIA_EVENT Event Framework
 * @brief PJMEDIA event framework
 * @{
 */

/**
 * This enumeration describes list of media events.
 */
typedef enum pjmedia_event_type
{
    /**
     * No event.
     */
    PJMEDIA_EVENT_NONE,

    /**
     * Media format has changed event.
     */
    PJMEDIA_EVENT_FMT_CHANGED,

    /**
     * Video window is being closed.
     */
    PJMEDIA_EVENT_WND_CLOSING,

    /**
     * Video window has been closed event.
     */
    PJMEDIA_EVENT_WND_CLOSED,

    /**
     * Video window has been resized event.
     */
    PJMEDIA_EVENT_WND_RESIZED,

    /**
     * Mouse button has been pressed event.
     */
    PJMEDIA_EVENT_MOUSE_BTN_DOWN,

    /**
     * Video key frame has just been decoded event.
     */
    PJMEDIA_EVENT_KEY_FRAME_FOUND,

    /**
     * Video decoding error due to missing key frame event.
     */
    PJMEDIA_EVENT_KEY_FRAME_MISSING,

    /**
     * Start of user event.
     */
    PJMEDIA_EVENT_USER = 100

} pjmedia_event_type;

/**
 * Forward declaration for event subscription.
 */
typedef struct pjmedia_event_subscription pjmedia_event_subscription;

/**
 * Forward declaration for event publisher.
 */
typedef struct pjmedia_event_publisher pjmedia_event_publisher;

/**
 * Additional data/parameters for media format changed event
 * (PJMEDIA_EVENT_FMT_CHANGED).
 */
typedef struct pjmedia_event_fmt_changed_data
{
    /** The media flow direction */
    pjmedia_dir		dir;

    /** The new media format. */
    pjmedia_format	new_fmt;
} pjmedia_event_fmt_changed_data;

/**
 * Additional data/parameters are not needed.
 */
typedef struct pjmedia_event_dummy_data
{
    /** Dummy data */
    int			dummy;
} pjmedia_event_dummy_data;

/**
 * Additional data/parameters for window resized event
 * (PJMEDIA_EVENT_WND_RESIZED).
 */
typedef struct pjmedia_event_wnd_resized_data
{
    /**
     * The new window size.
     */
    pjmedia_rect_size	new_size;
} pjmedia_event_wnd_resized_data;

/**
 * Additional data/parameters for window closing event.
 */
typedef struct pjmedia_event_wnd_closing_data
{
    /** Consumer may set this field to PJ_TRUE to cancel the closing */
    pj_bool_t		cancel;
} pjmedia_event_wnd_closing_data;

/** Additional parameters for window changed event. */
typedef pjmedia_event_dummy_data pjmedia_event_wnd_closed_data;

/** Additional parameters for mouse button down event */
typedef pjmedia_event_dummy_data pjmedia_event_mouse_btn_down_data;

/** Additional parameters for key frame found event */
typedef pjmedia_event_dummy_data pjmedia_event_key_frame_found_data;

/** Additional parameters for key frame missing event */
typedef pjmedia_event_dummy_data pjmedia_event_key_frame_missing_data;

/**
 * Maximum size of additional parameters section in pjmedia_event structure
 */
#define PJMEDIA_EVENT_DATA_MAX_SIZE	sizeof(pjmedia_event_fmt_changed_data)

/** Type of storage to hold user data in pjmedia_event structure */
typedef char pjmedia_event_user_data[PJMEDIA_EVENT_DATA_MAX_SIZE];

/**
 * This structure describes a media event. It consists mainly of the event
 * type and additional data/parameters for the event. Event publishers need
 * to use #pjmedia_event_init() to initialize this event structure with
 * basic information about the event.
 */
typedef struct pjmedia_event
{
    /**
     * The event type.
     */
    pjmedia_event_type			 type;

    /**
     * The media timestamp when the event occurs.
     */
    pj_timestamp		 	 timestamp;

    /**
     * This keeps count on the number of subscribers that have
     * processed this event.
     */
    unsigned				 proc_cnt;

    /**
     * Pointer information about the source of this event. This field
     * is provided mainly so that the event subscribers can compare it
     * against the publisher that it subscribed the events from initially,
     * a publisher can republish events from other publisher. Event
     * subscription must be careful when using this pointer other than for
     * comparison purpose, since access to the publisher may require special
     * care (e.g. mutex locking).
     */
    const pjmedia_event_publisher	*epub;

    /**
     * Additional data/parameters about the event. The type of data
     * will be specific to the event type being reported.
     */
    union {
	/** Media format changed event data. */
	pjmedia_event_fmt_changed_data		fmt_changed;

	/** Window resized event data */
	pjmedia_event_wnd_resized_data		wnd_resized;

	/** Window closing event data. */
	pjmedia_event_wnd_closing_data		wnd_closing;

	/** Window closed event data */
	pjmedia_event_wnd_closed_data		wnd_closed;

	/** Mouse button down event data */
	pjmedia_event_mouse_btn_down_data	mouse_btn_down;

	/** Key frame found event data */
	pjmedia_event_key_frame_found_data	key_frm_found;

	/** Key frame missing event data */
	pjmedia_event_key_frame_missing_data	key_frm_missing;

	/** Storage for user event data */
	pjmedia_event_user_data			user;

	/** Pointer to storage to user event data, if it's outside
	 * this struct
	 */
	void					*ptr;
    } data;
} pjmedia_event;

/**
 * The callback to receive media events. The callback should increase
 * \a proc_cnt field of the event if it processes the event.
 *
 * @param esub		The subscription that was made initially to receive
 * 			this event.
 * @param event		The media event itself.
 *
 * @return		If the callback returns non-PJ_SUCCESS, this return
 * 			code may be propagated back to the producer.
 */
typedef pj_status_t pjmedia_event_cb(pjmedia_event_subscription *esub,
				     pjmedia_event *event);

/**
 * This structure keeps the data needed to maintain an event subscription.
 * This data is normally kept by event publishers.
 */
struct pjmedia_event_subscription
{
    /** Standard list members */
    PJ_DECL_LIST_MEMBER(pjmedia_event_subscription);

    /** Callback that will be called by publisher to report events. */
    pjmedia_event_cb	*cb;

    /** User data for this subscription */
    void		*user_data;
};

/**
 * This describes an event publisher. An event publisher is an object that
 * maintains event subscriptions. When an event is published on behalf of
 * a publisher with #pjmedia_event_publish(), that event will be propagated
 * to all of the subscribers registered to the publisher.
 */
struct pjmedia_event_publisher
{
    /** List of subscriptions for this event publisher */
    pjmedia_event_subscription	subscription_list;
};

/**
 * Initialize event structure with basic data about the event.
 *
 * @param event		The event to be initialized.
 * @param type		The event type to be set for this event.
 * @param ts		Event timestamp. May be set to NULL to set the event
 * 			timestamp to zero.
 * @param epub		Event publisher.
 */
PJ_DECL(void) pjmedia_event_init(pjmedia_event *event,
                                 pjmedia_event_type type,
                                 const pj_timestamp *ts,
                                 const pjmedia_event_publisher *epub);

/**
 * Initialize an event publisher structure.
 *
 * @param epub		The event publisher.
 */
PJ_DECL(void) pjmedia_event_publisher_init(pjmedia_event_publisher *epub);

/**
 * Initialize subscription data.
 *
 * @param esub		The event subscription.
 * @param cb		The callback to receive events.
 * @param user_data	Arbitrary user data to be associated with the
 * 			subscription.
 */
PJ_DECL(void) pjmedia_event_subscription_init(pjmedia_event_subscription *esub,
                                              pjmedia_event_cb *cb,
                                              void *user_data);

/**
 * Subscribe to events published by the specified publisher using the
 * specified subscription object. The callback and user data fields of
 * the subscription object must have been initialized prior to calling
 * this function, and the subscription object must be kept alive throughout
 * the duration of the subscription (e.g. it must not be allocated from
 * the stack).
 *
 * Note that the subscriber may receive not only events emitted by
 * the specific publisher specified in the argument, but also from other
 * publishers contained by the publisher, if the publisher is republishing
 * events from other publishers.
 *
 * @param epub		The event publisher.
 * @param esub 		The event subscription object.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_event_subscribe(pjmedia_event_publisher *epub,
                                             pjmedia_event_subscription *esub);

/**
 * Unsubscribe from the specified publisher.
 *
 * @param epub		The event publisher.
 * @param esub		The event subscription object, which must be the same
 * 			object that was given to #pjmedia_event_subscribe().
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_event_unsubscribe(pjmedia_event_publisher *epub,
                                               pjmedia_event_subscription *esub);

/**
 * Check if the specified publisher has subscribers.
 *
 * @param epub		The event publisher.
 *
 * @return		PJ_TRUE if the publisher has at least one subscriber.
 */
PJ_DECL(pj_bool_t)
pjmedia_event_publisher_has_sub(pjmedia_event_publisher *epub);

/**
 * Publish the specified event to all subscribers of the specified event
 * publisher.
 *
 * @param epub		The event publisher.
 * @param event		The event to be published.
 *
 * @return		PJ_SUCCESS only if all subscription callbacks returned
 * 			PJ_SUCCESS.
 */
PJ_DECL(pj_status_t) pjmedia_event_publish(pjmedia_event_publisher *epub,
                                           pjmedia_event *event);

/**
 * Subscribe to events produced by the source publisher in \a esrc and
 * republish the events to all subscribers in \a epub publisher.
 *
 * @param esrc		The event source from which events will be
 * 			republished.
 * @param epub		Events from the event source above will be
 * 			republished to subscribers of this publisher.
 * @param esub		The subscription object to be used to subscribe
 * 			to \a esrc. This doesn't need to be initialized,
 * 			but it must be kept alive throughout the lifetime
 * 			of the subsciption.
 *
 * @return		PJ_SUCCESS only if all subscription callbacks returned
 * 			PJ_SUCCESS.
 */
PJ_DECL(pj_status_t) pjmedia_event_republish(pjmedia_event_publisher *esrc,
                                             pjmedia_event_publisher *epub,
                                             pjmedia_event_subscription *esub);

/**
 * @}  PJMEDIA_EVENT
 */


PJ_END_DECL

#endif	/* __PJMEDIA_EVENT_H__ */
