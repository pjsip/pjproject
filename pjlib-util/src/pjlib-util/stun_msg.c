/* $Id$ */
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
#include <pjlib-util/stun_msg.h>
#include <pjlib-util/crc32.h>
#include <pjlib-util/errno.h>
#include <pjlib-util/hmac_sha1.h>
#include <pjlib-util/md5.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/rand.h>
#include <pj/string.h>

#define THIS_FILE		"stun_msg.c"
#define STUN_XOR_FINGERPRINT	0x5354554eL

static const char *stun_method_names[] = 
{
    "Unknown",			/* 0 */
    "Binding",			/* 1 */
    "Shared Secret",		/* 2 */
    "Allocate",			/* 3 */
    "Send",			/* 4 */
    "Data",			/* 5 */
    "Set Active Destination",   /* 6 */
    "Connect",			/* 7 */
    "Connect Status"		/* 8 */
};

static struct
{
    int err_code;
    const char *err_msg;
} stun_err_msg_map[] = 
{
    { PJ_STUN_STATUS_TRY_ALTERNATE,	    "Try Alternate"}, 
    { PJ_STUN_STATUS_BAD_REQUEST,	    "Bad Request"},
    { PJ_STUN_STATUS_UNAUTHORIZED,	    "Unauthorized"},
    { PJ_STUN_STATUS_UNKNOWN_ATTRIBUTE,	    "Unknown Attribute"},
    { PJ_STUN_STATUS_STALE_CREDENTIALS,	    "Stale Credentials"},
    { PJ_STUN_STATUS_INTEGRITY_CHECK_FAILURE,"Integrity Check Failure"},
    { PJ_STUN_STATUS_MISSING_USERNAME,	    "Missing Username"},
    { PJ_STUN_STATUS_USE_TLS,		    "Use TLS"},
    { PJ_STUN_STATUS_MISSING_REALM,	    "Missing Realm"},
    { PJ_STUN_STATUS_MISSING_NONCE,	    "Missing Nonce"},
    { PJ_STUN_STATUS_UNKNOWN_USERNAME,	    "Unknown Username"},
    { PJ_STUN_STATUS_NO_BINDING,	    "No Binding"},
    { PJ_STUN_STATUS_STALE_NONCE,	    "Stale Nonce"},
    { PJ_STUN_STATUS_TRANSITIONING,	    "Transitioning"},
    { PJ_STUN_STATUS_WRONG_USERNAME,	    "Wrong Username"},
    { PJ_STUN_STATUS_UNSUPP_TRANSPORT_PROTO,"Unsupported Transport Protocol"},
    { PJ_STUN_STATUS_INVALID_IP_ADDR,	    "Invalid IP Address"},
    { PJ_STUN_STATUS_INVALID_PORT,	    "Invalid Port"},
    { PJ_STUN_STATUS_OPER_TCP_ONLY,	    "Operation for TCP Only"},
    { PJ_STUN_STATUS_CONNECTION_FAILURE,    "Connection Failure"},
    { PJ_STUN_STATUS_CONNECTION_TIMEOUT,    "Connection Timeout"},
    { PJ_STUN_STATUS_SERVER_ERROR,	    "Server Error"},
    { PJ_STUN_STATUS_GLOBAL_FAILURE,	    "Global Failure"}
};



struct attr_desc
{
    const char   *name;
    pj_status_t	(*decode_attr)(pj_pool_t *pool, const pj_uint8_t *buf, 
			       void **p_attr);
    pj_status_t (*encode_attr)(const void *a, pj_uint8_t *buf, 
			       unsigned len, unsigned *printed);

};

static pj_status_t decode_generic_ip_addr_attr(pj_pool_t *pool, 
					       const pj_uint8_t *buf, 
					       void **p_attr);
static pj_status_t encode_generic_ip_addr_attr(const void *a, pj_uint8_t *buf, 
					       unsigned len, 
					       unsigned *printed);
static pj_status_t decode_generic_string_attr(pj_pool_t *pool, 
					      const pj_uint8_t *buf, 
					      void **p_attr);
static pj_status_t encode_generic_string_attr(const void *a, pj_uint8_t *buf, 
					      unsigned len, unsigned *printed);
static pj_status_t decode_msg_integrity_attr(pj_pool_t *pool, 
					     const pj_uint8_t *buf,
					     void **p_attr);
static pj_status_t encode_msg_integrity_attr(const void *a, pj_uint8_t *buf, 
					     unsigned len, unsigned *printed);
static pj_status_t decode_error_code_attr(pj_pool_t *pool, 
					  const pj_uint8_t *buf,
					  void **p_attr);
static pj_status_t encode_error_code_attr(const void *a, pj_uint8_t *buf, 
					  unsigned len, unsigned *printed);
static pj_status_t decode_unknown_attr(pj_pool_t *pool, 
				       const pj_uint8_t *buf, 
				       void **p_attr);
static pj_status_t encode_unknown_attr(const void *a, pj_uint8_t *buf, 
				       unsigned len, unsigned *printed);
static pj_status_t decode_generic_uint_attr(pj_pool_t *pool, 
					    const pj_uint8_t *buf, 
					    void **p_attr);
static pj_status_t encode_generic_uint_attr(const void *a, pj_uint8_t *buf, 
					    unsigned len, unsigned *printed);
static pj_status_t decode_binary_attr(pj_pool_t *pool, 
				      const pj_uint8_t *buf,
				      void **p_attr);
static pj_status_t encode_binary_attr(const void *a, pj_uint8_t *buf, 
				      unsigned len, unsigned *printed);
static pj_status_t decode_empty_attr(pj_pool_t *pool, 
				     const pj_uint8_t *buf, 
				     void **p_attr);
static pj_status_t encode_empty_attr(const void *a, pj_uint8_t *buf, 
				     unsigned len, unsigned *printed);


