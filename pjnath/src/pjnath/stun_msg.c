/* $Id$ */
/* 
 * Copyright (C) 2003-2007 Benny Prijono <benny@prijono.org>
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
#include <pjnath/stun_msg.h>
#include <pjnath/errno.h>
#include <pjlib-util/crc32.h>
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

static int padding_char;

static const char *stun_method_names[PJ_STUN_METHOD_MAX] = 
{
    "Unknown",			/* 0 */
    "Binding",			/* 1 */
    "SharedSecret",		/* 2 */
    "Allocate",			/* 3 */
    "Refresh",			/* 4 */
    "???",			/* 5 */
    "Send",			/* 6 */
    "Data",			/* 7 */
    "???",			/* 8 */
    "ChannelBind",		/* 9 */
};

static struct
{
    int err_code;
    const char *err_msg;
} stun_err_msg_map[] = 
{
    { PJ_STUN_SC_TRY_ALTERNATE,		    "Try Alternate"}, 
    { PJ_STUN_SC_BAD_REQUEST,		    "Bad Request"},
    { PJ_STUN_SC_UNAUTHORIZED,		    "Unauthorized"},
    { PJ_STUN_SC_UNKNOWN_ATTRIBUTE,	    "Unknown Attribute"},
    //{ PJ_STUN_SC_STALE_CREDENTIALS,	    "Stale Credentials"},
    //{ PJ_STUN_SC_INTEGRITY_CHECK_FAILURE, "Integrity Check Failure"},
    //{ PJ_STUN_SC_MISSING_USERNAME,	    "Missing Username"},
    //{ PJ_STUN_SC_USE_TLS,		    "Use TLS"},
    //{ PJ_STUN_SC_MISSING_REALM,	    "Missing Realm"},
    //{ PJ_STUN_SC_MISSING_NONCE,	    "Missing Nonce"},
    //{ PJ_STUN_SC_UNKNOWN_USERNAME,	    "Unknown Username"},
    { PJ_STUN_SC_ALLOCATION_MISMATCH,	    "Allocation Mismatch"},
    { PJ_STUN_SC_STALE_NONCE,		    "Stale Nonce"},
    { PJ_STUN_SC_TRANSITIONING,		    "Active Destination Already Set"},
    { PJ_STUN_SC_UNSUPP_TRANSPORT_PROTO,    "Unsupported Transport Protocol"},
    { PJ_STUN_SC_INVALID_IP_ADDR,	    "Invalid IP Address"},
    { PJ_STUN_SC_INVALID_PORT,		    "Invalid Port"},
    { PJ_STUN_SC_OPER_TCP_ONLY,		    "Operation for TCP Only"},
    { PJ_STUN_SC_CONNECTION_FAILURE,	    "Connection Failure"},
    { PJ_STUN_SC_CONNECTION_TIMEOUT,	    "Connection Timeout"},
    { PJ_STUN_SC_ALLOCATION_QUOTA_REACHED,  "Allocation Quota Reached"},
    { PJ_STUN_SC_ROLE_CONFLICT,		    "Role Conflict"},
    { PJ_STUN_SC_SERVER_ERROR,		    "Server Error"},
    { PJ_STUN_SC_INSUFFICIENT_CAPACITY,	    "Insufficient Capacity"},
    { PJ_STUN_SC_GLOBAL_FAILURE,	    "Global Failure"}
};



struct attr_desc
{
    const char   *name;
    pj_status_t	(*decode_attr)(pj_pool_t *pool, const pj_uint8_t *buf, 
			       void **p_attr);
    pj_status_t (*encode_attr)(const void *a, pj_uint8_t *buf, 
			       unsigned len, unsigned *printed);
};

static pj_status_t decode_sockaddr_attr(pj_pool_t *pool, 
				       const pj_uint8_t *buf, 
				       void **p_attr);
static pj_status_t decode_xored_sockaddr_attr(pj_pool_t *pool, 
					      const pj_uint8_t *buf, 
					      void **p_attr);
static pj_status_t encode_sockaddr_attr(const void *a, pj_uint8_t *buf, 
				       unsigned len, 
				       unsigned *printed);
static pj_status_t decode_string_attr(pj_pool_t *pool, 
				      const pj_uint8_t *buf, 
				      void **p_attr);
static pj_status_t encode_string_attr(const void *a, pj_uint8_t *buf, 
				      unsigned len, unsigned *printed);
static pj_status_t decode_msgint_attr(pj_pool_t *pool, 
				      const pj_uint8_t *buf,
				      void **p_attr);
static pj_status_t encode_msgint_attr(const void *a, pj_uint8_t *buf, 
				      unsigned len, unsigned *printed);
static pj_status_t decode_errcode_attr(pj_pool_t *pool, 
				       const pj_uint8_t *buf,
				       void **p_attr);
static pj_status_t encode_errcode_attr(const void *a, pj_uint8_t *buf, 
				       unsigned len, unsigned *printed);
static pj_status_t decode_unknown_attr(pj_pool_t *pool, 
				       const pj_uint8_t *buf, 
				       void **p_attr);
static pj_status_t encode_unknown_attr(const void *a, pj_uint8_t *buf, 
				       unsigned len, unsigned *printed);
static pj_status_t decode_uint_attr(pj_pool_t *pool, 
				    const pj_uint8_t *buf, 
				    void **p_attr);
static pj_status_t encode_uint_attr(const void *a, pj_uint8_t *buf, 
				    unsigned len, unsigned *printed);
static pj_status_t decode_uint64_attr(pj_pool_t *pool, 
				      const pj_uint8_t *buf, 
				      void **p_attr);
