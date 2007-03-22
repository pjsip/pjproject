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
#include "test.h"

#define THIS_FILE   "ice.c"


struct ice_data
{
    const char	   *obj_name;
    pj_bool_t	    complete;
    pj_status_t	    err_code;
    unsigned	    rx_rtp_cnt;
    unsigned	    rx_rtcp_cnt;

    char	    rx_rtp_data[32];
    char	    rx_rtcp_data[32];
};

static pj_stun_config stun_cfg;

static void on_ice_complete(pj_icemt *icemt, 
			    pj_status_t status)
{
    struct ice_data *id = (struct ice_data*) icemt->user_data;
    id->complete = PJ_TRUE;
    id->err_code = status;
    PJ_LOG(3,(THIS_FILE, "    ICE %s complete %s", id->obj_name,
	      (status==PJ_SUCCESS ? "successfully" : "with failure")));
}


static void on_rx_rtp(pj_icemt *icemt,
		      void *pkt, pj_size_t size,
		      const pj_sockaddr_t *src_addr,
		      unsigned src_addr_len)
{
    struct ice_data *id = (struct ice_data*) icemt->user_data;

    id->rx_rtp_cnt++;
    pj_memcpy(id->rx_rtp_data, pkt, size);

    PJ_UNUSED_ARG(src_addr);
    PJ_UNUSED_ARG(src_addr_len);
}


static void on_rx_rtcp(pj_icemt *icemt,
		       void *pkt, pj_size_t size,
		       const pj_sockaddr_t *src_addr,
		       unsigned src_addr_len)
{
    struct ice_data *id = (struct ice_data*) icemt->user_data;

    id->rx_rtcp_cnt++;
    pj_memcpy(id->rx_rtcp_data, pkt, size);

    PJ_UNUSED_ARG(src_addr);
    PJ_UNUSED_ARG(src_addr_len);
}


static void handle_events(unsigned msec_timeout)
{
    pj_time_val delay;

    pj_timer_heap_poll(stun_cfg.timer_heap, NULL);

    delay.sec = 0;
    delay.msec = msec_timeout;
    pj_time_val_normalize(&delay);

    pj_ioqueue_poll(stun_cfg.ioqueue, &delay);
}


/* Basic create and destroy test */
static int ice_basic_create_destroy_test()
{
    pj_icemt *im;
    pj_ice *ice;
    pj_icemt_cb icemt_cb;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "...basic create/destroy"));

    pj_bzero(&icemt_cb, sizeof(icemt_cb));
    icemt_cb.on_ice_complete = &on_ice_complete;
    icemt_cb.on_rx_rtcp = &on_rx_rtp;
    icemt_cb.on_rx_rtcp = &on_rx_rtcp;

    status = pj_icemt_create(&stun_cfg, NULL, PJ_ICE_ROLE_CONTROLLING,
			     &icemt_cb, 0, PJ_FALSE, PJ_FALSE, NULL, &im);
    if (status != PJ_SUCCESS)
	return -10;

    ice = im->ice;

    pj_icemt_destroy(im);

    return 0;
}


static pj_status_t set_remote_list(pj_icemt *src, pj_icemt *dst)
{
    unsigned i, count;
    unsigned cand_id[PJ_ICE_MAX_CAND];
    pj_ice_cand cand[PJ_ICE_MAX_CAND];
    pj_status_t status;

    count = PJ_ARRAY_SIZE(cand_id);
    status = pj_ice_enum_cands(src->ice, &count, cand_id);
    if (status != PJ_SUCCESS)
	return status;

    for (i=0; i<count; ++i) {
	pj_ice_cand *p_cand;
	status = pj_ice_get_cand(src->ice, cand_id[i], &p_cand);
	if (status != PJ_SUCCESS)
	    return status;

	pj_memcpy(&cand[i], p_cand, sizeof(pj_ice_cand));
    }

    status = pj_ice_create_check_list(dst->ice, count, cand);
    return status;
}