struct attr_desc mandatory_attr_desc[] = 
{
    {
	/* type zero */
	NULL,
	NULL,
	NULL
    },
    {
	/* PJ_STUN_ATTR_MAPPED_ADDR, */
	"MAPPED-ADDRESS",
	&decode_generic_ip_addr_attr,
	&encode_generic_ip_addr_attr
    },
    {
	/* PJ_STUN_ATTR_RESPONSE_ADDR, */
	"RESPONSE-ADDRESS",
	&decode_generic_ip_addr_attr,
	&encode_generic_ip_addr_attr
    },
    {
	/* PJ_STUN_ATTR_CHANGE_REQUEST, */
	"CHANGE-REQUEST",
	&decode_generic_uint_attr,
	&encode_generic_uint_attr
    },
    {
	/* PJ_STUN_ATTR_SOURCE_ADDR, */
	"SOURCE-ADDRESS",
	&decode_generic_ip_addr_attr,
	&encode_generic_ip_addr_attr
    },
    {
	/* PJ_STUN_ATTR_CHANGED_ADDR, */
	"CHANGED-ADDRESS",
	&decode_generic_ip_addr_attr,
	&encode_generic_ip_addr_attr
    },
    {
	/* PJ_STUN_ATTR_USERNAME, */
	"USERNAME",
	&decode_generic_string_attr,
	&encode_generic_string_attr
    },
    {
	/* PJ_STUN_ATTR_PASSWORD, */
	"PASSWORD",
	&decode_generic_string_attr,
	&encode_generic_string_attr
    },
    {
	/* PJ_STUN_ATTR_MESSAGE_INTEGRITY, */
	"MESSAGE-INTEGRITY",
	&decode_msg_integrity_attr,
	&encode_msg_integrity_attr
    },
    {
	/* PJ_STUN_ATTR_ERROR_CODE, */
	"ERROR-CODE",
	&decode_error_code_attr,
	&encode_error_code_attr
    },
    {
	/* PJ_STUN_ATTR_UNKNOWN_ATTRIBUTES, */
	"UNKNOWN-ATTRIBUTES",
	&decode_unknown_attr,
	&encode_unknown_attr
    },
    {
	/* PJ_STUN_ATTR_REFLECTED_FROM, */
	"REFLECTED-FROM",
	&decode_generic_ip_addr_attr,
	&encode_generic_ip_addr_attr
    },
    {
	/* ID 0x000C is not assigned */
	NULL,
	NULL,
	NULL
    },
    {
	/* PJ_STUN_ATTR_LIFETIME, */
	"LIFETIME",
	&decode_generic_uint_attr,
	&encode_generic_uint_attr
    },
    {
	/* ID 0x000E is not assigned */
	NULL,
	NULL,
	NULL
    },
    {
	/* ID 0x000F is not assigned */
	NULL,
	NULL,
	NULL
    },
    {
	/* PJ_STUN_ATTR_BANDWIDTH, */
	"BANDWIDTH",
	&decode_generic_uint_attr,
	&encode_generic_uint_attr
    },
    {
	/* ID 0x0011 is not assigned */
	NULL,
	NULL,
	NULL
    },
    {
	/* PJ_STUN_ATTR_REMOTE_ADDRESS, */
	"REMOTE-ADDRESS",
	&decode_generic_ip_addr_attr,
	&encode_generic_ip_addr_attr
    },
    {
	/* PJ_STUN_ATTR_DATA, */
	"DATA",
	&decode_binary_attr,
	&encode_binary_attr
    },
    {
	/* PJ_STUN_ATTR_REALM, */
	"REALM",
	&decode_generic_string_attr,
	&encode_generic_string_attr
    },
    {
	/* PJ_STUN_ATTR_NONCE, */
	"NONCE",
	&decode_generic_string_attr,
	&encode_generic_string_attr
    },
    {
	/* PJ_STUN_ATTR_RELAY_ADDRESS, */
	"RELAY-ADDRESS",
	&decode_generic_ip_addr_attr,
	&encode_generic_ip_addr_attr
    },
    {
	/* PJ_STUN_ATTR_REQUESTED_ADDR_TYPE, */
	"REQUESTED-ADDRESS-TYPE",
	&decode_generic_uint_attr,
	&encode_generic_uint_attr
    },
    {
	/* PJ_STUN_ATTR_REQUESTED_PORT_PROPS, */
	"REQUESTED-PORT-PROPS",
	&decode_generic_uint_attr,
	&encode_generic_uint_attr
    },
    {
	/* PJ_STUN_ATTR_REQUESTED_TRANSPORT, */
	"REQUESTED-TRANSPORT",
	&decode_generic_uint_attr,
	&encode_generic_uint_attr
    },
    {
	/* ID 0x001A is not assigned */
	NULL,
	NULL,
	NULL
    },
    {
	/* ID 0x001B is not assigned */
	NULL,
	NULL,
	NULL
    },
    {
	/* ID 0x001C is not assigned */
	NULL,
	NULL,
	NULL
    },
    {
	/* ID 0x001D is not assigned */
	NULL,
	NULL,
	NULL
    },
    {
	/* ID 0x001E is not assigned */
	NULL,
	NULL,
	NULL
    },
    {
	/* ID 0x001F is not assigned */
	NULL,
	NULL,
	NULL
    },
    {
	/* PJ_STUN_ATTR_XOR_MAPPED_ADDRESS, */
	"XOR-MAPPED-ADDRESS",
	&decode_generic_ip_addr_attr,
	&encode_generic_ip_addr_attr
    },
    {
	/* PJ_STUN_ATTR_TIMER_VAL, */
	"TIMER-VAL",
	&decode_generic_uint_attr,
	&encode_generic_uint_attr
    },
    {
	/* PJ_STUN_ATTR_REQUESTED_IP, */
	"REQUESTED-IP",
	&decode_generic_ip_addr_attr,
	&encode_generic_ip_addr_attr
    },
    {
	/* PJ_STUN_ATTR_XOR_REFLECTED_FROM, */
	"XOR-REFLECTED-FROM",
	&decode_generic_ip_addr_attr,
	&encode_generic_ip_addr_attr
    },
    {
	/* PJ_STUN_ATTR_PRIORITY, */
	"PRIORITY",
	&decode_generic_uint_attr,
	&encode_generic_uint_attr
    },
    {
	/* PJ_STUN_ATTR_USE_CANDIDATE, */
	"USE-CANDIDATE",
	&decode_empty_attr,
	&encode_empty_attr
    },
    {
	/* PJ_STUN_ATTR_XOR_INTERNAL_ADDR, */
	"XOR-INTERNAL-ADDRESS",
	&decode_generic_ip_addr_attr,
	&encode_generic_ip_addr_attr
    },

    /* Sentinel */
    {
	/* PJ_STUN_ATTR_END_MANDATORY_ATTR */
	NULL,
	NULL,
	NULL
    }
};

static struct attr_desc extended_attr_desc[] =
{
    {
	/* PJ_STUN_ATTR_FINGERPRINT, */
	"FINGERPRINT",
	&decode_generic_uint_attr,
	&encode_generic_uint_attr
    },
    {
	/* PJ_STUN_ATTR_SERVER, */
	"SERVER",
	&decode_generic_string_attr,
	&encode_generic_string_attr
    },
    {
	/* PJ_STUN_ATTR_ALTERNATE_SERVER, */
	"ALTERNATE-SERVER",
	&decode_generic_ip_addr_attr,
	&encode_generic_ip_addr_attr
    },
    {
	/* PJ_STUN_ATTR_REFRESH_INTERVAL, */
	"REFRESH-INTERVAL",
	&decode_generic_uint_attr,
	&encode_generic_uint_attr
    },
};



/*
 * Get STUN message type name.
 */
PJ_DEF(const char*) pj_stun_get_method_name(unsigned msg_type)
{
    unsigned method = PJ_STUN_GET_METHOD(msg_type);

    if (method >= PJ_ARRAY_SIZE(stun_method_names))
	return "???";

    return stun_method_names[method];
}


/*
 * Get STUN message class name.
 */
PJ_DEF(const char*) pj_stun_get_class_name(unsigned msg_type)
{
    if (PJ_STUN_IS_REQUEST(msg_type))
	return "request";
    else if (PJ_STUN_IS_RESPONSE(msg_type))
	return "success response";
    else if (PJ_STUN_IS_ERROR_RESPONSE(msg_type))
	return "error response";
    else if (PJ_STUN_IS_INDICATION(msg_type))
	return "indication";
    else
	return "???";
}


static const struct attr_desc *find_attr_desc(unsigned attr_type)
{
    struct attr_desc *desc;

    /* Check that attr_desc array is valid */
    pj_assert(PJ_ARRAY_SIZE(mandatory_attr_desc)==
	      PJ_STUN_ATTR_END_MANDATORY_ATTR+1);
    pj_assert(mandatory_attr_desc[PJ_STUN_ATTR_END_MANDATORY_ATTR].decode_attr
	      == NULL);
    pj_assert(mandatory_attr_desc[PJ_STUN_ATTR_USE_CANDIDATE].decode_attr 
	      == &decode_empty_attr);
    pj_assert(PJ_ARRAY_SIZE(extended_attr_desc) ==
	      PJ_STUN_ATTR_END_EXTENDED_ATTR-PJ_STUN_ATTR_START_EXTENDED_ATTR);

    if (attr_type < PJ_STUN_ATTR_START_EXTENDED_ATTR)
	desc = &mandatory_attr_desc[attr_type];
    else if (attr_type >= PJ_STUN_ATTR_START_EXTENDED_ATTR &&
	     attr_type < PJ_STUN_ATTR_END_EXTENDED_ATTR)
	desc = &extended_attr_desc[attr_type-PJ_STUN_ATTR_START_EXTENDED_ATTR];
    else
	return NULL;

    return desc->decode_attr == NULL ? NULL : desc;
}


/*
 * Get STUN attribute name.
 */
PJ_DEF(const char*) pj_stun_get_attr_name(unsigned attr_type)
{
    const struct attr_desc *attr_desc;

    attr_desc = find_attr_desc(attr_type);
    if (!attr_desc || attr_desc->name==NULL)
	return "???";

    return attr_desc->name;
}


/**
 * Get STUN standard reason phrase for the specified error code.
 */
PJ_DEF(pj_str_t) pj_stun_get_err_reason(int err_code)
{
    unsigned i;

    for (i=0; i<PJ_ARRAY_SIZE(stun_err_msg_map); ++i) {
	if (stun_err_msg_map[i].err_code == err_code)
	    return pj_str((char*)stun_err_msg_map[i].err_msg);
    }
    return pj_str(NULL);
}



#define INIT_ATTR(a,t,l)    (a)->hdr.type=(pj_uint16_t)(t), \
			    (a)->hdr.length=(pj_uint16_t)(l)
#define ATTR_HDR_LEN	    4

#define getval16(p, pos)    (pj_uint16_t)(((p)[(pos)] << 8) | \
					  ((p)[(pos) + 1] << 0))


//////////////////////////////////////////////////////////////////////////////
/*
 * STUN generic IP address container
 */
#define STUN_GENERIC_IP_ADDR_LEN	8

/*
 * Create a generic STUN IP address attribute for IPv4 address.
 */
