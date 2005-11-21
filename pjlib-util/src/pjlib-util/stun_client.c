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
#include <pjlib-util/stun.h>
#include <pj/pool.h>
#include <pj/log.h>
#include <pj/string.h>
#include <pj/os.h>
#include <pj/sock_select.h>

enum { MAX_REQUEST = 3 };
static int stun_timer[] = {1600, 1600, 1600 };

#define THIS_FILE	"stunclient"
#define LOG_ADDR(addr)	pj_inet_ntoa(addr.sin_addr), pj_ntohs(addr.sin_port)


PJ_DECL(pj_status_t) pj_stun_get_mapped_addr( pj_pool_factory *pf,
					      int sock_cnt, pj_sock_t sock[],
					      const pj_str_t *srv1, int port1,
					      const pj_str_t *srv2, int port2,
					      pj_sockaddr_in mapped_addr[])
{
    pj_sockaddr_in srv_addr[2];
    int i, j, rc, send_cnt = 0;
    pj_pool_t *pool;
    struct {
	struct {
	    pj_uint32_t	mapped_addr;
	    pj_uint32_t	mapped_port;
	} srv[2];
    } *rec;
    void       *out_msg;
    pj_size_t	out_msg_len;
    int wait_resp = 0;
    int mapped_status = 0;

    PJ_CHECK_STACK();

    /* Create pool. */
    pool = pj_pool_create(pf, "stun%p", 1024, 1024, NULL);
    if (!pool) {
	mapped_status = PJ_STUN_ERR_MEMORY; 
	return -1;
    }

    /* Allocate client records */
    rec = pj_pool_calloc(pool, sock_cnt, sizeof(*rec));
    if (!rec) {
	mapped_status = PJ_STUN_ERR_MEMORY; 
	goto on_error;
    }

    /* Create the outgoing BIND REQUEST message template */
    rc = pj_stun_create_bind_req( pool, &out_msg, &out_msg_len, 0, 0);
    if (rc != 0) {
	mapped_status = -1;
	goto on_error;
    }

    /* Resolve servers. */
    if (pj_sockaddr_in_init(&srv_addr[0], srv1, (pj_uint16_t)port1) != 0) {
	mapped_status = PJ_STUN_ERR_RESOLVE; 
	goto on_error;
    }
    if (pj_sockaddr_in_init(&srv_addr[1], srv2, (pj_uint16_t)port2) != 0) {
	mapped_status = PJ_STUN_ERR_RESOLVE;
	goto on_error;
    }

    /* Init mapped addresses to zero */
    pj_memset(mapped_addr, 0, sock_cnt * sizeof(pj_sockaddr_in));

    /* Main retransmission loop. */
    for (send_cnt=0; send_cnt<MAX_REQUEST; ++send_cnt) {
	pj_time_val next_tx, now;
	pj_fd_set_t r;
	int select_rc;

	PJ_LOG(4,(THIS_FILE, "STUN retransmit %d, wait_resp=%d", 
			     send_cnt, wait_resp));

	PJ_FD_ZERO(&r);

	/* Send messages to servers that has not given us response. */
	for (i=0; i<sock_cnt && mapped_status==0; ++i) {
	    for (j=0; j<2 && mapped_status==0; ++j) {
		pj_stun_msg_hdr *msg_hdr = out_msg;
                pj_ssize_t sent_len;

		if (rec[i].srv[j].mapped_port != 0)
		    continue;

		/* Modify message so that we can distinguish response. */
		msg_hdr->tsx[2] = pj_htonl(i);
		msg_hdr->tsx[3] = pj_htonl(j);

		/* Send! */
                sent_len = out_msg_len;
		rc = pj_sock_sendto(sock[i], out_msg, &sent_len, 0,
				    (pj_sockaddr_t*)&srv_addr[j], 
				    sizeof(pj_sockaddr_in));
		if (sent_len != (int)out_msg_len) {
		    PJ_LOG(4,(THIS_FILE, 
			      "Error sending STUN request to %s:%d",
			      LOG_ADDR(srv_addr[j])));
		    mapped_status = PJ_STUN_ERR_TRANSPORT; 
		} else {
		    ++wait_resp;
		}
	    }
	}

	/* All requests sent.
	 * The loop below will wait for responses until all responses have
	 * been received (i.e. wait_resp==0) or timeout occurs, which then
	 * we'll go to the next retransmission iteration.
	 */

	/* Calculate time of next retransmission. */
	pj_gettimeofday(&next_tx);
	next_tx.sec += (stun_timer[send_cnt]/1000);
	next_tx.msec += (stun_timer[send_cnt]%1000);
	pj_time_val_normalize(&next_tx);

	for (pj_gettimeofday(&now), select_rc=1; 
	     mapped_status==0 && select_rc==1 && wait_resp>0 && PJ_TIME_VAL_LT(now, next_tx); 
	     pj_gettimeofday(&now)) 
	{
	    pj_time_val timeout;

	    timeout = next_tx;
	    PJ_TIME_VAL_SUB(timeout, now);

	    for (i=0; i<sock_cnt; ++i) {
		PJ_FD_SET(sock[i], &r);
	    }

	    select_rc = pj_sock_select(FD_SETSIZE, &r, NULL, NULL, &timeout);
	    if (select_rc < 1)
		continue;

	    for (i=0; i<sock_cnt; ++i) {
		int sock_idx, srv_idx;
                pj_ssize_t len;
		pj_stun_msg msg;
		pj_sockaddr_in addr;
		int addrlen = sizeof(addr);
		pj_stun_mapped_addr_attr *attr;
		char recv_buf[128];

		if (!PJ_FD_ISSET(sock[i], &r))
		    continue;

                len = sizeof(recv_buf);
		pj_sock_recvfrom( sock[i], recv_buf, 
				  &len, 0,
				  (pj_sockaddr_t*)&addr,
				  &addrlen);

		--wait_resp;

		if (len < 1) {
		    mapped_status = PJ_STUN_ERR_TRANSPORT; 
		    continue;
		}

		if (pj_stun_parse_msg(recv_buf, len, &msg) != 0) {
		    PJ_LOG(4,(THIS_FILE, 
				"Error parsing STUN response from %s:%d",
				LOG_ADDR(addr)));
		    mapped_status = PJ_STUN_ERR_INVALID_MSG;
		    continue;
		}

		sock_idx = pj_ntohl(msg.hdr->tsx[2]);
		srv_idx = pj_ntohl(msg.hdr->tsx[3]);

		if (sock_idx<0 || sock_idx>=sock_cnt || srv_idx<0 || srv_idx>=2) {
		    PJ_LOG(4,(THIS_FILE, 
				"Invalid transaction ID from %s:%d", 
				LOG_ADDR(addr)));
		    mapped_status = PJ_STUN_ERR_INVALID_MSG;
		    continue;
		}

		if (pj_ntohs(msg.hdr->type) != PJ_STUN_BINDING_RESPONSE) {
		    PJ_LOG(4,(THIS_FILE, 
				"Non binding response %d from %s:%d", 
				pj_ntohs(msg.hdr->type), LOG_ADDR(addr)));
		    mapped_status = PJ_STUN_ERR_INVALID_MSG;
		    continue;
		}

		if (pj_stun_msg_find_attr(&msg, PJ_STUN_ATTR_ERROR_CODE) != NULL) {
		    PJ_LOG(4,(THIS_FILE, 
				"Got STUN error attribute from %s:%d",
				LOG_ADDR(addr)));
		    mapped_status = PJ_STUN_ERR_INVALID_MSG;
		    continue;
		}

		attr = (void*)pj_stun_msg_find_attr(&msg, PJ_STUN_ATTR_MAPPED_ADDR);
		if (!attr) {
		    PJ_LOG(4,(THIS_FILE,
				"No mapped address in response from %s:%d",
				LOG_ADDR(addr)));
		    mapped_status = PJ_STUN_ERR_INVALID_MSG;
		    continue;
		}

		rec[sock_idx].srv[srv_idx].mapped_addr = attr->addr;
		rec[sock_idx].srv[srv_idx].mapped_port = attr->port;
	    }
	}

	/* The best scenario is if all requests have been replied.
	 * Then we don't need to go to the next retransmission iteration.
	 */
	if (wait_resp <= 0)
	    break;
    }

    for (i=0; i<sock_cnt && mapped_status==0; ++i) {
	if (rec[i].srv[0].mapped_addr == rec[i].srv[1].mapped_addr &&
	    rec[i].srv[0].mapped_port == rec[i].srv[1].mapped_port)
	{
	    mapped_addr[i].sin_family = PJ_AF_INET;
	    mapped_addr[i].sin_addr.s_addr = rec[i].srv[0].mapped_addr;
	    mapped_addr[i].sin_port = (pj_uint16_t)rec[i].srv[0].mapped_port;

	    if (rec[i].srv[0].mapped_addr == 0 || rec[i].srv[0].mapped_port == 0) {
		mapped_status = PJ_STUN_ERR_NO_RESPONSE;
	    }
	} else {
	    mapped_status = PJ_STUN_ERR_SYMETRIC;
	}
    }

    pj_pool_release(pool);

    return mapped_status;

on_error:
    if (pool) pj_pool_release(pool);
    return -1;
}

PJ_DEF(const char*) pj_stun_get_err_msg(pj_status_t status)
{
    switch (status) {
    case 0:			    return "No error";
    case -1:			    return "General error";
    case PJ_STUN_ERR_MEMORY:	    return "Memory allocation failed";
    case PJ_STUN_ERR_RESOLVE:	    return "Invalid IP or unable to resolve STUN server";
    case PJ_STUN_ERR_TRANSPORT:	    return "Unable to contact STUN server";
    case PJ_STUN_ERR_INVALID_MSG:   return "Invalid response from STUN server";
    case PJ_STUN_ERR_NO_RESPONSE:   return "No response from STUN server";
    case PJ_STUN_ERR_SYMETRIC:	    return "Different mappings are returned from servers";
    }
    return "Unknown error";
}