/* Perform ICE test with the following parameters:
 *
 * - title:	The title of the test
 * - ocand_cnt,
 *   ocand	Additional candidates to be added to offerer
 * - acand_cnt,
 *   acand	Additional candidates to be added to answerer
 *
 * The additional candidates are invalid candidates, meaning they
 * won't be reachable by the agents. They are used to "confuse"
 * ICE processing.
 */
static int perform_ice_test(const char *title,
			    unsigned ocand_cnt,
			    const pj_ice_cand ocand[],
			    unsigned acand_cnt,
			    const pj_ice_cand acand[])
{
    pj_icemt *im1, *im2;
    pj_icemt_cb icemt_cb;
    struct ice_data *id1, *id2;
    pj_timestamp t_start, t_end;
    pj_ice_cand *rcand;
    unsigned i;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "...%s", title));

    pj_bzero(&icemt_cb, sizeof(icemt_cb));
    icemt_cb.on_ice_complete = &on_ice_complete;
    icemt_cb.on_rx_rtcp = &on_rx_rtp;
    icemt_cb.on_rx_rtcp = &on_rx_rtcp;

    /* Create first ICE */
    status = pj_icemt_create(&stun_cfg, "offerer", PJ_ICE_ROLE_CONTROLLING,
			     &icemt_cb, 0, PJ_FALSE, PJ_FALSE, NULL, &im1);
    if (status != PJ_SUCCESS)
	return -20;

    id1 = PJ_POOL_ZALLOC_T(im1->pool, struct ice_data);
    id1->obj_name = "offerer";
    im1->user_data = id1;

    /* Add additional candidates */
    for (i=0; i<ocand_cnt; ++i) {
	status = pj_ice_add_cand(im1->ice, 1, ocand[i].type, 65535,
				 &ocand[i].foundation, &ocand[i].addr,
				 &ocand[i].base_addr, &ocand[i].srv_addr,
				 sizeof(pj_sockaddr_in), NULL);
	if (status != PJ_SUCCESS)
	    return -22;
    }

    /* Create second ICE */
    status = pj_icemt_create(&stun_cfg, "answerer", PJ_ICE_ROLE_CONTROLLED,
			     &icemt_cb, 0, PJ_FALSE, PJ_FALSE, NULL, &im2);
    if (status != PJ_SUCCESS)
	return -25;

    id2 = PJ_POOL_ZALLOC_T(im2->pool, struct ice_data);
    id2->obj_name = "answerer";
    im2->user_data = id2;

    /* Add additional candidates */
    for (i=0; i<acand_cnt; ++i) {
	status = pj_ice_add_cand(im1->ice, 1, acand[i].type, 65535,
				 &acand[i].foundation, &acand[i].addr,
				 &acand[i].base_addr, &acand[i].srv_addr,
				 sizeof(pj_sockaddr_in), NULL);
	if (status != PJ_SUCCESS)
	    return -22;
    }

    /* Set credentials */
    {
	pj_str_t u1 = pj_str("offerer");
	pj_str_t p1 = pj_str("pass1");
	pj_str_t u2 = pj_str("answerer");
	pj_str_t p2 = pj_str("pass2");

	pj_ice_set_credentials(im1->ice, &u1, &p1, &u2, &p2);
	pj_ice_set_credentials(im2->ice, &u2, &p2, &u1, &p1);
    }

    /* Send offer to im2 */
    status = set_remote_list(im1, im2);
    if (status != PJ_SUCCESS)
	return -30;

    /* Send answer to im1 */
    status = set_remote_list(im2, im1);
    if (status != PJ_SUCCESS)
	return -35;

    /* Mark start time */
    pj_get_timestamp(&t_start);

    /* Both can start now */
    status = pj_ice_start_check(im1->ice);
    if (status != PJ_SUCCESS)
	return -40;

