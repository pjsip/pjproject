/* $Id */
/* 
 * Copyright (C) 2003-2005 Benny Prijono <benny@prijono.org>
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
#ifndef __PJ_STUN_H__
#define __PJ_STUN_H__

/**
 * @file stun.h
 * @brief STUN client.
 */

#include <pjlib-util/types.h>
#include <pj/sock.h>

/**
 * @defgroup PJLIB_UTIL_STUN_CLIENT Mini/Tiny STUN Client
 * @ingroup PJLIB_UTIL
 * @{
 */

PJ_BEGIN_DECL

/**
 * This enumeration describes STUN message types.
 */
typedef enum pj_stun_msg_type
{
    PJ_STUN_BINDING_REQUEST		    = 0x0001,
    PJ_STUN_BINDING_RESPONSE		    = 0x0101,
    PJ_STUN_BINDING_ERROR_RESPONSE	    = 0x0111,
    PJ_STUN_SHARED_SECRET_REQUEST	    = 0x0002,
    PJ_STUN_SHARED_SECRET_RESPONSE	    = 0x0102,
    PJ_STUN_SHARED_SECRET_ERROR_RESPONSE    = 0x0112
} pj_stun_msg_type;


/**
 * This enumeration describes STUN attribute types.
 */
typedef enum pj_stun_attr_type
{
    PJ_STUN_ATTR_MAPPED_ADDR = 1,
    PJ_STUN_ATTR_RESPONSE_ADDR,
    PJ_STUN_ATTR_CHANGE_REQUEST,
    PJ_STUN_ATTR_SOURCE_ADDR,
    PJ_STUN_ATTR_CHANGED_ADDR,
    PJ_STUN_ATTR_USERNAME,
    PJ_STUN_ATTR_PASSWORD,
    PJ_STUN_ATTR_MESSAGE_INTEGRITY,
    PJ_STUN_ATTR_ERROR_CODE,
    PJ_STUN_ATTR_UNKNOWN_ATTRIBUTES,
    PJ_STUN_ATTR_REFLECTED_FORM
} pj_stun_attr_type;


/**
 * This structre describes STUN message header.
 */
typedef struct pj_stun_msg_hdr
{
    pj_uint16_t		type;
    pj_uint16_t		length;
    pj_uint32_t		tsx[4];
} pj_stun_msg_hdr;


/**
 * This structre describes STUN attribute header.
 */
typedef struct pj_stun_attr_hdr
{
    pj_uint16_t		type;
    pj_uint16_t		length;
} pj_stun_attr_hdr;


/**
 * This structre describes STUN MAPPED-ADDR attribute.
 */
typedef struct pj_stun_mapped_addr_attr
{
    pj_stun_attr_hdr	hdr;
    pj_uint8_t		ignored;
    pj_uint8_t		family;
    pj_uint16_t		port;
    pj_uint32_t		addr;
} pj_stun_mapped_addr_attr;

typedef pj_stun_mapped_addr_attr pj_stun_response_addr_attr;
typedef pj_stun_mapped_addr_attr pj_stun_changed_addr_attr;
typedef pj_stun_mapped_addr_attr pj_stun_src_addr_attr;
typedef pj_stun_mapped_addr_attr pj_stun_reflected_form_attr;

typedef struct pj_stun_change_request_attr
{
    pj_stun_attr_hdr	hdr;
    pj_uint32_t		value;
} pj_stun_change_request_attr;

typedef struct pj_stun_username_attr
{
    pj_stun_attr_hdr	hdr;
    pj_uint32_t		value[1];
} pj_stun_username_attr;

typedef pj_stun_username_attr pj_stun_password_attr;

typedef struct pj_stun_error_code_attr
{
    pj_stun_attr_hdr	hdr;
    pj_uint16_t		ignored;
    pj_uint8_t		err_class;
    pj_uint8_t		number;
    char		reason[4];
} pj_stun_error_code_attr;

typedef struct pj_stun_msg
{
    pj_stun_msg_hdr    *hdr;
    int			attr_count;
    pj_stun_attr_hdr   *attr[PJ_STUN_MAX_ATTR];
} pj_stun_msg;

/* STUN message API (stun.c). */

PJ_DECL(pj_status_t) pj_stun_create_bind_req( pj_pool_t *pool, 
					      void **msg, pj_size_t *len,
					      pj_uint32_t id_hi,
					      pj_uint32_t id_lo);
PJ_DECL(pj_status_t) pj_stun_parse_msg( void *buf, pj_size_t len,
				        pj_stun_msg *msg);
PJ_DECL(void*) pj_stun_msg_find_attr( pj_stun_msg *msg, pj_stun_attr_type t);


/**
 * This is the main function to request the mapped address of local sockets
 * to multiple STUN servers. This function is able to find the mapped 
 * addresses of multiple sockets simultaneously, and for each socket, two
 * requests will be sent to two different STUN servers to see if both servers
 * get the same public address for the same socket. (Note that application can
 * specify the same address for the two servers, but still two requests will
 * be sent for each server).
 *
 * This function will perform necessary retransmissions of the requests if
 * response is not received within a predetermined period. When all responses
 * have been received, the function will compare the mapped addresses returned
 * by the servers, and when both are equal, the address will be returned in
 * \a mapped_addr argument.
 *
 * @param pf		The pool factory where memory will be allocated from.
 * @param sock_cnt	Number of sockets in the socket array.
 * @param sock		Array of local UDP sockets which public addresses are
 *			to be queried from the STUN servers.
 * @param srv1		Host name or IP address string of the first STUN
 *			server.
 * @param port1		The port number of the first STUN server. 
 * @param srv2		Host name or IP address string of the second STUN
 *			server.
 * @param port2		The port number of the second STUN server. 
 * @param mapped_addr	Array to receive the mapped public address of the local
 *			UDP sockets, when the function returns PJ_SUCCESS.
 *
 * @return		This functions returns PJ_SUCCESS if responses are
 *			received from all servers AND all servers returned the
 *			same mapped public address. Otherwise this function may
 *			return one of the following error codes:
 *			- PJLIB_UTIL_ESTUNNOTRESPOND: no respons from servers.
 *			- PJLIB_UTIL_ESTUNSYMMETRIC: different mapped addresses
 *			  are returned by servers.
 *			- etc.
 *
 */
PJ_DECL(pj_status_t) pj_stun_get_mapped_addr( pj_pool_factory *pf,
					      int sock_cnt, pj_sock_t sock[],
					      const pj_str_t *srv1, int port1,
					      const pj_str_t *srv2, int port2,
					      pj_sockaddr_in mapped_addr[]);

PJ_END_DECL

/**
 * @}
 */

#endif	/* __PJ_STUN_H__ */

