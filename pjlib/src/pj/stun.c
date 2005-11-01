/* $Header: /pjproject-0.3/pjlib/src/pj/stun.c 6     9/17/05 10:37a Bennylp $ */
/* $Log: /pjproject-0.3/pjlib/src/pj/stun.c $
 * 
 * 6     9/17/05 10:37a Bennylp
 * Major reorganization towards version 0.3.
 * 
 */
#include <pj/stun.h>
#include <pj/pool.h>
#include <pj/log.h>
#include <pj/sock.h>
#include <pj/os.h>

#define THIS_FILE   "stun"

PJ_DEF(pj_status_t) pj_stun_create_bind_req( pj_pool_t *pool, 
					     void **msg, pj_size_t *len,
					     pj_uint32_t id_hi, 
					     pj_uint32_t id_lo)
{
    pj_stun_msg_hdr *hdr;
    
    PJ_CHECK_STACK();

    PJ_LOG(5,(THIS_FILE, "pj_stun_create_bind_req"));

    hdr = pj_pool_calloc(pool, 1, sizeof(pj_stun_msg_hdr));
    if (!hdr) {
	PJ_LOG(5,(THIS_FILE, "Error allocating memory!"));
	return -1;
    }

    hdr->type = pj_htons(PJ_STUN_BINDING_REQUEST);
    hdr->tsx[2] = pj_htonl(id_hi);
    hdr->tsx[3] = pj_htonl(id_lo);
    *msg = hdr;
    *len = sizeof(pj_stun_msg_hdr);

    return 0;
}

PJ_DEF(pj_status_t) pj_stun_parse_msg( void *buf, pj_size_t len, 
				       pj_stun_msg *msg)
{
    pj_uint16_t msg_type, msg_len;
    char *p_attr;

    PJ_CHECK_STACK();

    PJ_LOG(5,(THIS_FILE, "pj_stun_parse_msg %p, len=%d", buf, len));

    msg->hdr = (pj_stun_msg_hdr*)buf;
    msg_type = pj_ntohs(msg->hdr->type);

    switch (msg_type) {
    case PJ_STUN_BINDING_REQUEST:
    case PJ_STUN_BINDING_RESPONSE:
    case PJ_STUN_BINDING_ERROR_RESPONSE:
    case PJ_STUN_SHARED_SECRET_REQUEST:
    case PJ_STUN_SHARED_SECRET_RESPONSE:
    case PJ_STUN_SHARED_SECRET_ERROR_RESPONSE:
	break;
    default:
	PJ_LOG(5,(THIS_FILE, "Error: unknown msg type %d", msg_type));
	return -1;
    }

    msg_len = pj_ntohs(msg->hdr->length);
    if (msg_len != len - sizeof(pj_stun_msg_hdr)) {
	PJ_LOG(5,(THIS_FILE, "Error: invalid msg_len %d (expecting %d)", 
			     msg_len, len - sizeof(pj_stun_msg_hdr)));
	return -1;
    }

    msg->attr_count = 0;
    p_attr = (char*)buf + sizeof(pj_stun_msg_hdr);

    while (msg_len > 0) {
	pj_stun_attr_hdr **attr = &msg->attr[msg->attr_count];
	pj_uint32_t len;

	*attr = (pj_stun_attr_hdr*)p_attr;
	len = pj_ntohs((pj_uint16_t) ((*attr)->length)) + sizeof(pj_stun_attr_hdr);

	if (msg_len < len) {
	    PJ_LOG(5,(THIS_FILE, "Error: length mismatch in attr %d", 
				 msg->attr_count));
	    return -1;
	}

	if (pj_ntohs((*attr)->type) > PJ_STUN_ATTR_REFLECTED_FORM) {
	    PJ_LOG(5,(THIS_FILE, "Error: invalid attr type %d in attr %d",
				 pj_ntohs((*attr)->type), msg->attr_count));
	    return -1;
	}

	msg_len = (pj_uint16_t)(msg_len - len);
	p_attr += len;
	++msg->attr_count;
    }

    return 0;
}

PJ_DEF(void*) pj_stun_msg_find_attr( pj_stun_msg *msg, pj_stun_attr_type t)
{
    int i;

    PJ_CHECK_STACK();

    for (i=0; i<msg->attr_count; ++i) {
	pj_stun_attr_hdr *attr = msg->attr[i];
	if (pj_ntohs(attr->type) == t)
	    return attr;
    }

    return 0;
}