#if 1
    status = pj_ice_start_check(im2->ice);
    if (status != PJ_SUCCESS)
	return -45;
#endif

    /* Just wait until both completes, or timed out */
    while (!id1->complete || !id2->complete) {
	pj_timestamp t_now;

	handle_events(1);

	pj_get_timestamp(&t_now);
	if (pj_elapsed_msec(&t_start, &t_now) >= 10000) {
	    PJ_LOG(3,(THIS_FILE, "....error: timed-out"));
	    return -50;
	}
    }

    /* Mark end-time */
    pj_get_timestamp(&t_end);

    /* Check status */
    if (id1->err_code != PJ_SUCCESS)
	return -53;
    if (id2->err_code != PJ_SUCCESS)
	return -56;

    /* Verify that offerer gets answerer's transport address */
    rcand = im1->ice->clist.checks[im1->ice->comp[0].nominated_check_id].rcand;
    if (pj_memcmp(&rcand->addr, &im2->ice->lcand[0].addr, sizeof(pj_sockaddr_in))!=0) {
	PJ_LOG(3,(THIS_FILE, "....error: address mismatch"));
	return -60;
    }

    /* And the other way around */
    rcand = im2->ice->clist.checks[im2->ice->comp[0].nominated_check_id].rcand;
    if (pj_memcmp(&rcand->addr, &im1->ice->lcand[0].addr, sizeof(pj_sockaddr_in))!=0) {
	PJ_LOG(3,(THIS_FILE, "....error: address mismatch"));
	return -70;
    }

    /* Done */
    PJ_LOG(3,(THIS_FILE, "....success: ICE completed in %d msec", 
	      pj_elapsed_msec(&t_start, &t_end)));

    /* Wait for some more time */
    PJ_LOG(3,(THIS_FILE, ".....waiting.."));
    for (;;) {
	pj_timestamp t_now;

	pj_get_timestamp(&t_now);
	if (pj_elapsed_msec(&t_end, &t_now) > 10000)
	    break;

	handle_events(1);
    }


    pj_icemt_destroy(im1);
    pj_icemt_destroy(im2);
    return 0;
}


int ice_test(void)
{
    int rc = 0;
    pj_pool_t *pool;
    pj_ioqueue_t *ioqueue;
    pj_timer_heap_t *timer_heap;
    pj_ice_cand ocand[PJ_ICE_MAX_CAND];
    pj_ice_cand acand[PJ_ICE_MAX_CAND];
    pj_str_t s;

    pool = pj_pool_create(mem, NULL, 4000, 4000, NULL);
    pj_ioqueue_create(pool, 12, &ioqueue);
    pj_timer_heap_create(pool, 100, &timer_heap);
    
    pj_stun_config_init(&stun_cfg, mem, 0, ioqueue, timer_heap);

    pj_log_set_level(5);

    /* Basic create/destroy */
    rc = ice_basic_create_destroy_test();
    if (rc != 0)
	goto on_return;

    /* Direct communication */
    rc = perform_ice_test("Direct connection", 0, NULL, 0, NULL);
    if (rc != 0)
	goto on_return;

    /* Direct communication with invalid address */
    pj_bzero(ocand, sizeof(ocand));
    pj_sockaddr_in_init(&ocand[0].addr.ipv4, pj_cstr(&s, "127.0.0.127"), 1234);
    pj_sockaddr_in_init(&ocand[0].base_addr.ipv4, pj_cstr(&s, "127.0.0.128"), 1234);
    ocand[0].comp_id = 1;
    ocand[0].foundation = pj_str("H2");
    ocand[0].type = PJ_ICE_CAND_TYPE_HOST;

    rc = perform_ice_test("Direct connection with 1 invalid address", 1, ocand, 0, NULL);
    if (rc != 0)
	goto on_return;


on_return:
    pj_log_set_level(3);
    pj_ioqueue_destroy(stun_cfg.ioqueue);
    pj_pool_release(pool);
    return rc;
}