PJ_DEF(pj_status_t) 
pj_stun_generic_ip_addr_attr_create(pj_pool_t *pool,
				    int attr_type,
				    pj_bool_t xor_ed,
				    unsigned addr_len,
				    const pj_sockaddr_t *addr,
				    pj_stun_generic_ip_addr_attr **p_attr)
{
    pj_stun_generic_ip_addr_attr *attr;

    PJ_ASSERT_RETURN(pool && addr_len && addr && p_attr, PJ_EINVAL);
    PJ_ASSERT_RETURN(addr_len == sizeof(pj_sockaddr_in) ||
		     addr_len == sizeof(pj_sockaddr_in6), PJ_EINVAL);

    attr = PJ_POOL_ZALLOC_TYPE(pool, pj_stun_generic_ip_addr_attr);
    INIT_ATTR(attr, attr_type, STUN_GENERIC_IP_ADDR_LEN);

    if (!xor_ed) {
	pj_memcpy(&attr->addr, addr, addr_len);
    } else if (addr_len == sizeof(pj_sockaddr_in)) {
	const pj_sockaddr_in *addr4 = (const pj_sockaddr_in*) addr;

	pj_sockaddr_in_init(&attr->addr.ipv4, NULL, 0);
	attr->addr.ipv4.sin_port = (pj_uint16_t)(addr4->sin_port ^ 0x2112);
	attr->addr.ipv4.sin_addr.s_addr = (addr4->sin_addr.s_addr ^ 
					   pj_htonl(0x2112A442));
    } else if (addr_len == sizeof(pj_sockaddr_in6)) {
	return PJLIB_UTIL_ESTUNIPV6NOTSUPP;
    } else {
	return PJLIB_UTIL_ESTUNINADDRLEN;
    }

    *p_attr = attr;

    return PJ_SUCCESS;
}


/*
 * Create and add generic STUN IP address attribute to a STUN message.
 */
PJ_DEF(pj_status_t) 
pj_stun_msg_add_generic_ip_addr_attr(pj_pool_t *pool,
				     pj_stun_msg *msg,
				     int attr_type, 
				     pj_bool_t xor_ed,
				     unsigned addr_len,
				     const pj_sockaddr_t *addr)
{
    pj_stun_generic_ip_addr_attr *attr;
    pj_status_t status;

    status = pj_stun_generic_ip_addr_attr_create(pool, attr_type, xor_ed,
					         addr_len, addr, &attr);
    if (status != PJ_SUCCESS)
	return status;

    return pj_stun_msg_add_attr(msg, &attr->hdr);
}

static pj_status_t decode_generic_ip_addr_attr(pj_pool_t *pool, 
					       const pj_uint8_t *buf, 
					       void **p_attr)
{
    pj_stun_generic_ip_addr_attr *attr;
    pj_uint32_t val;

    /* Create the attribute */
    attr = PJ_POOL_ZALLOC_TYPE(pool, pj_stun_generic_ip_addr_attr);
    pj_memcpy(attr, buf, ATTR_HDR_LEN);

    /* Convert to host byte order */
    attr->hdr.type = pj_ntohs(attr->hdr.type);
    attr->hdr.length = pj_ntohs(attr->hdr.length);

    /* Check that the attribute length is valid */
    if (attr->hdr.length != STUN_GENERIC_IP_ADDR_LEN)
	return PJLIB_UTIL_ESTUNINATTRLEN;

    /* Check address family */
    val = *(pj_uint8_t*)(buf + ATTR_HDR_LEN + 1);

    /* Check address family is valid (only supports ipv4 for now) */
    if (val != 1)
	return PJLIB_UTIL_ESTUNIPV6NOTSUPP;

    /* Get port and address */
    pj_sockaddr_in_init(&attr->addr.ipv4, NULL, 0);
    attr->addr.ipv4.sin_port = getval16(buf, ATTR_HDR_LEN + 2);
    pj_memcpy(&attr->addr.ipv4.sin_addr, buf+ATTR_HDR_LEN+4, 4);

    /* Done */
    *p_attr = attr;

    return PJ_SUCCESS;
}