static pj_status_t encode_uint64_attr(const void *a, pj_uint8_t *buf, 
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


static struct attr_desc mandatory_attr_desc[] = 
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
	&decode_sockaddr_attr,
	&encode_sockaddr_attr
    },
    {
	/* PJ_STUN_ATTR_RESPONSE_ADDR, */
	"RESPONSE-ADDRESS",
	&decode_sockaddr_attr,
	&encode_sockaddr_attr
    },
    {
	/* PJ_STUN_ATTR_CHANGE_REQUEST, */
	"CHANGE-REQUEST",
	&decode_uint_attr,
	&encode_uint_attr
    },
    {
	/* PJ_STUN_ATTR_SOURCE_ADDR, */
	"SOURCE-ADDRESS",
	&decode_sockaddr_attr,
	&encode_sockaddr_attr
    },
    {
	/* PJ_STUN_ATTR_CHANGED_ADDR, */
	"CHANGED-ADDRESS",
	&decode_sockaddr_attr,
	&encode_sockaddr_attr
    },
    {
	/* PJ_STUN_ATTR_USERNAME, */
	"USERNAME",
	&decode_string_attr,
	&encode_string_attr
    },
    {
	/* PJ_STUN_ATTR_PASSWORD, */
	"PASSWORD",
	&decode_string_attr,
	&encode_string_attr
    },
    {
	/* PJ_STUN_ATTR_MESSAGE_INTEGRITY, */
	"MESSAGE-INTEGRITY",
	&decode_msgint_attr,
	&encode_msgint_attr
    },
    {
	/* PJ_STUN_ATTR_ERROR_CODE, */
	"ERROR-CODE",
	&decode_errcode_attr,
	&encode_errcode_attr
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
	&decode_sockaddr_attr,
	&encode_sockaddr_attr
    },
    {
	/* PJ_STUN_ATTR_CHANNEL_NUMBER (0x000C) */
	"CHANNEL-NUMBER",
	&decode_uint_attr,
	&encode_uint_attr
    },
    {
	/* PJ_STUN_ATTR_LIFETIME, */
	"LIFETIME",
	&decode_uint_attr,
	&encode_uint_attr
    },
    {
	/* ID 0x000E is not assigned */
	NULL,
	NULL,
	NULL
    },
    {
	/* PJ_STUN_ATTR_MAGIC_COOKIE */
	"MAGIC-COOKIE",
	&decode_uint_attr,
	&encode_uint_attr
    },
    {
	/* PJ_STUN_ATTR_BANDWIDTH, */
	"BANDWIDTH",
	&decode_uint_attr,
	&encode_uint_attr
    },
    {
	/* ID 0x0011 is not assigned */
	NULL,
	NULL,
	NULL
    },
    {
	/* PJ_STUN_ATTR_PEER_ADDRESS, */
	"PEER-ADDRESS",
	&decode_xored_sockaddr_attr,
	&encode_sockaddr_attr
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
	&decode_string_attr,
	&encode_string_attr
    },
    {
	/* PJ_STUN_ATTR_NONCE, */
	"NONCE",
	&decode_string_attr,
	&encode_string_attr
    },
    {
	/* PJ_STUN_ATTR_RELAY_ADDRESS, */
	"RELAY-ADDRESS",
	&decode_xored_sockaddr_attr,
	&encode_sockaddr_attr
    },
    {
	/* PJ_STUN_ATTR_REQUESTED_ADDR_TYPE, */
	"REQUESTED-ADDRESS-TYPE",
	&decode_uint_attr,
	&encode_uint_attr
    },
    {
	/* PJ_STUN_ATTR_REQUESTED_PORT_PROPS, */
	"REQUESTED-PORT-PROPS",
	&decode_uint_attr,
	&encode_uint_attr
    },
    {
	/* PJ_STUN_ATTR_REQUESTED_TRANSPORT, */
	"REQUESTED-TRANSPORT",
	&decode_uint_attr,
	&encode_uint_attr
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
	&decode_xored_sockaddr_attr,
	&encode_sockaddr_attr
    },
    {
	/* PJ_STUN_ATTR_TIMER_VAL, */
	"TIMER-VAL",
	&decode_uint_attr,
	&encode_uint_attr
    },
    {
	/* PJ_STUN_ATTR_REQUESTED_IP, */
	"REQUESTED-IP",
	&decode_xored_sockaddr_attr,
	&encode_sockaddr_attr
    },
    {
	/* PJ_STUN_ATTR_XOR_REFLECTED_FROM, */
	"XOR-REFLECTED-FROM",
	&decode_xored_sockaddr_attr,
	&encode_sockaddr_attr
    },
    {
	/* PJ_STUN_ATTR_PRIORITY, */
	"PRIORITY",
	&decode_uint_attr,
	&encode_uint_attr
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
	&decode_xored_sockaddr_attr,
	&encode_sockaddr_attr
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
	/* ID 0x8021 is not assigned */
	NULL,
	NULL,
	NULL
    },
    {
	/* PJ_STUN_ATTR_SERVER, */
	"SERVER",
	&decode_string_attr,
	&encode_string_attr
    },
    {
	/* PJ_STUN_ATTR_ALTERNATE_SERVER, */
	"ALTERNATE-SERVER",
	&decode_sockaddr_attr,
	&encode_sockaddr_attr
    },
    {
	/* PJ_STUN_ATTR_REFRESH_INTERVAL, */
	"REFRESH-INTERVAL",
	&decode_uint_attr,
	&encode_uint_attr
    },
    {
	/* ID 0x8025 is not assigned*/
	NULL,
	NULL,
	NULL
    },
    {
	/* PADDING, 0x8026 */
	NULL,
	NULL,
	NULL
    },
    {
	/* CACHE-TIMEOUT, 0x8027 */
	NULL,
	NULL,
	NULL
    },
    {
	/* PJ_STUN_ATTR_FINGERPRINT, */
	"FINGERPRINT",
	&decode_uint_attr,
	&encode_uint_attr
    },
    {
	/* PJ_STUN_ATTR_ICE_CONTROLLED, */
	"ICE-CONTROLLED",
	&decode_uint64_attr,
	&encode_uint64_attr
    },
    {
	/* PJ_STUN_ATTR_ICE_CONTROLLING, */
	"ICE-CONTROLLING",
	&decode_uint64_attr,
	&encode_uint64_attr
    }
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
    else if (PJ_STUN_IS_SUCCESS_RESPONSE(msg_type))
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

    if (attr_type < PJ_STUN_ATTR_END_MANDATORY_ATTR)
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
#if 0
    /* Find error using linear search */
    unsigned i;

    for (i=0; i<PJ_ARRAY_SIZE(stun_err_msg_map); ++i) {
	if (stun_err_msg_map[i].err_code == err_code)
	    return pj_str((char*)stun_err_msg_map[i].err_msg);
    }
    return pj_str(NULL);
#else
    /* Find error message using binary search */
    int first = 0;
    int n = PJ_ARRAY_SIZE(stun_err_msg_map);

    while (n > 0) {
	int half = n/2;
	int mid = first + half;

	if (stun_err_msg_map[mid].err_code < err_code) {
	    first = mid+1;
	    n -= (half+1);
	} else if (stun_err_msg_map[mid].err_code > err_code) {
	    n = half;
	} else {
	    first = mid;
	    break;
	}
    }


    if (stun_err_msg_map[first].err_code == err_code) {
	return pj_str((char*)stun_err_msg_map[first].err_msg);
    } else {
	return pj_str(NULL);
    }
#endif
}


/*
 * Set padding character.
 */
PJ_DEF(int) pj_stun_set_padding_char(int chr)
{
    int old_pad = padding_char;
    padding_char = chr;
    return old_pad;
}


//////////////////////////////////////////////////////////////////////////////


#define INIT_ATTR(a,t,l)    (a)->hdr.type=(pj_uint16_t)(t), \
			    (a)->hdr.length=(pj_uint16_t)(l)
#define ATTR_HDR_LEN	    4

static pj_uint16_t GETVAL16H(const pj_uint8_t *buf, unsigned pos)
{
    return (pj_uint16_t) ((buf[pos + 0] << 8) | \
			  (buf[pos + 1] << 0));
}

static pj_uint16_t GETVAL16N(const pj_uint8_t *buf, unsigned pos)
{
    return pj_htons(GETVAL16H(buf,pos));
}

static void PUTVAL16H(pj_uint8_t *buf, unsigned pos, pj_uint16_t hval)
{
    buf[pos+0] = (pj_uint8_t) ((hval & 0xFF00) >> 8);
    buf[pos+1] = (pj_uint8_t) ((hval & 0x00FF) >> 0);
}

static pj_uint32_t GETVAL32H(const pj_uint8_t *buf, unsigned pos)
{
    return (pj_uint32_t) ((buf[pos + 0] << 24UL) | \
	                  (buf[pos + 1] << 16UL) | \
	                  (buf[pos + 2] <<  8UL) | \
			  (buf[pos + 3] <<  0UL));
}

static pj_uint32_t GETVAL32N(const pj_uint8_t *buf, unsigned pos)
{
    return pj_htonl(GETVAL32H(buf,pos));
}

