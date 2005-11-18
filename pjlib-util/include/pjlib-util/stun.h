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

#include <pj/types.h>
#include <pj/sock.h>

PJ_BEGIN_DECL

#define PJ_STUN_MAX_ATTR    16

typedef enum pj_stun_msg_type
{
    PJ_STUN_BINDING_REQUEST		    = 0x0001,
    PJ_STUN_BINDING_RESPONSE		    = 0x0101,
    PJ_STUN_BINDING_ERROR_RESPONSE	    = 0x0111,
    PJ_STUN_SHARED_SECRET_REQUEST	    = 0x0002,
    PJ_STUN_SHARED_SECRET_RESPONSE	    = 0x0102,
    PJ_STUN_SHARED_SECRET_ERROR_RESPONSE    = 0x0112
} pj_stun_msg_type;

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

typedef struct pj_stun_msg_hdr
{
    pj_uint16_t		type;
    pj_uint16_t		length;
    pj_uint32_t		tsx[4];
} pj_stun_msg_hdr;

typedef struct pj_stun_attr_hdr
{
    pj_uint16_t		type;
    pj_uint16_t		length;
} pj_stun_attr_hdr;

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

/* STUN simple client API (stun_client.c) */
enum pj_stun_err_code {
    PJ_STUN_ERR_MEMORY		= (-2),
    PJ_STUN_ERR_RESOLVE		= (-3),
    PJ_STUN_ERR_TRANSPORT	= (-4),
    PJ_STUN_ERR_INVALID_MSG	= (-5),
    PJ_STUN_ERR_NO_RESPONSE	= (-6),
    PJ_STUN_ERR_SYMETRIC	= (-7),
};

PJ_DECL(pj_status_t) pj_stun_get_mapped_addr( pj_pool_factory *pf,
					      int sock_cnt, pj_sock_t sock[],
					      const pj_str_t *srv1, int port1,
					      const pj_str_t *srv2, int port2,
					      pj_sockaddr_in mapped_addr[]);
PJ_DECL(const char*) pj_stun_get_err_msg(pj_status_t status);

PJ_END_DECL

#endif	/* __PJ_STUN_H__ */