static pj_status_t encode_generic_ip_addr_attr(const void *a, pj_uint8_t *buf, 
					       unsigned len, unsigned *printed)
{
    enum {
	ATTR_LEN = ATTR_HDR_LEN + STUN_GENERIC_IP_ADDR_LEN
    };
    pj_uint8_t *start_buf = buf;
    const pj_stun_generic_ip_addr_attr *ca = 
	(const pj_stun_generic_ip_addr_attr *)a;
    pj_stun_generic_ip_addr_attr *attr;

    if (len < ATTR_LEN) 
	return PJ_ETOOSMALL;

    /* Copy and convert headers to network byte order */
    pj_memcpy(buf, a, ATTR_HDR_LEN);
    attr = (pj_stun_generic_ip_addr_attr*) buf;
    attr->hdr.type = pj_htons(attr->hdr.type);
    attr->hdr.length = pj_htons((pj_uint16_t)STUN_GENERIC_IP_ADDR_LEN);
    buf += ATTR_HDR_LEN;
    
    /* Ignored */
    *buf++ = '\0';

    /* Family (IPv4 only for now) */
    PJ_ASSERT_RETURN(ca->addr.addr.sa_family == PJ_AF_INET, PJ_EINVAL);
    *buf++ = 1;

    /* Port */
    pj_memcpy(buf, &ca->addr.ipv4.sin_port, 2);
    buf += 2;

    /* Address */
    pj_memcpy(buf, &ca->addr.ipv4.sin_addr, 4);
    buf += 4;

    pj_assert(buf - start_buf == ATTR_LEN);

    /* Done */
    *printed = buf - start_buf;

    return PJ_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////////
/*
 * STUN generic string attribute
 */

/*
 * Create a STUN generic string attribute.
 */
PJ_DEF(pj_status_t) 
pj_stun_generic_string_attr_create(pj_pool_t *pool,
				   int attr_type,
				   const pj_str_t *value,
				   pj_stun_generic_string_attr **p_attr)
{
    pj_stun_generic_string_attr *attr;

    PJ_ASSERT_RETURN(pool && value && p_attr, PJ_EINVAL);

    attr = PJ_POOL_ZALLOC_TYPE(pool, pj_stun_generic_string_attr);
    INIT_ATTR(attr, attr_type, value->slen);
    pj_strdup(pool, &attr->value, value);

    *p_attr = attr;

    return PJ_SUCCESS;
}


/*
 * Create and add STUN generic string attribute to the message.
 */
PJ_DEF(pj_status_t) 
pj_stun_msg_add_generic_string_attr(pj_pool_t *pool,
				    pj_stun_msg *msg,
				    int attr_type,
				    const pj_str_t *value)
{
    pj_stun_generic_string_attr *attr;
    pj_status_t status;

    status = pj_stun_generic_string_attr_create(pool, attr_type, value, 
						&attr);
    if (status != PJ_SUCCESS)
	return status;

    return pj_stun_msg_add_attr(msg, &attr->hdr);
}


static pj_status_t decode_generic_string_attr(pj_pool_t *pool, 
					      const pj_uint8_t *buf, 
					      void **p_attr)
{
    pj_stun_generic_string_attr *attr;
    pj_str_t value;

    /* Create the attribute */
    attr = PJ_POOL_ZALLOC_TYPE(pool, pj_stun_generic_string_attr);

    /* Copy the header */
    pj_memcpy(attr, buf, ATTR_HDR_LEN);

    /* Convert to host byte order */
    attr->hdr.type = pj_ntohs(attr->hdr.type);
    attr->hdr.length = pj_ntohs(attr->hdr.length);

    /* Get pointer to the string in the message */
    value.ptr = ((char*)buf + ATTR_HDR_LEN);
    value.slen = attr->hdr.length;

    /* Copy the string to the attribute */
    pj_strdup(pool, &attr->value, &value);

    /* Done */
    *p_attr = attr;

    return PJ_SUCCESS;

}


static pj_status_t encode_generic_string_attr(const void *a, pj_uint8_t *buf, 
					      unsigned len, unsigned *printed)
{
    const pj_stun_generic_string_attr *ca = 
	(const pj_stun_generic_string_attr*)a;
    pj_stun_attr_hdr *attr;

    /* Calculated total attr_len (add padding if necessary) */
    *printed = (ca->value.slen + ATTR_HDR_LEN + 3) & (~3);
    if (len < *printed) {
	*printed = 0;
	return PJ_ETOOSMALL;
    }

    /* Copy header */
    pj_memcpy(buf, a, ATTR_HDR_LEN);
    attr = (pj_stun_attr_hdr*)buf;

    /* Set the correct length */
    attr->length = (pj_uint16_t) ca->value.slen;

    /* Convert to network byte order */
    attr->type = pj_htons(attr->type);
    attr->length = pj_htons(attr->length);

    /* Copy the string */
    pj_memcpy(buf+ATTR_HDR_LEN, ca->value.ptr, ca->value.slen);

    /* Done */
    return PJ_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////////
/*
 * STUN empty attribute (used by USE-CANDIDATE).
 */

/*
 * Create a STUN empty attribute.
 */
PJ_DEF(pj_status_t) 
pj_stun_empty_attr_create(pj_pool_t *pool,
			  int attr_type,
			  pj_stun_empty_attr **p_attr)
{
    pj_stun_empty_attr *attr;

    PJ_ASSERT_RETURN(pool && p_attr, PJ_EINVAL);

    attr = PJ_POOL_ZALLOC_TYPE(pool, pj_stun_empty_attr);
    INIT_ATTR(attr, attr_type, sizeof(pj_stun_empty_attr));

    *p_attr = attr;

    return PJ_SUCCESS;
}


static pj_status_t decode_empty_attr(pj_pool_t *pool, 
				     const pj_uint8_t *buf, 
				     void **p_attr)
{
    pj_stun_empty_attr *attr;

    /* Check that the struct address is valid */
    pj_assert(sizeof(pj_stun_empty_attr) == ATTR_HDR_LEN);

    /* Create the attribute */
    attr = PJ_POOL_ZALLOC_TYPE(pool, pj_stun_empty_attr);
    pj_memcpy(attr, buf, ATTR_HDR_LEN);

    /* Convert to host byte order */
    attr->hdr.type = pj_ntohs(attr->hdr.type);
    attr->hdr.length = pj_ntohs(attr->hdr.length);

    /* Check that the attribute length is valid */
    if (attr->hdr.length != ATTR_HDR_LEN)
	return PJLIB_UTIL_ESTUNINATTRLEN;

    /* Done */
    *p_attr = attr;

    return PJ_SUCCESS;
}


static pj_status_t encode_empty_attr(const void *a, pj_uint8_t *buf, 
				     unsigned len, unsigned *printed)
{
    pj_stun_empty_attr *attr;

    if (len < ATTR_HDR_LEN) 
	return PJ_ETOOSMALL;

    /* Copy and convert attribute to network byte order */
    pj_memcpy(buf, a, ATTR_HDR_LEN);
    attr = (pj_stun_empty_attr*) buf;
    attr->hdr.type = pj_htons(attr->hdr.type);
    pj_assert(attr->hdr.length == ATTR_HDR_LEN);
    attr->hdr.length = pj_htons(ATTR_HDR_LEN);

    /* Done */
    *printed = ATTR_HDR_LEN;

    return PJ_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////////
/*
 * STUN generic 32bit integer attribute.
 */
#define STUN_UINT_LEN	4

/*
 * Create a STUN generic 32bit value attribute.
 */
PJ_DEF(pj_status_t) 
pj_stun_generic_uint_attr_create(pj_pool_t *pool,
				 int attr_type,
				 pj_uint32_t value,
				 pj_stun_generic_uint_attr **p_attr)
{
    pj_stun_generic_uint_attr *attr;

    PJ_ASSERT_RETURN(pool && p_attr, PJ_EINVAL);

    attr = PJ_POOL_ZALLOC_TYPE(pool, pj_stun_generic_uint_attr);
    INIT_ATTR(attr, attr_type, STUN_UINT_LEN);
    attr->value = value;

    *p_attr = attr;

    return PJ_SUCCESS;
}

/* Create and add STUN generic 32bit value attribute to the message. */
PJ_DEF(pj_status_t) 
pj_stun_msg_add_generic_uint_attr(pj_pool_t *pool,
				  pj_stun_msg *msg,
				  int attr_type,
				  pj_uint32_t value)
{
    pj_stun_generic_uint_attr *attr;
    pj_status_t status;

    status = pj_stun_generic_uint_attr_create(pool, attr_type, value, &attr);
    if (status != PJ_SUCCESS)
	return status;

    return pj_stun_msg_add_attr(msg, &attr->hdr);
}

static pj_status_t decode_generic_uint_attr(pj_pool_t *pool, 
					    const pj_uint8_t *buf, 
					    void **p_attr)
{
    enum
    {
	ATTR_LEN = STUN_UINT_LEN + ATTR_HDR_LEN
    };
    pj_stun_generic_uint_attr *attr;

    /* Check that the struct address is valid */
    pj_assert(sizeof(pj_stun_generic_uint_attr) == ATTR_LEN);

    /* Create the attribute */
    attr = PJ_POOL_ZALLOC_TYPE(pool, pj_stun_generic_uint_attr);
    pj_memcpy(attr, buf, ATTR_LEN);

    /* Convert to host byte order */
    attr->hdr.type = pj_ntohs(attr->hdr.type);
    attr->hdr.length = pj_ntohs(attr->hdr.length);
    attr->value = pj_ntohl(attr->value);

    /* Check that the attribute length is valid */
    if (attr->hdr.length != STUN_UINT_LEN)
	return PJLIB_UTIL_ESTUNINATTRLEN;

    /* Done */
    *p_attr = attr;

    return PJ_SUCCESS;
}


static pj_status_t encode_generic_uint_attr(const void *a, pj_uint8_t *buf, 
					    unsigned len, unsigned *printed)
{
    enum
    {
	ATTR_LEN = STUN_UINT_LEN + ATTR_HDR_LEN
    };
    pj_stun_generic_uint_attr *attr;

    if (len < ATTR_LEN) 
	return PJ_ETOOSMALL;

    /* Copy and convert attribute to network byte order */
    pj_memcpy(buf, a, ATTR_LEN);
    attr = (pj_stun_generic_uint_attr*) buf;
    attr->hdr.type = pj_htons(attr->hdr.type);
    pj_assert(attr->hdr.length == STUN_UINT_LEN);
    attr->hdr.length = pj_htons(STUN_UINT_LEN);
    attr->value = pj_htonl(attr->value);

    /* Done */
    *printed = ATTR_LEN;

    return PJ_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
/*
 * STUN MESSAGE-INTEGRITY attribute.
 */

#define STUN_MSG_INTEGRITY_LEN	20

/*
 * Create a STUN MESSAGE-INTEGRITY attribute.
 */
PJ_DEF(pj_status_t) 
pj_stun_msg_integrity_attr_create(pj_pool_t *pool,
				  pj_stun_msg_integrity_attr **p_attr)
{
    pj_stun_msg_integrity_attr *attr;

    PJ_ASSERT_RETURN(pool && p_attr, PJ_EINVAL);

    attr = PJ_POOL_ZALLOC_TYPE(pool, pj_stun_msg_integrity_attr);
    INIT_ATTR(attr, PJ_STUN_ATTR_MESSAGE_INTEGRITY, STUN_MSG_INTEGRITY_LEN);

    *p_attr = attr;

    return PJ_SUCCESS;
}


static pj_status_t decode_msg_integrity_attr(pj_pool_t *pool, 
					     const pj_uint8_t *buf,
					     void **p_attr)
{
    enum
    {
	ATTR_LEN = STUN_MSG_INTEGRITY_LEN + ATTR_HDR_LEN
    };
    pj_stun_msg_integrity_attr *attr;

    /* Check that struct size is valid */
    pj_assert(sizeof(pj_stun_msg_integrity_attr)==STUN_MSG_INTEGRITY_LEN);

    /* Create attribute */
    attr = PJ_POOL_ZALLOC_TYPE(pool, pj_stun_msg_integrity_attr);
    pj_memcpy(attr, buf, sizeof(pj_stun_msg_integrity_attr));
    attr->hdr.type = pj_ntohs(attr->hdr.type);
    attr->hdr.length = pj_ntohs(attr->hdr.length);

    /* Check that the attribute length is valid */
    if (attr->hdr.length != STUN_MSG_INTEGRITY_LEN)
	return PJLIB_UTIL_ESTUNINATTRLEN;

    /* Done */
    *p_attr = attr;
    return PJ_SUCCESS;
}


static pj_status_t encode_msg_integrity_attr(const void *a, pj_uint8_t *buf, 
					     unsigned len, unsigned *printed)
{
    enum
    {
	ATTR_LEN = STUN_MSG_INTEGRITY_LEN + ATTR_HDR_LEN
    };
    pj_stun_msg_integrity_attr *attr;

    if (len < ATTR_LEN) 
	return PJ_ETOOSMALL;

    /* Copy and convert attribute to network byte order */
    pj_memcpy(buf, a, ATTR_LEN);
    attr = (pj_stun_msg_integrity_attr*) buf;
    attr->hdr.type = pj_htons(attr->hdr.type);
    pj_assert(attr->hdr.length == STUN_MSG_INTEGRITY_LEN);
    attr->hdr.length = pj_htons(STUN_MSG_INTEGRITY_LEN);

    /* Done */
    *printed = ATTR_LEN;

    return PJ_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
/*
 * STUN ERROR-CODE
 */

/*
 * Create a STUN ERROR-CODE attribute.
 */
PJ_DEF(pj_status_t) 
pj_stun_error_code_attr_create(pj_pool_t *pool,
			       int err_code,
			       const pj_str_t *err_reason,
			       pj_stun_error_code_attr **p_attr)
{
    pj_stun_error_code_attr *attr;
    char err_buf[80];
    pj_str_t str;

    PJ_ASSERT_RETURN(pool && err_code && p_attr, PJ_EINVAL);

    if (err_reason == NULL) {
	str = pj_stun_get_err_reason(err_code);
	if (str.slen == 0) {
	    str.slen = pj_ansi_snprintf(err_buf, sizeof(err_buf),
				        "Unknown error %d", err_code);
	    str.ptr = err_buf;
	}
	err_reason = &str;
    }

    attr = PJ_POOL_ZALLOC_TYPE(pool, pj_stun_error_code_attr);
    INIT_ATTR(attr, PJ_STUN_ATTR_ERROR_CODE, 4+err_reason->slen);
    attr->err_class = (pj_uint8_t)(err_code / 100);
    attr->number = (pj_uint8_t) (err_code % 100);
    pj_strdup(pool, &attr->reason, err_reason);

    *p_attr = attr;

    return PJ_SUCCESS;
}


static pj_status_t decode_error_code_attr(pj_pool_t *pool, 
					  const pj_uint8_t *buf,
					  void **p_attr)
{
    pj_stun_error_code_attr *attr;
    pj_str_t value;

    /* Create the attribute */
    attr = PJ_POOL_ZALLOC_TYPE(pool, pj_stun_error_code_attr);

    /* Copy the header */
    pj_memcpy(attr, buf, ATTR_HDR_LEN + 4);

    /* Convert to host byte order */
    attr->hdr.type = pj_ntohs(attr->hdr.type);
    attr->hdr.length = pj_ntohs(attr->hdr.length);

    /* Get pointer to the string in the message */
    value.ptr = ((char*)buf + ATTR_HDR_LEN + 4);
    value.slen = attr->hdr.length - 4;

    /* Copy the string to the attribute */
    pj_strdup(pool, &attr->reason, &value);

    /* Done */
    *p_attr = attr;

    return PJ_SUCCESS;
}


static pj_status_t encode_error_code_attr(const void *a, pj_uint8_t *buf, 
					  unsigned len, unsigned *printed)
{
    const pj_stun_error_code_attr *ca = 
	(const pj_stun_error_code_attr*)a;
    pj_stun_error_code_attr *attr;

    if (len < ATTR_HDR_LEN + 4 + (unsigned)ca->reason.slen) 
	return PJ_ETOOSMALL;

    /* Copy and convert attribute to network byte order */
    pj_memcpy(buf, ca, ATTR_HDR_LEN + 4);

    /* Update length */
    attr = (pj_stun_error_code_attr*) buf;
    attr->hdr.length = (pj_uint16_t)(4 + ca->reason.slen);

    /* Convert fiends to network byte order */
    attr->hdr.type = pj_htons(attr->hdr.type);
    attr->hdr.length = pj_htons(attr->hdr.length);

    /* Copy error string */
    pj_memcpy(buf + ATTR_HDR_LEN + 4, ca->reason.ptr, ca->reason.slen);

    /* Done */
    *printed = (ATTR_HDR_LEN + 4 + ca->reason.slen + 3) & (~3);

    return PJ_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
/*
 * STUN UNKNOWN-ATTRIBUTES attribute
 */

/*
 * Create an empty instance of STUN UNKNOWN-ATTRIBUTES attribute.
 *
 * @param pool		The pool to allocate memory from.
 * @param p_attr	Pointer to receive the attribute.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DEF(pj_status_t) 
pj_stun_unknown_attr_create(pj_pool_t *pool,
			    unsigned attr_cnt,
			    const pj_uint16_t attr_array[],
			    pj_stun_unknown_attr **p_attr)
{
    pj_stun_unknown_attr *attr;
    unsigned i;

    PJ_ASSERT_RETURN(pool && attr_cnt < PJ_STUN_MAX_ATTR && p_attr, PJ_EINVAL);

    attr = PJ_POOL_ZALLOC_TYPE(pool, pj_stun_unknown_attr);
    INIT_ATTR(attr, PJ_STUN_ATTR_UNKNOWN_ATTRIBUTES, attr_cnt * 2);

    attr->attr_count = attr_cnt;
    for (i=0; i<attr_cnt; ++i) {
	attr->attrs[i] = attr_array[i];
    }

    /* If the number of unknown attributes is an odd number, one of the
     * attributes MUST be repeated in the list.
     */
    if ((attr_cnt & 0x01)) {
	attr->attrs[attr_cnt] = attr_array[attr_cnt-1];
    }

    *p_attr = NULL;

    return PJ_SUCCESS;
}


/* Create and add STUN UNKNOWN-ATTRIBUTES attribute to the message. */
PJ_DEF(pj_status_t) 
pj_stun_msg_add_unknown_attr(pj_pool_t *pool,
			     pj_stun_msg *msg,
			     unsigned attr_cnt,
			     const pj_uint16_t attr_types[])
{
    pj_stun_unknown_attr *attr;
    pj_status_t status;

    status = pj_stun_unknown_attr_create(pool, attr_cnt, attr_types, &attr);
    if (status != PJ_SUCCESS)
	return status;

    return pj_stun_msg_add_attr(msg, &attr->hdr);
}

static pj_status_t decode_unknown_attr(pj_pool_t *pool, 
				       const pj_uint8_t *buf, 
				       void **p_attr)
{
    pj_stun_unknown_attr *attr;
    const pj_uint16_t *punk_attr;
    unsigned i;

    attr = PJ_POOL_ZALLOC_TYPE(pool, pj_stun_unknown_attr);
    pj_memcpy(attr, buf, ATTR_HDR_LEN);

    attr->hdr.type = pj_ntohs(attr->hdr.type);
    attr->hdr.length = pj_ntohs(attr->hdr.length);

    attr->attr_count = (attr->hdr.length >> 1);
    if (attr->attr_count > PJ_STUN_MAX_ATTR)
	return PJ_ETOOMANY;

    punk_attr = (const pj_uint16_t*)(buf + ATTR_HDR_LEN);
    for (i=0; i<attr->attr_count; ++i) {
	attr->attrs[i] = pj_ntohs(punk_attr[i]);
    }

    /* Done */
    *p_attr = attr;

    return PJ_SUCCESS;
}


static pj_status_t encode_unknown_attr(const void *a, pj_uint8_t *buf, 
				       unsigned len, unsigned *printed)
{
    const pj_stun_unknown_attr *ca = (const pj_stun_unknown_attr*) a;
    pj_stun_unknown_attr *attr;
    pj_uint16_t *dst_unk_attr;
    unsigned i;

    /* Check that buffer is enough */
    if (len < ATTR_HDR_LEN + (ca->attr_count << 1))
	return PJ_ETOOSMALL;

    /* Copy to message */
    pj_memcpy(buf, ca, ATTR_HDR_LEN);

    /* Set the correct length */
    attr = (pj_stun_unknown_attr *) buf;
    attr->hdr.length = (pj_uint16_t)(ca->attr_count << 1);

    /* Convert to network byte order */
    attr->hdr.type = pj_htons(attr->hdr.type);
    attr->hdr.length = pj_htons(attr->hdr.length);

    /* Copy individual attribute */
    dst_unk_attr = (pj_uint16_t*)(buf + ATTR_HDR_LEN);
    for (i=0; i < ca->attr_count; ++i, ++dst_unk_attr) {
	*dst_unk_attr = pj_htons(attr->attrs[i]);
    }

    /* Done */
    *printed = (ATTR_HDR_LEN + (ca->attr_count << 1) + 3) & (~3);

    return PJ_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////////
/*
 * STUN generic binary attribute
 */

/*
 * Create a blank binary attribute.
 */
PJ_DEF(pj_status_t)
pj_stun_binary_attr_create(pj_pool_t *pool,
			   int attr_type,
			   const pj_uint8_t *data,
			   unsigned length,
			   pj_stun_binary_attr **p_attr)
{
    pj_stun_binary_attr *attr;

    PJ_ASSERT_RETURN(pool && attr_type && p_attr, PJ_EINVAL);

    attr = PJ_POOL_ZALLOC_TYPE(pool, pj_stun_binary_attr);
    INIT_ATTR(attr, attr_type, sizeof(pj_stun_binary_attr));

    if (data && length) {
	attr->length = length;
	attr->data = pj_pool_alloc(pool, length);
	pj_memcpy(attr->data, data, length);
    }

    *p_attr = attr;

    return PJ_SUCCESS;
}


/* Create and add binary attr. */
PJ_DEF(pj_status_t)
pj_stun_msg_add_binary_attr(pj_pool_t *pool,
			    pj_stun_msg *msg,
			    int attr_type,
			    const pj_uint8_t *data,
			    unsigned length)
{
    pj_stun_binary_attr *attr;
    pj_status_t status;

    status = pj_stun_binary_attr_create(pool, attr_type,
					data, length, &attr);
    if (status != PJ_SUCCESS)
	return status;

    return pj_stun_msg_add_attr(msg, &attr->hdr);
}


static pj_status_t decode_binary_attr(pj_pool_t *pool, 
				      const pj_uint8_t *buf,
				      void **p_attr)
{
    pj_stun_binary_attr *attr;

    /* Create the attribute */
    attr = PJ_POOL_ZALLOC_TYPE(pool, pj_stun_binary_attr);

    /* Copy the header */
    pj_memcpy(attr, buf, ATTR_HDR_LEN);

    /* Convert to host byte order */
    attr->hdr.type = pj_ntohs(attr->hdr.type);
    attr->hdr.length = pj_ntohs(attr->hdr.length);

    /* Copy the data to the attribute */
    attr->length = attr->hdr.length;
    attr->data = pj_pool_alloc(pool, attr->length);
    pj_memcpy(attr->data, buf+ATTR_HDR_LEN, attr->length);

    /* Done */
    *p_attr = attr;

    return PJ_SUCCESS;

}


static pj_status_t encode_binary_attr(const void *a, pj_uint8_t *buf, 
				      unsigned len, unsigned *printed)
{
    const pj_stun_binary_attr *ca = (const pj_stun_binary_attr*)a;
    pj_stun_attr_hdr *attr;

    /* Calculated total attr_len (add padding if necessary) */
    *printed = (ca->length + ATTR_HDR_LEN + 3) & (~3);
    if (len < *printed)
	return PJ_ETOOSMALL;

    /* Copy header */
    pj_memcpy(buf, a, ATTR_HDR_LEN);

    /* Set the correct length */
    attr = (pj_stun_attr_hdr*)buf;
    attr->length = (pj_uint16_t) ca->length;

    /* Convert to network byte order */
    attr->type = pj_htons(attr->type);
    attr->length = pj_htons(attr->length);

    /* Copy the data */
    pj_memcpy(buf+ATTR_HDR_LEN, ca->data, ca->length);

    /* Done */
    return PJ_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////////

/*
 * Create a blank STUN message.
 */
PJ_DEF(pj_status_t) pj_stun_msg_create( pj_pool_t *pool,
					unsigned msg_type,
					pj_uint32_t magic,
					const pj_uint8_t tsx_id[12],
					pj_stun_msg **p_msg)
{
    pj_stun_msg *msg;

    PJ_ASSERT_RETURN(pool && msg_type && p_msg, PJ_EINVAL);

    msg = PJ_POOL_ZALLOC_TYPE(pool, pj_stun_msg);
    msg->hdr.type = (pj_uint16_t) msg_type;
    msg->hdr.magic = magic;

    if (tsx_id) {
	pj_memcpy(&msg->hdr.tsx_id, tsx_id, sizeof(msg->hdr.tsx_id));
    } else {
	struct transaction_id
	{
	    pj_uint32_t	    proc_id;
	    pj_uint32_t	    random;
	    pj_uint32_t	    counter;
	} id;
	static pj_uint32_t pj_stun_tsx_id_counter;

	id.proc_id = pj_getpid();
	id.random = pj_rand();
	id.counter = pj_stun_tsx_id_counter++;

	pj_memcpy(&msg->hdr.tsx_id, &id, sizeof(msg->hdr.tsx_id));
    }

    *p_msg = msg;

    return PJ_SUCCESS;
}


/*
 * Add STUN attribute to STUN message.
 */
PJ_DEF(pj_status_t) pj_stun_msg_add_attr(pj_stun_msg *msg,
					 pj_stun_attr_hdr *attr)
{
    PJ_ASSERT_RETURN(msg && attr, PJ_EINVAL);
    PJ_ASSERT_RETURN(msg->attr_count < PJ_STUN_MAX_ATTR, PJ_ETOOMANY);

    msg->attr[msg->attr_count++] = attr;
    return PJ_SUCCESS;
}


PJ_INLINE(pj_uint16_t) GET_VAL16(const pj_uint8_t *pdu, unsigned pos)
{
    pj_uint16_t val = (pj_uint16_t) ((pdu[pos] << 8) + pdu[pos+1]);
    return pj_ntohs(val);
}

PJ_INLINE(pj_uint32_t) GET_VAL32(const pj_uint8_t *pdu, unsigned pos)
{
    pj_uint32_t val = (pdu[pos+0] << 24) +
		      (pdu[pos+1] << 16) +
		      (pdu[pos+2] <<  8) +
		      (pdu[pos+3]);
    return pj_ntohl(val);
}


/*
 * Check that the PDU is potentially a valid STUN message.
 */
PJ_DEF(pj_status_t) pj_stun_msg_check(const pj_uint8_t *pdu, unsigned pdu_len,
				      unsigned options)
{
    unsigned msg_len;

    PJ_ASSERT_RETURN(pdu, PJ_EINVAL);

    if (pdu_len < sizeof(pj_stun_msg_hdr))
	return PJLIB_UTIL_ESTUNINMSGLEN;

    /* First byte of STUN message is always 0x00 or 0x01. */
    if (*pdu != 0x00 && *pdu != 0x01)
	return PJLIB_UTIL_ESTUNINMSGTYPE;

    /* If magic is set, then there is great possibility that this is
     * a STUN message.
     */
    if (GET_VAL32(pdu, 4) != PJ_STUN_MAGIC)
	return PJLIB_UTIL_ESTUNNOTMAGIC;

    /* Check the PDU length */
    msg_len = GET_VAL16(pdu, 2);
    if ((msg_len > pdu_len) || 
	((options & PJ_STUN_IS_DATAGRAM) && msg_len != pdu_len))
    {
	return PJLIB_UTIL_ESTUNINMSGLEN;
    }

    /* Check if FINGERPRINT attribute is present */
    if (GET_VAL16(pdu, msg_len + 20) == PJ_STUN_ATTR_FINGERPRINT) {
	pj_uint16_t attr_len = GET_VAL16(pdu, msg_len + 22);
	pj_uint32_t fingerprint = GET_VAL32(pdu, msg_len + 24);
	pj_uint32_t crc;

	if (attr_len != 4)
	    return PJLIB_UTIL_ESTUNINATTRLEN;

	crc = pj_crc32_calc(pdu, msg_len + 20);
	crc ^= STUN_XOR_FINGERPRINT;

	if (crc != fingerprint)
	    return PJLIB_UTIL_ESTUNFINGERPRINT;
    }

    /* Could be a STUN message */
    return PJ_SUCCESS;
}


/* Create error response */
PJ_DEF(pj_status_t) pj_stun_msg_create_response(pj_pool_t *pool,
						const pj_stun_msg *req_msg,
						unsigned err_code,
						const pj_str_t *err_msg,
						pj_stun_msg **p_response)
{
    unsigned msg_type = req_msg->hdr.type;
    pj_stun_msg *response;
    pj_stun_error_code_attr *err_attr;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && p_response, PJ_EINVAL);

    PJ_ASSERT_RETURN(PJ_STUN_IS_REQUEST(msg_type), 
		     PJLIB_UTIL_ESTUNINMSGTYPE);

    /* Create response or error response */
    if (err_code)
	msg_type |= PJ_STUN_ERROR_RESPONSE_BIT;
    else
	msg_type |= PJ_STUN_RESPONSE_BIT;

    status = pj_stun_msg_create(pool, msg_type, req_msg->hdr.magic, 
				req_msg->hdr.tsx_id, &response);
    if (status != PJ_SUCCESS) {
	return status;
    }

    /* Add error code attribute */
    if (err_code) {
	status = pj_stun_error_code_attr_create(pool, err_code, err_msg,
						&err_attr);
	if (status != PJ_SUCCESS) {
	    return status;
	}

	pj_stun_msg_add_attr(response, &err_attr->hdr);
    }

    *p_response = response;
    return PJ_SUCCESS;
}


/*
 * Parse incoming packet into STUN message.
 */
PJ_DEF(pj_status_t) pj_stun_msg_decode(pj_pool_t *pool,
				       const pj_uint8_t *pdu,
				       unsigned pdu_len,
				       unsigned options,
				       pj_stun_msg **p_msg,
				       unsigned *p_parsed_len,
				       pj_stun_msg **p_response)
{
    
    pj_stun_msg *msg;
    unsigned uattr_cnt;
    const pj_uint8_t *start_pdu = pdu;
    pj_status_t status;

    PJ_UNUSED_ARG(options);

    PJ_ASSERT_RETURN(pool && pdu && pdu_len && p_msg, PJ_EINVAL);
    PJ_ASSERT_RETURN(sizeof(pj_stun_msg_hdr) == 20, PJ_EBUG);

    if (p_parsed_len)
	*p_parsed_len = 0;
    if (p_response)
	*p_response = NULL;

    /* Check if this is a STUN message, if necessary */
    if (options & PJ_STUN_CHECK_PACKET) {
	status = pj_stun_msg_check(pdu, pdu_len, options);
	if (status != PJ_SUCCESS)
	    return status;
    }

    /* Create the message, copy the header, and convert to host byte order */
    msg = PJ_POOL_ZALLOC_TYPE(pool, pj_stun_msg);
    pj_memcpy(&msg->hdr, pdu, sizeof(pj_stun_msg_hdr));
    msg->hdr.type = pj_ntohs(msg->hdr.type);
    msg->hdr.length = pj_ntohs(msg->hdr.length);
    msg->hdr.magic = pj_ntohl(msg->hdr.magic);

    pdu += sizeof(pj_stun_msg_hdr);
    pdu_len -= sizeof(pj_stun_msg_hdr);

    /* No need to create response if this is not a request */
    if (!PJ_STUN_IS_REQUEST(msg->hdr.type))
	p_response = NULL;

    /* Parse attributes */
    uattr_cnt = 0;
    while (pdu_len > 0) {
	unsigned attr_type, attr_val_len;
	const struct attr_desc *adesc;

	/* Get attribute type and length. If length is not aligned
	 * to 4 bytes boundary, add padding.
	 */
	attr_type = pj_ntohs(*(pj_uint16_t*)pdu);
	attr_val_len = pj_ntohs(*(pj_uint16_t*)(pdu+2));
	attr_val_len = (attr_val_len + 3) & (~3);

	/* Check length */
	if (pdu_len < attr_val_len) {
	    pj_str_t err_msg;
	    char err_msg_buf[80];

	    err_msg.ptr = err_msg_buf;
	    err_msg.slen = pj_ansi_snprintf(err_msg_buf, sizeof(err_msg_buf),
					    "Attribute %s has invalid length",
					    pj_stun_get_attr_name(attr_type));

	    PJ_LOG(4,(THIS_FILE, "Error decoding message: %.*s",
		      (int)err_msg.slen, err_msg.ptr));

	    if (p_response) {
		pj_stun_msg_create_response(pool, msg, 
					    PJ_STUN_STATUS_BAD_REQUEST, 
					    &err_msg, p_response);
	    }
	    return PJLIB_UTIL_ESTUNINATTRLEN;
	}

	/* Get the attribute descriptor */
	adesc = find_attr_desc(attr_type);

	if (adesc == NULL) {
	    /* Unrecognized attribute */

	    PJ_LOG(4,(THIS_FILE, "Unrecognized attribute type %d", 
		      attr_type));

	    /* Is this a fatal condition? */
	    if (attr_type <= 0x7FFF) {
		/* This is a mandatory attribute, we must return error
		 * if we don't understand the attribute.
		 */
		if (p_response) {
		    unsigned err_code = PJ_STUN_STATUS_UNKNOWN_ATTRIBUTE;

		    status = pj_stun_msg_create_response(pool, msg,
							 err_code, NULL, 
							 p_response);
		    if (status==PJ_SUCCESS) {
			pj_uint16_t d = (pj_uint16_t)attr_type;
			pj_stun_msg_add_unknown_attr(pool, *p_response, 1, &d);
		    }
		}

		return PJLIB_UTIL_ESTUNUNKNOWNATTR;
	    }

	} else {
	    void *attr;
	    char err_msg1[PJ_ERR_MSG_SIZE],
		 err_msg2[PJ_ERR_MSG_SIZE];

	    /* Parse the attribute */
	    status = (adesc->decode_attr)(pool, pdu, &attr);

	    if (status != PJ_SUCCESS) {
		pj_strerror(status, err_msg1, sizeof(err_msg1));

		if (p_response) {
		    pj_str_t e;

		    e.ptr = err_msg2;
		    e.slen= pj_ansi_snprintf(err_msg2, sizeof(err_msg2),
					     "%s in %s",
					     err_msg1,
					     pj_stun_get_attr_name(attr_type));

		    pj_stun_msg_create_response(pool, msg,
						PJ_STUN_STATUS_BAD_REQUEST,
						&e, p_response);
		}

		PJ_LOG(4,(THIS_FILE, 
			  "Error parsing STUN attribute %s: %s",
			  pj_stun_get_attr_name(attr_type), 
			  err_msg1));

		return status;
	    }
	    
	    /* Make sure we have rooms for the new attribute */
	    if (msg->attr_count >= PJ_STUN_MAX_ATTR) {
		if (p_response) {
		    pj_str_t e;

		    e = pj_str("Too many attributes");
		    pj_stun_msg_create_response(pool, msg,
						PJ_STUN_STATUS_BAD_REQUEST,
						&e, p_response);
		}
		return PJLIB_UTIL_ESTUNTOOMANYATTR;
	    }

	    /* Add the attribute */
	    msg->attr[msg->attr_count++] = (pj_stun_attr_hdr*)attr;
	}

	pdu += (attr_val_len + 4);
	pdu_len -= (attr_val_len + 4);
    }

    *p_msg = msg;

    if (p_parsed_len)
	*p_parsed_len = (pdu - start_pdu);

    return PJ_SUCCESS;
}


/*
 * Print the message structure to a buffer.
 */
PJ_DEF(pj_status_t) pj_stun_msg_encode(const pj_stun_msg *msg,
				       pj_uint8_t *buf, unsigned buf_size,
				       unsigned options,
				       const pj_str_t *password,
				       unsigned *p_msg_len)
{
    pj_stun_msg_hdr *hdr;
    pj_uint8_t *start = buf;
    pj_stun_realm_attr *arealm = NULL;
    pj_stun_username_attr *auname = NULL;
    pj_stun_msg_integrity_attr *amsg_integrity = NULL;
    pj_stun_fingerprint_attr *afingerprint = NULL;
    unsigned printed;
    pj_status_t status;
    unsigned i, length;


    PJ_ASSERT_RETURN(msg && buf && buf_size, PJ_EINVAL);

    PJ_UNUSED_ARG(options);
    PJ_ASSERT_RETURN(options == 0, PJ_EINVAL);

    /* Copy the message header part and convert the header fields to
     * network byte order
     */
    if (buf_size < sizeof(pj_stun_msg_hdr))
	return PJ_ETOOSMALL;
    pj_memcpy(buf, &msg->hdr, sizeof(pj_stun_msg_hdr));
    hdr = (pj_stun_msg_hdr*) buf;
    hdr->magic = pj_htonl(hdr->magic);
    hdr->type = pj_htons(hdr->type);
    /* We'll fill in the length later */

    buf += sizeof(pj_stun_msg_hdr);
    buf_size -= sizeof(pj_stun_msg_hdr);

    /* Print each attribute */
    for (i=0; i<msg->attr_count; ++i) {
	const struct attr_desc *adesc;
	const pj_stun_attr_hdr *attr_hdr;

	attr_hdr = msg->attr[i];

	if (attr_hdr->type == PJ_STUN_ATTR_MESSAGE_INTEGRITY) {
	    pj_assert(amsg_integrity == NULL);
	    amsg_integrity = (pj_stun_msg_integrity_attr*) attr_hdr;

	    /* Stop when encountering MESSAGE-INTEGRITY */
	    break;

	} else if (attr_hdr->type == PJ_STUN_ATTR_USERNAME) {
	    pj_assert(auname == NULL);
	    auname = (pj_stun_username_attr*) attr_hdr;

	} else if (attr_hdr->type == PJ_STUN_ATTR_REALM) {
	    pj_assert(arealm == NULL);
	    arealm = (pj_stun_realm_attr*) attr_hdr;

	} else if (attr_hdr->type == PJ_STUN_ATTR_FINGERPRINT) {
	    afingerprint = (pj_stun_fingerprint_attr*) attr_hdr;
	    break;
	}

	adesc = find_attr_desc(attr_hdr->type);
	PJ_ASSERT_RETURN(adesc != NULL, PJ_EBUG);

	status = adesc->encode_attr(attr_hdr, buf, buf_size, &printed);
	if (status != PJ_SUCCESS)
	    return status;

	buf += printed;
	buf_size -= printed;
    }

    /* Calculate message integrity, if present */
    if (amsg_integrity != NULL) {

	pj_uint8_t md5_key_buf[16];
	pj_str_t key;

	/* MESSAGE-INTEGRITY must be the last attribute in the message, or
	 * the last attribute before FINGERPRINT.
	 */
	if (i < msg->attr_count-2) {
	    /* Should not happen for message generated by us */
	    pj_assert(PJ_FALSE);
	    return PJLIB_UTIL_ESTUNMSGINT;

	} else if (i == msg->attr_count-2)  {
	    if (msg->attr[i+1]->type != PJ_STUN_ATTR_FINGERPRINT) {
		/* Should not happen for message generated by us */
		pj_assert(PJ_FALSE);
		return PJLIB_UTIL_ESTUNMSGINT;
	    } else {
		afingerprint = (pj_stun_fingerprint_attr*) msg->attr[i+1];
	    }
	}

	/* Must have USERNAME attribute */
	if (auname == NULL) {
	    /* Should not happen for message generated by us */
	    pj_assert(PJ_FALSE);
	    return PJLIB_UTIL_ESTUNNOUSERNAME;
	}

	/* Password must be specified */
	PJ_ASSERT_RETURN(password, PJ_EINVAL);

	/* Get the key to sign the message */
	if (arealm == NULL ) {
	    /* For short term credential, the key is the password */
	    key = *password;

	} else {
	    /* The 16-byte key for MESSAGE-INTEGRITY HMAC is formed by taking
	     * the MD5 hash of the result of concatenating the following five
	     * fields: (1) The username, with any quotes and trailing nulls
	     * removed, (2) A single colon, (3) The realm, with any quotes and
	     * trailing nulls removed, (4) A single colon, and (5) The 
	     * password, with any trailing nulls removed.
	     */
	    pj_md5_context ctx;
	    pj_str_t s;

	    pj_md5_init(&ctx);

#define REMOVE_QUOTE(s)	if (s.slen && *s.ptr=='"') \
			    s.ptr++, s.slen--; \
			if (s.slen && s.ptr[s.slen-1]=='"') \
			    s.slen--;

	    /* Add username */
	    s = auname->value;
	    REMOVE_QUOTE(s);
	    pj_md5_update(&ctx, (pj_uint8_t*)s.ptr, s.slen);

	    /* Add single colon */
	    pj_md5_update(&ctx, (pj_uint8_t*)":", 1);

	    /* Add realm */
	    s = arealm->value;
	    REMOVE_QUOTE(s);
	    pj_md5_update(&ctx, (pj_uint8_t*)s.ptr, s.slen);

#undef REMOVE_QUOTE

	    /* Another colon */
	    pj_md5_update(&ctx, (pj_uint8_t*)":", 1);

	    /* Add password */
	    pj_md5_update(&ctx, (pj_uint8_t*)password->ptr, password->slen);

	    /* Done */
	    pj_md5_final(&ctx, md5_key_buf);
	    key.ptr = (char*) md5_key_buf;
	    key.slen = 16;
	}

	/* Calculate HMAC-SHA1 digest */
	pj_hmac_sha1((pj_uint8_t*)buf, buf-start, 
		     (pj_uint8_t*)key.ptr, key.slen,
		     amsg_integrity->hmac);

	/* Put this attribute in the message */
	status = encode_msg_integrity_attr(amsg_integrity, buf, buf_size, 
				           &printed);
	if (status != PJ_SUCCESS)
	    return status;

	buf += printed;
	buf_size -= printed;
    }

    /* Calculate FINGERPRINT if present */
    if (afingerprint != NULL) {
	afingerprint->value = pj_crc32_calc(start, buf-start);
	afingerprint->value ^= STUN_XOR_FINGERPRINT;

	/* Put this attribute in the message */
	status = encode_generic_uint_attr(afingerprint, buf, buf_size, 
				          &printed);
	if (status != PJ_SUCCESS)
	    return status;

	buf += printed;
	buf_size -= printed;
    }

    /* Update the message length in the header. 
     * Note that length is not including the 20 bytes header.
     */
    length = (pj_uint16_t)((buf - start) - 20);
    /* hdr->length = pj_htons(length); */
    *(buf+2) = (pj_uint8_t)((length >> 8) & 0x00FF);
    *(buf+3) = (pj_uint8_t)(length & 0x00FF);


    /* Done */
    if (p_msg_len)
	*p_msg_len = (buf - start);

    return PJ_SUCCESS;
}


/*
 * Find STUN attribute in the STUN message, starting from the specified
 * index.
 */
PJ_DEF(pj_stun_attr_hdr*) pj_stun_msg_find_attr( const pj_stun_msg *msg,
						 int attr_type,
						 unsigned index)
{
    PJ_ASSERT_RETURN(msg, NULL);

    for (; index < msg->attr_count; ++index) {
	if (msg->attr[index]->type == attr_type)
	    return (pj_stun_attr_hdr*) &msg->attr[index];
    }

    return NULL;
}


/* Verify credential */
PJ_DEF(pj_status_t) pj_stun_verify_credential( const pj_stun_msg *msg,
					       const pj_str_t *realm,
					       const pj_str_t *username,
					       const pj_str_t *password,
					       unsigned options,
					       pj_pool_t *pool,
					       pj_stun_msg **p_response)
{
    const pj_stun_msg_integrity_attr *amsgi;
    const pj_stun_username_attr *auser;
    const pj_stun_realm_attr *arealm;

    PJ_ASSERT_RETURN(msg && password, PJ_EINVAL);
    PJ_ASSERT_RETURN(options==0, PJ_EINVAL);
    PJ_UNUSED_ARG(options);

    if (p_response)
	*p_response = NULL;

    /* First check that MESSAGE-INTEGRITY is present */
    amsgi = (const pj_stun_msg_integrity_attr*)
	    pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_MESSAGE_INTEGRITY, 0);
    if (amsgi == NULL) {
	if (pool && p_response) {
	    pj_status_t rc;

	    rc = pj_stun_msg_create_response(pool, msg, 
					     PJ_STUN_STATUS_UNAUTHORIZED, 
					     NULL, p_response);
	    if (rc==PJ_SUCCESS && realm) {
		pj_stun_msg_add_generic_string_attr(pool, *p_response,
						    PJ_STUN_ATTR_REALM, 
						    realm);
	    }
	}
	return PJ_STATUS_FROM_STUN_CODE(PJ_STUN_STATUS_UNAUTHORIZED);
    }

    /* Next check that USERNAME is present */
    auser = (const pj_stun_username_attr*)
	    pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_USERNAME, 0);
    if (auser == NULL) {
	if (pool && p_response) {
	    pj_status_t rc;

	    rc = pj_stun_msg_create_response(pool, msg, 
					     PJ_STUN_STATUS_MISSING_USERNAME, 
					     NULL, p_response);
	    if (rc==PJ_SUCCESS && realm) {
		pj_stun_msg_add_generic_string_attr(pool, *p_response,
						    PJ_STUN_ATTR_REALM, 
						    realm);
	    }
	}
	return PJ_STATUS_FROM_STUN_CODE(PJ_STUN_STATUS_MISSING_USERNAME);
    }

    /* Check if username match */
    if (username && pj_stricmp(&auser->value, username) != 0) {
	/* Username mismatch */
	if (pool && p_response) {
	    pj_status_t rc;

	    rc = pj_stun_msg_create_response(pool, msg, 
					     PJ_STUN_STATUS_WRONG_USERNAME, 
					     NULL, p_response);
	    if (rc==PJ_SUCCESS && realm) {
		pj_stun_msg_add_generic_string_attr(pool, *p_response,
						    PJ_STUN_ATTR_REALM, 
						    realm);
	    }
	}
	return PJ_STATUS_FROM_STUN_CODE(PJ_STUN_STATUS_WRONG_USERNAME);
    }

    /* Next check that REALM is present */
    arealm = (const pj_stun_realm_attr*)
	     pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_REALM, 0);
    if (realm != NULL && arealm == NULL) {
	/* Long term credential is required */
	if (pool && p_response) {
	    pj_status_t rc;

	    rc = pj_stun_msg_create_response(pool, msg, 
					     PJ_STUN_STATUS_MISSING_REALM, 
					     NULL, p_response);
	    if (rc==PJ_SUCCESS) {
		pj_stun_msg_add_generic_string_attr(pool, *p_response,
						    PJ_STUN_ATTR_REALM, 
						    realm);
	    }
	}
	return PJ_STATUS_FROM_STUN_CODE(PJ_STUN_STATUS_MISSING_REALM);

    } else if (realm != NULL && arealm != NULL) {


    } else if (realm == NULL && arealm != NULL) {
	/* We want to use short term credential, but client uses long
	 * term credential. The draft doesn't mention anything about
	 * switching between long term and short term.
	 */
	PJ_TODO(SWITCHING_BETWEEN_SHORT_TERM_AND_LONG_TERM);
    }

    PJ_TODO(CONTINUE_IMPLEMENTATION);

    return PJ_SUCCESS;
}