static void PUTVAL32H(pj_uint8_t *buf, unsigned pos, pj_uint32_t hval)
{
    buf[pos+0] = (pj_uint8_t) ((hval & 0xFF000000UL) >> 24);
    buf[pos+1] = (pj_uint8_t) ((hval & 0x00FF0000UL) >> 16);
    buf[pos+2] = (pj_uint8_t) ((hval & 0x0000FF00UL) >>  8);
    buf[pos+3] = (pj_uint8_t) ((hval & 0x000000FFUL) >>  0);
}

static void GETVAL64H(const pj_uint8_t *buf, unsigned pos, pj_timestamp *ts)
{
    ts->u32.hi = GETVAL32H(buf, pos);
    ts->u32.lo = GETVAL32H(buf, pos+4);
}

static void PUTVAL64H(pj_uint8_t *buf, unsigned pos, const pj_timestamp *ts)
{
    PUTVAL32H(buf, pos, ts->u32.hi);
    PUTVAL32H(buf, pos+4, ts->u32.lo);
}


static void GETATTRHDR(const pj_uint8_t *buf, pj_stun_attr_hdr *hdr)
{
    hdr->type = GETVAL16H(buf, 0);
    hdr->length = GETVAL16H(buf, 2);
}

//////////////////////////////////////////////////////////////////////////////
/*
 * STUN generic IP address container
 */
#define STUN_GENERIC_IP_ADDR_LEN	8

/*
 * Create a generic STUN IP address attribute for IPv4 address.
 */
PJ_DEF(pj_status_t) pj_stun_sockaddr_attr_create(pj_pool_t *pool,
						 int attr_type,
						 pj_bool_t xor_ed,
						 const pj_sockaddr_t *addr,
						 unsigned addr_len,
						 pj_stun_sockaddr_attr **p_attr)
{
    pj_stun_sockaddr_attr *attr;

    PJ_ASSERT_RETURN(pool && addr_len && addr && p_attr, PJ_EINVAL);
    PJ_ASSERT_RETURN(addr_len == sizeof(pj_sockaddr_in) ||
		     addr_len == sizeof(pj_sockaddr_in6), PJ_EINVAL);

    attr = PJ_POOL_ZALLOC_T(pool, pj_stun_sockaddr_attr);
    INIT_ATTR(attr, attr_type, STUN_GENERIC_IP_ADDR_LEN);

    pj_memcpy(&attr->sockaddr, addr, addr_len);
    attr->xor_ed = xor_ed;

    *p_attr = attr;

    return PJ_SUCCESS;
}


/*
 * Create and add generic STUN IP address attribute to a STUN message.
 */
PJ_DEF(pj_status_t) pj_stun_msg_add_sockaddr_attr(pj_pool_t *pool,
						  pj_stun_msg *msg,
						  int attr_type, 
						  pj_bool_t xor_ed,
						  const pj_sockaddr_t *addr,
						  unsigned addr_len)
{
    pj_stun_sockaddr_attr *attr;
    pj_status_t status;

    status = pj_stun_sockaddr_attr_create(pool, attr_type, xor_ed,
					         addr, addr_len, &attr);
    if (status != PJ_SUCCESS)
	return status;

    return pj_stun_msg_add_attr(msg, &attr->hdr);
}

static pj_status_t decode_sockaddr_attr(pj_pool_t *pool, 
				        const pj_uint8_t *buf, 
				        void **p_attr)
{
    pj_stun_sockaddr_attr *attr;
    pj_uint32_t val;

    PJ_CHECK_STACK();
    
    /* Create the attribute */
    attr = PJ_POOL_ZALLOC_T(pool, pj_stun_sockaddr_attr);
    GETATTRHDR(buf, &attr->hdr);

    /* Check that the attribute length is valid */
    if (attr->hdr.length != STUN_GENERIC_IP_ADDR_LEN)
	return PJNATH_ESTUNINATTRLEN;

    /* Check address family */
    val = *(pj_uint8_t*)(buf + ATTR_HDR_LEN + 1);

    /* Check address family is valid (only supports ipv4 for now) */
    if (val != 1)
	return PJNATH_ESTUNIPV6NOTSUPP;

    /* Get port and address */
    pj_sockaddr_in_init(&attr->sockaddr.ipv4, NULL, 0);
    attr->sockaddr.ipv4.sin_port = GETVAL16N(buf, ATTR_HDR_LEN+2);
    attr->sockaddr.ipv4.sin_addr.s_addr = GETVAL32N(buf, ATTR_HDR_LEN+4);

    /* Done */
    *p_attr = (void*)attr;

    return PJ_SUCCESS;
}


static pj_status_t decode_xored_sockaddr_attr(pj_pool_t *pool, 
					      const pj_uint8_t *buf, 
					      void **p_attr)
{
    pj_stun_sockaddr_attr *attr;
    pj_status_t status;

    status = decode_sockaddr_attr(pool, buf, p_attr);
    if (status != PJ_SUCCESS)
	return status;

    attr = *(pj_stun_sockaddr_attr**)p_attr;

    attr->xor_ed = PJ_TRUE;
    attr->sockaddr.ipv4.sin_port ^= pj_htons(0x2112);
    attr->sockaddr.ipv4.sin_addr.s_addr ^= pj_htonl(0x2112A442);

    /* Done */
    *p_attr = attr;

    return PJ_SUCCESS;
}


