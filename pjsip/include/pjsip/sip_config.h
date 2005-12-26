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
#ifndef __PJSIP_SIP_CONFIG_H__
#define __PJSIP_SIP_CONFIG_H__

#include <pj/config.h>

/* Endpoint. */
#define PJSIP_MAX_TIMER_COUNT		(2*PJSIP_MAX_TSX_COUNT + 2*PJSIP_MAX_DIALOG_COUNT)
#define PJSIP_POOL_LEN_ENDPT		(2048+64*PJSIP_MAX_TSX_COUNT)
#define PJSIP_POOL_INC_ENDPT		(1024)

/* Transport related constants. */
#define PJSIP_MAX_TRANSPORTS		(PJ_IOQUEUE_MAX_HANDLES)
#define PJSIP_MAX_PKT_LEN		1500
#define PJSIP_POOL_RDATA_LEN		4000
#define PJSIP_POOL_RDATA_INC		4000
#define PJSIP_POOL_LEN_TRANSPORT	512
#define PJSIP_POOL_INC_TRANSPORT	512
#define PJSIP_POOL_LEN_TDATA		2500
#define PJSIP_POOL_INC_TDATA		512
#define PJSIP_POOL_LEN_UA		(64 + 32*PJSIP_MAX_DIALOG_COUNT)
#define PJSIP_POOL_INC_UA		0
#define PJSIP_TRANSPORT_CLOSE_TIMEOUT	30
#define PJSIP_MAX_TRANSPORT_USAGE	16

#define PJSIP_MAX_FORWARDS_VALUE	70

#define PJSIP_RFC3261_BRANCH_ID		"z9hG4bK"
#define PJSIP_RFC3261_BRANCH_LEN	7

/* Message/URL related constants. */
#define PJSIP_MAX_CALL_ID_LEN		PJ_GUID_STRING_LENGTH
#define PJSIP_MAX_TAG_LEN		PJ_GUID_STRING_LENGTH
#define PJSIP_MAX_BRANCH_LEN		(PJSIP_RFC3261_BRANCH_LEN + PJ_GUID_STRING_LENGTH)
#define PJSIP_MAX_URL_SIZE		256
#define PJSIP_MAX_HNAME_LEN		64

/* Transction related constants. */
#define PJSIP_MAX_TSX_COUNT		(16*1024)
#define PJSIP_POOL_LEN_TSX		1536 //768
#define PJSIP_POOL_INC_TSX		256
#define PJSIP_MAX_TSX_KEY_LEN		(PJSIP_MAX_URL_SIZE*2)

/* Dialog related constants. */
#define PJSIP_MAX_DIALOG_COUNT		(16*1024)
#define PJSIP_POOL_LEN_DIALOG		1200
#define PJSIP_POOL_INC_DIALOG		512

/* Transport manager hash table size (must be 2^n-1). */
#define PJSIP_TPMGR_HTABLE_SIZE		31

/* Transport idle timeout before it's destroyed. */
#define PJSIP_TRANSPORT_IDLE_TIME	30

/* Max entries to process in timer heap per poll. */
#define PJSIP_MAX_TIMED_OUT_ENTRIES	10

/* Module related constants. */
#define PJSIP_MAX_MODULE		8

/* Maximum header types. */
#define PJSIP_MAX_HEADER_TYPES		64

/* Maximum URI types. */
#define PJSIP_MAX_URI_TYPES		4

/*****************************************************************************
 *  Default timeout settings, in miliseconds. 
 */

//#define PJSIP_T1_TIMEOUT	15000
//#define PJSIP_T2_TIMEOUT	60000

/* T1 timeout value. */
#if !defined(PJSIP_T1_TIMEOUT)
#  define PJSIP_T1_TIMEOUT	500
#endif

/* T2 timeout value. */
#if !defined(PJSIP_T2_TIMEOUT)
#  define PJSIP_T2_TIMEOUT	4000
#endif

/* Completed timer for non-INVITE */
#if !defined(PJSIP_T4_TIMEOUT)
#  define PJSIP_T4_TIMEOUT	5000
#endif

/* Completed timer for INVITE */
#if !defined(PJSIP_TD_TIMEOUT)
#  define PJSIP_TD_TIMEOUT	32000
#endif


/*****************************************************************************
 *  Authorization
 */

/*
 * If this flag is set, the stack will keep the Authorization/Proxy-Authorization
 * headers that are sent in a cache. Future requests with the same realm and
 * the same method will use the headers in the cache (as long as no qop is
 * required by server).
 *
 * Turning on this flag will make authorization process goes faster, but
 * will grow the memory usage undefinitely until the dialog/registration
 * session is terminated.
 *
 * Default: 1
 */
#if !defined(PJSIP_AUTH_HEADER_CACHING)
#   define PJSIP_AUTH_HEADER_CACHING	    1
#endif

/*
 * If this flag is set, the stack will proactively send Authorization/Proxy-
 * Authorization header for next requests. If next request has the same method
 * with any of previous requests, then the last header which is saved in
 * the cache will be used (if PJSIP_AUTH_CACHING is set). Otherwise a fresh
 * header will be recalculated. If a particular server has requested qop, then
 * a fresh header will always be calculated.
 *
 * If this flag is NOT set, then the stack will only send Authorization/Proxy-
 * Authorization headers when it receives 401/407 response from server.
 *
 * Turning ON this flag will grow memory usage of a dialog/registration pool
 * indefinitely until it is terminated, because the stack needs to keep the
 * last WWW-Authenticate/Proxy-Authenticate challenge.
 *
 * Default: 1
 */
#if !defined(PJSIP_AUTH_AUTO_SEND_NEXT)
#   define PJSIP_AUTH_AUTO_SEND_NEXT	    1
#endif

/*
 * Support qop="auth" directive.
 * This option also requires client to cache the last challenge offered by
 * server.
 *
 * Default: 1
 */
#if !defined(PJSIP_AUTH_QOP_SUPPORT)
#   define PJSIP_AUTH_QOP_SUPPORT	    1
#endif


#include <pj/config.h>


#endif	/* __PJSIP_SIP_CONFIG_H__ */