static pj_status_t encode_sockaddr_attr(const void *a, pj_uint8_t *buf, 
				        unsigned len, unsigned *printed)
{
    enum {
	ATTR_LEN = ATTR_HDR_LEN + STUN_GENERIC_IP_ADDR_LEN
    };
    pj_uint8_t *start_buf = buf;
    const pj_stun_sockaddr_attr *ca = 
	(const pj_stun_sockaddr_attr *)a;

    if (len < ATTR_LEN) 
	return PJ_ETOOSMALL;

    PJ_CHECK_STACK();
    
    /* Copy and convert headers to network byte order */
    PUTVAL16H(buf, 0, ca->hdr.type);
    PUTVAL16H(buf, 2, STUN_GENERIC_IP_ADDR_LEN);
    buf += ATTR_HDR_LEN;
    
    /* Ignored */
    *buf++ = '\0';

    /* Family (IPv4 only for now) */
    PJ_ASSERT_RETURN(ca->sockaddr.addr.sa_family == pj_AF_INET(), PJ_EINVAL);
    *buf++ = 1;

    if (ca->xor_ed) {
	pj_uint32_t addr;
	pj_uint16_t port;

	addr = ca->sockaddr.ipv4.sin_addr.s_addr;
	port = ca->sockaddr.ipv4.sin_port;

	port ^= pj_htons(0x2112);
	addr ^= pj_htonl(0x2112A442);

	/* Port */
	pj_memcpy(buf, &port, 2);
	buf += 2;

	/* Address */
	pj_memcpy(buf, &addr, 4);
	buf += 4;

    } else {
	/* Port */
	pj_memcpy(buf, &ca->sockaddr.ipv4.sin_port, 2);
	buf += 2;

	/* Address */
	pj_memcpy(buf, &ca->sockaddr.ipv4.sin_addr, 4);
	buf += 4;
    }

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
PJ_DEF(pj_status_t) pj_stun_string_attr_create(pj_pool_t *pool,
					       int attr_type,
					       const pj_str_t *value,
					       pj_stun_string_attr **p_attr)
{
    pj_stun_string_attr *attr;

    PJ_ASSERT_RETURN(pool && value && p_attr, PJ_EINVAL);

    attr = PJ_POOL_ZALLOC_T(pool, pj_stun_string_attr);
    INIT_ATTR(attr, attr_type, value->slen);
    pj_strdup(pool, &attr->value, value);

    *p_attr = attr;

    return PJ_SUCCESS;
}


/*
 * Create and add STUN generic string attribute to the message.
 */
PJ_DEF(pj_status_t) pj_stun_msg_add_string_attr(pj_pool_t *pool,
						pj_stun_msg *msg,
						int attr_type,
						const pj_str_t *value)
{
    pj_stun_string_attr *attr = NULL;
    pj_status_t status;

    status = pj_stun_string_attr_create(pool, attr_type, value, 
						&attr);
    if (status != PJ_SUCCESS)
	return status;

    return pj_stun_msg_add_attr(msg, &attr->hdr);
}


static pj_status_t decode_string_attr(pj_pool_t *pool, 
				      const pj_uint8_t *buf, 
				      void **p_attr)
{
    pj_stun_string_attr *attr;
    pj_str_t value;

    /* Create the attribute */
    attr = PJ_POOL_ZALLOC_T(pool, pj_stun_string_attr);
    GETATTRHDR(buf, &attr->hdr);

    /* Get pointer to the string in the message */
    value.ptr = ((char*)buf + ATTR_HDR_LEN);
    value.slen = attr->hdr.length;

    /* Copy the string to the attribute */
    pj_strdup(pool, &attr->value, &value);

    /* Done */
    *p_attr = attr;

    return PJ_SUCCESS;

}


static pj_status_t encode_string_attr(const void *a, pj_uint8_t *buf, 
				      unsigned len, unsigned *printed)
{
    const pj_stun_string_attr *ca = 
	(const pj_stun_string_attr*)a;

    PJ_CHECK_STACK();
    
    /* Calculated total attr_len (add padding if necessary) */
    *printed = (ca->value.slen + ATTR_HDR_LEN + 3) & (~3);
    if (len < *printed) {
	*printed = 0;
	return PJ_ETOOSMALL;
    }

    PUTVAL16H(buf, 0, ca->hdr.type);
    PUTVAL16H(buf, 2, (pj_uint16_t)ca->value.slen);
    

    /* Copy the string */
    pj_memcpy(buf+ATTR_HDR_LEN, ca->value.ptr, ca->value.slen);

    /* Add padding character, if string is not 4-bytes aligned. */
    if (ca->value.slen & 0x03) {
	pj_uint8_t pad[3];
	pj_memset(pad, padding_char, sizeof(pad));
	pj_memcpy(buf+ATTR_HDR_LEN+ca->value.slen, pad,
		  4-(ca->value.slen & 0x03));
    }

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
PJ_DEF(pj_status_t) pj_stun_empty_attr_create(pj_pool_t *pool,
					      int attr_type,
					      pj_stun_empty_attr **p_attr)
{
    pj_stun_empty_attr *attr;

    PJ_ASSERT_RETURN(pool && p_attr, PJ_EINVAL);

    attr = PJ_POOL_ZALLOC_T(pool, pj_stun_empty_attr);
    INIT_ATTR(attr, attr_type, 0);

    *p_attr = attr;

    return PJ_SUCCESS;
}


/*
 * Create STUN empty attribute and add the attribute to the message.
 */
PJ_DEF(pj_status_t) pj_stun_msg_add_empty_attr( pj_pool_t *pool,
						pj_stun_msg *msg,
						int attr_type)
{
    pj_stun_empty_attr *attr = NULL;
    pj_status_t status;

    status = pj_stun_empty_attr_create(pool, attr_type, &attr);
    if (status != PJ_SUCCESS)
	return status;

    return pj_stun_msg_add_attr(msg, &attr->hdr);
}

static pj_status_t decode_empty_attr(pj_pool_t *pool, 
				     const pj_uint8_t *buf, 
				     void **p_attr)
{
    pj_stun_empty_attr *attr;

    /* Check that the struct address is valid */
    pj_assert(sizeof(pj_stun_empty_attr) == ATTR_HDR_LEN);

    /* Create the attribute */
    attr = PJ_POOL_ZALLOC_T(pool, pj_stun_empty_attr);
    GETATTRHDR(buf, &attr->hdr);

    /* Check that the attribute length is valid */
    if (attr->hdr.length != 0)
	return PJNATH_ESTUNINATTRLEN;

    /* Done */
    *p_attr = attr;

    return PJ_SUCCESS;
}


static pj_status_t encode_empty_attr(const void *a, pj_uint8_t *buf, 
				     unsigned len, unsigned *printed)
{
    const pj_stun_empty_attr *ca = (pj_stun_empty_attr*)a;

    if (len < ATTR_HDR_LEN) 
	return PJ_ETOOSMALL;

    PUTVAL16H(buf, 0, ca->hdr.type);
    PUTVAL16H(buf, 2, 0);

    /* Done */
    *printed = ATTR_HDR_LEN;

    return PJ_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////////
/*
 * STUN generic 32bit integer attribute.
 */

/*
 * Create a STUN generic 32bit value attribute.
 */
PJ_DEF(pj_status_t) pj_stun_uint_attr_create(pj_pool_t *pool,
					     int attr_type,
					     pj_uint32_t value,
					     pj_stun_uint_attr **p_attr)
{
    pj_stun_uint_attr *attr;

    PJ_ASSERT_RETURN(pool && p_attr, PJ_EINVAL);

    attr = PJ_POOL_ZALLOC_T(pool, pj_stun_uint_attr);
    INIT_ATTR(attr, attr_type, 4);
    attr->value = value;

    *p_attr = attr;

    return PJ_SUCCESS;
}

/* Create and add STUN generic 32bit value attribute to the message. */
PJ_DEF(pj_status_t) pj_stun_msg_add_uint_attr(pj_pool_t *pool,
					      pj_stun_msg *msg,
					      int attr_type,
					      pj_uint32_t value)
{
    pj_stun_uint_attr *attr = NULL;
    pj_status_t status;

    status = pj_stun_uint_attr_create(pool, attr_type, value, &attr);
    if (status != PJ_SUCCESS)
	return status;

    return pj_stun_msg_add_attr(msg, &attr->hdr);
}

static pj_status_t decode_uint_attr(pj_pool_t *pool, 
				    const pj_uint8_t *buf, 
				    void **p_attr)
{
    pj_stun_uint_attr *attr;

    /* Create the attribute */
    attr = PJ_POOL_ZALLOC_T(pool, pj_stun_uint_attr);
    GETATTRHDR(buf, &attr->hdr);

    attr->value = GETVAL32H(buf, 4);

    /* Check that the attribute length is valid */
    if (attr->hdr.length != 4)
	return PJNATH_ESTUNINATTRLEN;

    /* Done */
    *p_attr = attr;

    return PJ_SUCCESS;
}


static pj_status_t encode_uint_attr(const void *a, pj_uint8_t *buf, 
				    unsigned len, unsigned *printed)
{
    const pj_stun_uint_attr *ca = (const pj_stun_uint_attr*)a;

    PJ_CHECK_STACK();
    
    if (len < 8) 
	return PJ_ETOOSMALL;

    PUTVAL16H(buf, 0, ca->hdr.type);
    PUTVAL16H(buf, 2, (pj_uint16_t)4);
    PUTVAL32H(buf, 4, ca->value);
    
    /* Done */
    *printed = 8;

    return PJ_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////

/*
 * Create a STUN generic 64bit value attribute.
 */
PJ_DEF(pj_status_t) pj_stun_uint64_attr_create(pj_pool_t *pool,
					       int attr_type,
					       const pj_timestamp *value,
					       pj_stun_uint64_attr **p_attr)
{
    pj_stun_uint64_attr *attr;

    PJ_ASSERT_RETURN(pool && p_attr, PJ_EINVAL);

    attr = PJ_POOL_ZALLOC_T(pool, pj_stun_uint64_attr);
    INIT_ATTR(attr, attr_type, 8);

    if (value) {
	attr->value.u32.hi = value->u32.hi;
	attr->value.u32.lo = value->u32.lo;
    }

    *p_attr = attr;

    return PJ_SUCCESS;
}

/* Create and add STUN generic 64bit value attribute to the message. */
PJ_DEF(pj_status_t)  pj_stun_msg_add_uint64_attr(pj_pool_t *pool,
					         pj_stun_msg *msg,
					         int attr_type,
					         const pj_timestamp *value)
{
    pj_stun_uint64_attr *attr = NULL;
    pj_status_t status;

    status = pj_stun_uint64_attr_create(pool, attr_type, value, &attr);
    if (status != PJ_SUCCESS)
	return status;

    return pj_stun_msg_add_attr(msg, &attr->hdr);
}

static pj_status_t decode_uint64_attr(pj_pool_t *pool, 
				      const pj_uint8_t *buf, 
				      void **p_attr)
{
    pj_stun_uint64_attr *attr;

    /* Create the attribute */
    attr = PJ_POOL_ZALLOC_T(pool, pj_stun_uint64_attr);
    GETATTRHDR(buf, &attr->hdr);

    if (attr->hdr.length != 8)
	return PJNATH_ESTUNINATTRLEN;

    GETVAL64H(buf, 4, &attr->value);	

    /* Done */
    *p_attr = attr;

    return PJ_SUCCESS;
}


static pj_status_t encode_uint64_attr(const void *a, pj_uint8_t *buf, 
				      unsigned len, unsigned *printed)
{
    const pj_stun_uint64_attr *ca = (const pj_stun_uint64_attr*)a;

    PJ_CHECK_STACK();
    
    if (len < 12) 
	return PJ_ETOOSMALL;

    PUTVAL16H(buf, 0, ca->hdr.type);
    PUTVAL16H(buf, 2, (pj_uint16_t)8);
    PUTVAL64H(buf, 4, &ca->value);

    /* Done */
    *printed = 12;

    return PJ_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////////
/*
 * STUN MESSAGE-INTEGRITY attribute.
 */

/*
 * Create a STUN MESSAGE-INTEGRITY attribute.
 */
PJ_DEF(pj_status_t) pj_stun_msgint_attr_create(pj_pool_t *pool,
					       pj_stun_msgint_attr **p_attr)
{
    pj_stun_msgint_attr *attr;

    PJ_ASSERT_RETURN(pool && p_attr, PJ_EINVAL);

    attr = PJ_POOL_ZALLOC_T(pool, pj_stun_msgint_attr);
    INIT_ATTR(attr, PJ_STUN_ATTR_MESSAGE_INTEGRITY, 20);

    *p_attr = attr;

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_stun_msg_add_msgint_attr(pj_pool_t *pool,
						pj_stun_msg *msg)
{
    pj_stun_msgint_attr *attr = NULL;
    pj_status_t status;

    status = pj_stun_msgint_attr_create(pool, &attr);
    if (status != PJ_SUCCESS)
	return status;

    return pj_stun_msg_add_attr(msg, &attr->hdr);
}

static pj_status_t decode_msgint_attr(pj_pool_t *pool, 
				      const pj_uint8_t *buf,
				      void **p_attr)
{
    pj_stun_msgint_attr *attr;

    /* Create attribute */
    attr = PJ_POOL_ZALLOC_T(pool, pj_stun_msgint_attr);
    GETATTRHDR(buf, &attr->hdr);

    /* Check that the attribute length is valid */
    if (attr->hdr.length != 20)
	return PJNATH_ESTUNINATTRLEN;

    /* Copy hmac */
    pj_memcpy(attr->hmac, buf+4, 20);

    /* Done */
    *p_attr = attr;
    return PJ_SUCCESS;
}


static pj_status_t encode_msgint_attr(const void *a, pj_uint8_t *buf, 
				      unsigned len, unsigned *printed)
{
    const pj_stun_msgint_attr *ca = (const pj_stun_msgint_attr*)a;

    PJ_CHECK_STACK();
    
    if (len < 24) 
	return PJ_ETOOSMALL;

    /* Copy and convert attribute to network byte order */
    PUTVAL16H(buf, 0, ca->hdr.type);
    PUTVAL16H(buf, 2, ca->hdr.length);

    pj_memcpy(buf+4, ca->hmac, 20);

    /* Done */
    *printed = 24;

    return PJ_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
/*
 * STUN ERROR-CODE
 */

/*
 * Create a STUN ERROR-CODE attribute.
 */
PJ_DEF(pj_status_t) pj_stun_errcode_attr_create(pj_pool_t *pool,
						int err_code,
						const pj_str_t *err_reason,
						pj_stun_errcode_attr **p_attr)
{
    pj_stun_errcode_attr *attr;
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

    attr = PJ_POOL_ZALLOC_T(pool, pj_stun_errcode_attr);
    INIT_ATTR(attr, PJ_STUN_ATTR_ERROR_CODE, 4+err_reason->slen);
    attr->err_code = err_code;
    pj_strdup(pool, &attr->reason, err_reason);

    *p_attr = attr;

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_stun_msg_add_errcode_attr(pj_pool_t *pool,
						 pj_stun_msg *msg,
						 int err_code,
						 const pj_str_t *err_reason)
{
    pj_stun_errcode_attr *err_attr = NULL;
    pj_status_t status;

    status = pj_stun_errcode_attr_create(pool, err_code, err_reason,
					 &err_attr);
    if (status != PJ_SUCCESS)
	return status;

    return pj_stun_msg_add_attr(msg, &err_attr->hdr);
}

static pj_status_t decode_errcode_attr(pj_pool_t *pool, 
				       const pj_uint8_t *buf,
				       void **p_attr)
{
    pj_stun_errcode_attr *attr;
    pj_str_t value;

    /* Create the attribute */
    attr = PJ_POOL_ZALLOC_T(pool, pj_stun_errcode_attr);
    GETATTRHDR(buf, &attr->hdr);

    attr->err_code = buf[6] * 100 + buf[7];

    /* Get pointer to the string in the message */
    value.ptr = ((char*)buf + ATTR_HDR_LEN + 4);
    value.slen = attr->hdr.length - 4;

    /* Copy the string to the attribute */
    pj_strdup(pool, &attr->reason, &value);

    /* Done */
    *p_attr = attr;

    return PJ_SUCCESS;
}


static pj_status_t encode_errcode_attr(const void *a, pj_uint8_t *buf, 
				       unsigned len, unsigned *printed)
{
    const pj_stun_errcode_attr *ca = 
	(const pj_stun_errcode_attr*)a;

    PJ_CHECK_STACK();
    
    if (len < ATTR_HDR_LEN + 4 + (unsigned)ca->reason.slen) 
	return PJ_ETOOSMALL;

    /* Copy and convert attribute to network byte order */
    PUTVAL16H(buf, 0, ca->hdr.type);
    PUTVAL16H(buf, 2, (pj_uint16_t)(4 + ca->reason.slen));
    PUTVAL16H(buf, 4, 0);
    buf[6] = (pj_uint8_t)(ca->err_code / 100);
    buf[7] = (pj_uint8_t)(ca->err_code % 100);

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
PJ_DEF(pj_status_t) pj_stun_unknown_attr_create(pj_pool_t *pool,
						unsigned attr_cnt,
						const pj_uint16_t attr_array[],
						pj_stun_unknown_attr **p_attr)
{
    pj_stun_unknown_attr *attr;
    unsigned i;

    PJ_ASSERT_RETURN(pool && attr_cnt < PJ_STUN_MAX_ATTR && p_attr, PJ_EINVAL);

    attr = PJ_POOL_ZALLOC_T(pool, pj_stun_unknown_attr);
    INIT_ATTR(attr, PJ_STUN_ATTR_UNKNOWN_ATTRIBUTES, attr_cnt * 2);

    attr->attr_count = attr_cnt;
    for (i=0; i<attr_cnt; ++i) {
	attr->attrs[i] = attr_array[i];
    }

    /* If the number of unknown attributes is an odd number, one of the
     * attributes MUST be repeated in the list.
     */
    /* No longer necessary
    if ((attr_cnt & 0x01)) {
	attr->attrs[attr_cnt] = attr_array[attr_cnt-1];
    }
    */

    *p_attr = attr;

    return PJ_SUCCESS;
}


/* Create and add STUN UNKNOWN-ATTRIBUTES attribute to the message. */
PJ_DEF(pj_status_t) pj_stun_msg_add_unknown_attr(pj_pool_t *pool,
						 pj_stun_msg *msg,
						 unsigned attr_cnt,
						 const pj_uint16_t attr_types[])
{
    pj_stun_unknown_attr *attr = NULL;
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

    attr = PJ_POOL_ZALLOC_T(pool, pj_stun_unknown_attr);
    GETATTRHDR(buf, &attr->hdr);
 
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
    pj_uint16_t *dst_unk_attr;
    unsigned i;

    PJ_CHECK_STACK();
    
    /* Check that buffer is enough */
    if (len < ATTR_HDR_LEN + (ca->attr_count << 1))
	return PJ_ETOOSMALL;

    PUTVAL16H(buf, 0, ca->hdr.type);
    PUTVAL16H(buf, 2, (pj_uint16_t)(ca->attr_count << 1));

    /* Copy individual attribute */
    dst_unk_attr = (pj_uint16_t*)(buf + ATTR_HDR_LEN);
    for (i=0; i < ca->attr_count; ++i, ++dst_unk_attr) {
	*dst_unk_attr = pj_htons(ca->attrs[i]);
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
PJ_DEF(pj_status_t) pj_stun_binary_attr_create(pj_pool_t *pool,
					       int attr_type,
					       const pj_uint8_t *data,
					       unsigned length,
					       pj_stun_binary_attr **p_attr)
{
    pj_stun_binary_attr *attr;

    PJ_ASSERT_RETURN(pool && attr_type && p_attr, PJ_EINVAL);

    attr = PJ_POOL_ZALLOC_T(pool, pj_stun_binary_attr);
    INIT_ATTR(attr, attr_type, length);

    if (data && length) {
	attr->length = length;
	attr->data = (pj_uint8_t*) pj_pool_alloc(pool, length);
	pj_memcpy(attr->data, data, length);
    }

    *p_attr = attr;

    return PJ_SUCCESS;
}


/* Create and add binary attr. */
PJ_DEF(pj_status_t) pj_stun_msg_add_binary_attr(pj_pool_t *pool,
						pj_stun_msg *msg,
						int attr_type,
						const pj_uint8_t *data,
						unsigned length)
{
    pj_stun_binary_attr *attr = NULL;
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
    attr = PJ_POOL_ZALLOC_T(pool, pj_stun_binary_attr);
    GETATTRHDR(buf, &attr->hdr);

    /* Copy the data to the attribute */
    attr->length = attr->hdr.length;
    attr->data = (pj_uint8_t*) pj_pool_alloc(pool, attr->length);
    pj_memcpy(attr->data, buf+ATTR_HDR_LEN, attr->length);

    /* Done */
    *p_attr = attr;

    return PJ_SUCCESS;

}


static pj_status_t encode_binary_attr(const void *a, pj_uint8_t *buf, 
				      unsigned len, unsigned *printed)
{
    const pj_stun_binary_attr *ca = (const pj_stun_binary_attr*)a;

    PJ_CHECK_STACK();
    
    /* Calculated total attr_len (add padding if necessary) */
    *printed = (ca->length + ATTR_HDR_LEN + 3) & (~3);
    if (len < *printed)
	return PJ_ETOOSMALL;

    PUTVAL16H(buf, 0, ca->hdr.type);
    PUTVAL16H(buf, 2, (pj_uint16_t) ca->length);

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

    msg = PJ_POOL_ZALLOC_T(pool, pj_stun_msg);
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


/*
 * Check that the PDU is potentially a valid STUN message.
 */
PJ_DEF(pj_status_t) pj_stun_msg_check(const pj_uint8_t *pdu, unsigned pdu_len,
				      unsigned options)
{
    unsigned msg_len;

    PJ_ASSERT_RETURN(pdu, PJ_EINVAL);

    if (pdu_len < sizeof(pj_stun_msg_hdr))
	return PJNATH_EINSTUNMSGLEN;

    /* First byte of STUN message is always 0x00 or 0x01. */
    if (*pdu != 0x00 && *pdu != 0x01)
	return PJNATH_EINSTUNMSGTYPE;

    /* Check the PDU length */
    msg_len = GETVAL16H(pdu, 2);
    if ((msg_len + 20 > pdu_len) || 
	((options & PJ_STUN_IS_DATAGRAM) && msg_len + 20 != pdu_len))
    {
	return PJNATH_EINSTUNMSGLEN;
    }

    /* STUN message is always padded to the nearest 4 bytes, thus
     * the last two bits of the length field are always zero.
     */
    if ((msg_len & 0x03) != 0) {
	return PJNATH_EINSTUNMSGLEN;
    }

    /* If magic is set, then there is great possibility that this is
     * a STUN message.
     */
    if (GETVAL32H(pdu, 4) == PJ_STUN_MAGIC) {

	/* Check if FINGERPRINT attribute is present */
	if (GETVAL16H(pdu, msg_len + 20 - 8) == PJ_STUN_ATTR_FINGERPRINT) {
	    pj_uint16_t attr_len = GETVAL16H(pdu, msg_len + 20 - 8 + 2);
	    pj_uint32_t fingerprint = GETVAL32H(pdu, msg_len + 20 - 8 + 4);
	    pj_uint32_t crc;

	    if (attr_len != 4)
		return PJNATH_ESTUNINATTRLEN;

	    crc = pj_crc32_calc(pdu, msg_len + 20 - 8);
	    crc ^= STUN_XOR_FINGERPRINT;

	    if (crc != fingerprint)
		return PJNATH_ESTUNFINGERPRINT;
	}
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
    pj_stun_msg *response = NULL;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && p_response, PJ_EINVAL);

    PJ_ASSERT_RETURN(PJ_STUN_IS_REQUEST(msg_type), 
		     PJNATH_EINSTUNMSGTYPE);

    /* Create response or error response */
    if (err_code)
	msg_type |= PJ_STUN_ERROR_RESPONSE_BIT;
    else
	msg_type |= PJ_STUN_SUCCESS_RESPONSE_BIT;

    status = pj_stun_msg_create(pool, msg_type, req_msg->hdr.magic, 
				req_msg->hdr.tsx_id, &response);
    if (status != PJ_SUCCESS) {
	return status;
    }

    /* Add error code attribute */
    if (err_code) {
	status = pj_stun_msg_add_errcode_attr(pool, response, 
					      err_code, err_msg);
	if (status != PJ_SUCCESS) {
	    return status;
	}
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
    pj_bool_t has_msg_int = PJ_FALSE;
    pj_bool_t has_fingerprint = PJ_FALSE;
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
    msg = PJ_POOL_ZALLOC_T(pool, pj_stun_msg);
    pj_memcpy(&msg->hdr, pdu, sizeof(pj_stun_msg_hdr));
    msg->hdr.type = pj_ntohs(msg->hdr.type);
    msg->hdr.length = pj_ntohs(msg->hdr.length);
    msg->hdr.magic = pj_ntohl(msg->hdr.magic);

    pdu += sizeof(pj_stun_msg_hdr);
    /* pdu_len -= sizeof(pj_stun_msg_hdr); */
    pdu_len = msg->hdr.length;

    /* No need to create response if this is not a request */
    if (!PJ_STUN_IS_REQUEST(msg->hdr.type))
	p_response = NULL;

    /* Parse attributes */
    uattr_cnt = 0;
    while (pdu_len >= 4) {
	unsigned attr_type, attr_val_len;
	const struct attr_desc *adesc;

	/* Get attribute type and length. If length is not aligned
	 * to 4 bytes boundary, add padding.
	 */
	attr_type = GETVAL16H(pdu, 0);
	attr_val_len = GETVAL16H(pdu, 2);
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
					    PJ_STUN_SC_BAD_REQUEST, 
					    &err_msg, p_response);
	    }
	    return PJNATH_ESTUNINATTRLEN;
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
		    unsigned err_code = PJ_STUN_SC_UNKNOWN_ATTRIBUTE;

		    status = pj_stun_msg_create_response(pool, msg,
							 err_code, NULL, 
							 p_response);
		    if (status==PJ_SUCCESS) {
			pj_uint16_t d = (pj_uint16_t)attr_type;
			pj_stun_msg_add_unknown_attr(pool, *p_response, 1, &d);
		    }
		}

		return PJ_STATUS_FROM_STUN_CODE(PJ_STUN_SC_UNKNOWN_ATTRIBUTE);
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
						PJ_STUN_SC_BAD_REQUEST,
						&e, p_response);
		}

		PJ_LOG(4,(THIS_FILE, 
			  "Error parsing STUN attribute %s: %s",
			  pj_stun_get_attr_name(attr_type), 
			  err_msg1));

		return status;
	    }

	    if (attr_type == PJ_STUN_ATTR_MESSAGE_INTEGRITY && 
		!has_fingerprint) 
	    {
		if (has_msg_int) {
		    /* Already has MESSAGE-INTEGRITY */
		    if (p_response) {
			pj_stun_msg_create_response(pool, msg,
						    PJ_STUN_SC_BAD_REQUEST,
						    NULL, p_response);
		    }
		    return PJNATH_ESTUNDUPATTR;
		}
		has_msg_int = PJ_TRUE;

	    } else if (attr_type == PJ_STUN_ATTR_FINGERPRINT) {
		if (has_fingerprint) {
		    /* Already has FINGERPRINT */
		    if (p_response) {
			pj_stun_msg_create_response(pool, msg,
						    PJ_STUN_SC_BAD_REQUEST,
						    NULL, p_response);
		    }
		    return PJNATH_ESTUNDUPATTR;
		}
		has_fingerprint = PJ_TRUE;
	    } else {
		if (has_fingerprint) {
		    /* Another attribute is found which is not FINGERPRINT
		     * after FINGERPRINT. Note that non-FINGERPRINT is
		     * allowed to appear after M-I
		     */
		    if (p_response) {
			pj_stun_msg_create_response(pool, msg,
						    PJ_STUN_SC_BAD_REQUEST,
						    NULL, p_response);
		    }
		    return PJNATH_ESTUNFINGERPOS;
		}
	    }

	    /* Make sure we have rooms for the new attribute */
	    if (msg->attr_count >= PJ_STUN_MAX_ATTR) {
		if (p_response) {
		    pj_stun_msg_create_response(pool, msg,
						PJ_STUN_SC_BAD_REQUEST,
						NULL, p_response);
		}
		return PJNATH_ESTUNTOOMANYATTR;
	    }

	    /* Add the attribute */
	    msg->attr[msg->attr_count++] = (pj_stun_attr_hdr*)attr;
	}

	if (attr_val_len + 4 >= pdu_len) {
	    pdu += pdu_len;
	    pdu_len = 0;
	} else {
	    pdu += (attr_val_len + 4);
	    pdu_len -= (attr_val_len + 4);
	}
    }

    if (pdu_len > 0) {
	/* Stray trailing bytes */
	PJ_LOG(4,(THIS_FILE, 
		  "Error decoding STUN message: unparsed trailing %d bytes",
		  pdu_len));
	return PJNATH_EINSTUNMSGLEN;
    }

    *p_msg = msg;

    if (p_parsed_len)
	*p_parsed_len = (pdu - start_pdu);

    return PJ_SUCCESS;
}

/* Calculate HMAC-SHA1 key for long term credential, by getting
 * MD5 digest of username, realm, and password. 
 */
static void calc_md5_key(pj_uint8_t digest[16],
			 const pj_str_t *realm,
			 const pj_str_t *username,
			 const pj_str_t *passwd)
{
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
    s = *username;
    REMOVE_QUOTE(s);
    pj_md5_update(&ctx, (pj_uint8_t*)s.ptr, s.slen);

    /* Add single colon */
    pj_md5_update(&ctx, (pj_uint8_t*)":", 1);

    /* Add realm */
    s = *realm;
    REMOVE_QUOTE(s);
    pj_md5_update(&ctx, (pj_uint8_t*)s.ptr, s.slen);

#undef REMOVE_QUOTE

    /* Another colon */
    pj_md5_update(&ctx, (pj_uint8_t*)":", 1);

    /* Add password */
    pj_md5_update(&ctx, (pj_uint8_t*)passwd->ptr, passwd->slen);

    /* Done */
    pj_md5_final(&ctx, digest);
}


/*
 * Create authentication key to be used for encoding the message with
 * MESSAGE-INTEGRITY. 
 */
PJ_DEF(void) pj_stun_create_key(pj_pool_t *pool,
				pj_str_t *key,
				const pj_str_t *realm,
				const pj_str_t *username,
				const pj_str_t *passwd)
{
    PJ_ASSERT_ON_FAIL(pool && key && username && passwd, return);

    if (realm && realm->slen) {
	key->ptr = (char*) pj_pool_alloc(pool, 16);
	calc_md5_key((pj_uint8_t*)key->ptr, realm, username, passwd);
	key->slen = 16;
    } else {
	pj_strdup(pool, key, passwd);
    }
}


/*
static char *print_binary(const pj_uint8_t *data, unsigned data_len)
{
    static char static_buffer[1024];
    char *buffer = static_buffer;
    unsigned length=sizeof(static_buffer), i;

    if (length < data_len * 2 + 8)
	return "";

    pj_ansi_sprintf(buffer, ", data=");
    buffer += 7;

    for (i=0; i<data_len; ++i) {
	pj_ansi_sprintf(buffer, "%02x", (*data) & 0xFF);
	buffer += 2;
	data++;
    }

    pj_ansi_sprintf(buffer, "\n");
    buffer++;

    return static_buffer;
}
*/

/*
 * Print the message structure to a buffer.
 */
PJ_DEF(pj_status_t) pj_stun_msg_encode(pj_stun_msg *msg,
				       pj_uint8_t *buf, unsigned buf_size,
				       unsigned options,
				       const pj_str_t *key,
				       unsigned *p_msg_len)
{
    pj_uint8_t *start = buf;
    pj_stun_msgint_attr *amsgint = NULL;
    pj_stun_fingerprint_attr *afingerprint = NULL;
    unsigned printed = 0, body_len;
    pj_status_t status;
    unsigned i;


    PJ_ASSERT_RETURN(msg && buf && buf_size, PJ_EINVAL);

    PJ_UNUSED_ARG(options);
    PJ_ASSERT_RETURN(options == 0, PJ_EINVAL);

    /* Copy the message header part and convert the header fields to
     * network byte order
     */
    if (buf_size < sizeof(pj_stun_msg_hdr))
	return PJ_ETOOSMALL;
    
    PUTVAL16H(buf, 0, msg->hdr.type);
    PUTVAL16H(buf, 2, 0);   /* length will be calculated later */
    PUTVAL32H(buf, 4, msg->hdr.magic);
    pj_memcpy(buf+8, msg->hdr.tsx_id, sizeof(msg->hdr.tsx_id));

    buf += sizeof(pj_stun_msg_hdr);
    buf_size -= sizeof(pj_stun_msg_hdr);

    /* Encode each attribute to the message */
    for (i=0; i<msg->attr_count; ++i) {
	const struct attr_desc *adesc;
	const pj_stun_attr_hdr *attr_hdr = msg->attr[i];

	if (attr_hdr->type == PJ_STUN_ATTR_MESSAGE_INTEGRITY) {
	    pj_assert(amsgint == NULL);
	    amsgint = (pj_stun_msgint_attr*) attr_hdr;

	    /* Stop when encountering MESSAGE-INTEGRITY */
	    break;

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

    /* We may have stopped printing attribute because we found
     * MESSAGE-INTEGRITY or FINGERPRINT. Scan the rest of the
     * attributes.
     */
    for ( ++i; i<msg->attr_count; ++i) {
	const pj_stun_attr_hdr *attr_hdr = msg->attr[i];

	/* There mustn't any attribute after FINGERPRINT */
	PJ_ASSERT_RETURN(afingerprint == NULL, PJNATH_ESTUNFINGERPOS);

	if (attr_hdr->type == PJ_STUN_ATTR_MESSAGE_INTEGRITY) {
	    /* There mustn't be MESSAGE-INTEGRITY before */
	    PJ_ASSERT_RETURN(amsgint == NULL, 
			     PJNATH_ESTUNMSGINTPOS);
	    amsgint = (pj_stun_msgint_attr*) attr_hdr;

	} else if (attr_hdr->type == PJ_STUN_ATTR_FINGERPRINT) {
	    afingerprint = (pj_stun_fingerprint_attr*) attr_hdr;
	}
    }

#if PJ_STUN_OLD_STYLE_MI_FINGERPRINT
    /*
     * This is the old style MESSAGE-INTEGRITY and FINGERPRINT
     * calculation, used in rfc3489bis-06 and older.
     */
    /* We MUST update the message length in the header NOW before
     * calculating MESSAGE-INTEGRITY and FINGERPRINT. 
     * Note that length is not including the 20 bytes header.
      */
    if (amsgint && afingerprint) {
	body_len = (pj_uint16_t)((buf - start) - 20 + 24 + 8);
    } else if (amsgint) {
	body_len = (pj_uint16_t)((buf - start) - 20 + 24);
    } else if (afingerprint) {
	body_len = (pj_uint16_t)((buf - start) - 20 + 8);
    } else {
	body_len = (pj_uint16_t)((buf - start) - 20);
    }
#else
    /* If MESSAGE-INTEGRITY is present, include the M-I attribute
     * in message length before calculating M-I
     */
    if (amsgint) {
	body_len = (pj_uint16_t)((buf - start) - 20 + 24);
    } else {
	body_len = (pj_uint16_t)((buf - start) - 20);
    }
#endif	/* PJ_STUN_OLD_STYLE_MI_FINGERPRINT */

    /* hdr->length = pj_htons(length); */
    PUTVAL16H(start, 2, (pj_uint16_t)body_len);

    /* Calculate message integrity, if present */
    if (amsgint != NULL) {
	pj_hmac_sha1_context ctx;

	/* Key MUST be specified */
	PJ_ASSERT_RETURN(key, PJ_EINVALIDOP);

	/* MESSAGE-INTEGRITY must be the last attribute in the message, or
	 * the last attribute before FINGERPRINT.
	 */
	if (i < msg->attr_count-2) {
	    /* Should not happen for message generated by us */
	    pj_assert(PJ_FALSE);
	    return PJNATH_ESTUNMSGINTPOS;

	} else if (i == msg->attr_count-2)  {
	    if (msg->attr[i+1]->type != PJ_STUN_ATTR_FINGERPRINT) {
		/* Should not happen for message generated by us */
		pj_assert(PJ_FALSE);
		return PJNATH_ESTUNMSGINTPOS;
	    } else {
		afingerprint = (pj_stun_fingerprint_attr*) msg->attr[i+1];
	    }
	}

	/* Calculate HMAC-SHA1 digest, add zero padding to input
	 * if necessary to make the input 64 bytes aligned.
	 */
	pj_hmac_sha1_init(&ctx, (pj_uint8_t*)key->ptr, key->slen);
	pj_hmac_sha1_update(&ctx, (pj_uint8_t*)start, buf-start);
#if PJ_STUN_OLD_STYLE_MI_FINGERPRINT
	// These are obsoleted in rfc3489bis-08
	if ((buf-start) & 0x3F) {
	    pj_uint8_t zeroes[64];
	    pj_bzero(zeroes, sizeof(zeroes));
	    pj_hmac_sha1_update(&ctx, zeroes, 64-((buf-start) & 0x3F));
	}
#endif	/* PJ_STUN_OLD_STYLE_MI_FINGERPRINT */
	pj_hmac_sha1_final(&ctx, amsgint->hmac);

	/* Put this attribute in the message */
	status = encode_msgint_attr(amsgint, buf, buf_size, 
			            &printed);
	if (status != PJ_SUCCESS)
	    return status;

	buf += printed;
	buf_size -= printed;
    }

    /* Calculate FINGERPRINT if present */
    if (afingerprint != NULL) {

#if !PJ_STUN_OLD_STYLE_MI_FINGERPRINT
	/* Update message length */
	PUTVAL16H(start, 2, 
		 (pj_uint16_t)(GETVAL16H(start, 2)+8));
#endif

	afingerprint->value = pj_crc32_calc(start, buf-start);
	afingerprint->value ^= STUN_XOR_FINGERPRINT;

	/* Put this attribute in the message */
	status = encode_uint_attr(afingerprint, buf, buf_size, 
				          &printed);
	if (status != PJ_SUCCESS)
	    return status;

	buf += printed;
	buf_size -= printed;
    }

    /* Update message length. */
    msg->hdr.length = (pj_uint16_t) ((buf - start) - 20);

    /* Return the length */
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
	    return (pj_stun_attr_hdr*) msg->attr[index];
    }

    return NULL;
}

