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

    unsigned	    rx_rtp_count;
    char	    last_rx_rtp_data[32];
    unsigned	    rx_rtcp_count;
    char	    last_rx_rtcp_data[32];
};

static pj_stun_config stun_cfg;

static void on_ice_complete(pj_ice_st *icest, 
			    pj_status_t status)
{
    struct ice_data *id = (struct ice_data*) icest->user_data;
    id->complete = PJ_TRUE;
    id->err_code = status;
    PJ_LOG(3,(THIS_FILE, "    ICE %s complete %s", id->obj_name,
	      (status==PJ_SUCCESS ? "successfully" : "with failure")));
}


static void on_rx_data(pj_ice_st *icest,
		       unsigned comp_id, unsigned cand_id,
		       void *pkt, pj_size_t size,
		       const pj_sockaddr_t *src_addr,
		       unsigned src_addr_len)
{
    struct ice_data *id = (struct ice_data*) icest->user_data;

    if (comp_id == 1) {
	id->rx_rtp_cnt++;
	pj_memcpy(id->last_rx_rtp_data, pkt, size);
	id->last_rx_rtp_data[size] = '\0';
    } else if (comp_id == 2) {
	id->rx_rtcp_cnt++;
	pj_memcpy(id->last_rx_rtcp_data, pkt, size);
	id->last_rx_rtcp_data[size] = '\0';
    } else {
	pj_assert(!"Invalid component ID");
    }

    PJ_UNUSED_ARG(cand_id);
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
    pj_ice_st *im;
    pj_ice_st_cb icest_cb;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "...basic create/destroy"));

    pj_bzero(&icest_cb, sizeof(icest_cb));
    icest_cb.on_ice_complete = &on_ice_complete;
    icest_cb.on_rx_data = &on_rx_data;

    status = pj_ice_st_create(&stun_cfg, NULL, NULL, &icest_cb, &im);
    if (status != PJ_SUCCESS)
	return -10;

    pj_ice_st_destroy(im);

    return 0;
}


static pj_status_t start_ice(pj_ice_st *ist, pj_ice_st *remote)
{
    unsigned count;
    pj_ice_cand cand[PJ_ICE_MAX_CAND];
    pj_status_t status;

    count = PJ_ARRAY_SIZE(cand);
    status = pj_ice_st_enum_cands(remote, &count, cand);
    if (status != PJ_SUCCESS)
	return status;

    return pj_ice_st_start_ice(ist, &remote->ice->rx_ufrag, &remote->ice->rx_pass,
			       count, cand);
}



/* Perform ICE test with the following parameters:
 *
 * - title:	The title of the test
 * - ocand_cnt,
 *   ocand	Additional candidates to be added to offerer
 * - acand_cnt,
 *   acand	Additional candidates to be added to answerer
 *
 * The additional candidates are normally invalid candidates, meaning 
 * they won't be reachable by the agents. They are used to "confuse"
 * ICE processing.
 */
static int perform_ice_test(const char *title,
			    unsigned wait_before_send,
			    unsigned max_total_time)
{
    pj_ice_st *im1, *im2;
    pj_ice_st_cb icest_cb;
    struct ice_data *id1, *id2;
    pj_timestamp t_start, t_end;
    pj_ice_cand *rcand;
    pj_str_t data_from_offerer, data_from_answerer;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "...%s", title));

    pj_bzero(&icest_cb, sizeof(icest_cb));
    icest_cb.on_ice_complete = &on_ice_complete;
    icest_cb.on_rx_data = &on_rx_data;

    /* Create first ICE */
    status = pj_ice_st_create(&stun_cfg, "offerer", NULL, &icest_cb, &im1);
    if (status != PJ_SUCCESS)
	return -20;

    id1 = PJ_POOL_ZALLOC_T(im1->pool, struct ice_data);
    id1->obj_name = "offerer";
    im1->user_data = id1;

    /* Add first component */
    status = pj_ice_st_add_comp(im1, 1);
    if (status != PJ_SUCCESS)
	return -21;

    /* Add host candidate */
    status = pj_ice_st_add_host_interface(im1, 1, 65535, NULL, NULL);
    if (status != PJ_SUCCESS)
	return -21;

    /* Create second ICE */
    status = pj_ice_st_create(&stun_cfg, "answerer", NULL, &icest_cb, &im2);
    if (status != PJ_SUCCESS)
	return -25;

    id2 = PJ_POOL_ZALLOC_T(im2->pool, struct ice_data);
    id2->obj_name = "answerer";
    im2->user_data = id2;

    /* Add first component */
    status = pj_ice_st_add_comp(im2, 1);
    if (status != PJ_SUCCESS)
	return -26;

    /* Add host candidate */
    status = pj_ice_st_add_host_interface(im2, 1, 65535, NULL, NULL);
    if (status != PJ_SUCCESS)
	return -27;

    /* Init ICE on im1 */
    status = pj_ice_st_init_ice(im1, PJ_ICE_ROLE_CONTROLLING, NULL, NULL);
    if (status != PJ_SUCCESS)
	return -29;

    /* Init ICE on im2 */
    status = pj_ice_st_init_ice(im2, PJ_ICE_ROLE_CONTROLLED, NULL, NULL);
    if (status != PJ_SUCCESS)
	return -29;

    /* Start ICE on im2 */
    status = start_ice(im2, im1);
    if (status != PJ_SUCCESS)
	return -30;

    /* Start ICE on im1 */
    status = start_ice(im1, im2);
    if (status != PJ_SUCCESS)
	return -35;

    /* Mark start time */
    pj_get_timestamp(&t_start);

    /* Poll for wait_before_send msecs before we send the first data */
    for (;;) {
	pj_timestamp t_now;

	handle_events(1);

	pj_get_timestamp(&t_now);
	if (pj_elapsed_msec(&t_start, &t_now) >= wait_before_send)
	    break;
    }

    /* Send data. It must be successful! */
    data_from_offerer = pj_str("from offerer");
    status = pj_ice_send_data(im1->ice, 1, data_from_offerer.ptr, data_from_offerer.slen);
    if (status != PJ_SUCCESS)
	return -47;

    data_from_answerer = pj_str("from answerer");
    status = pj_ice_send_data(im2->ice, 1, data_from_answerer.ptr, data_from_answerer.slen);
    if (status != PJ_SUCCESS)
	return -48;

    /* Poll to allow data to be received */
    for (;;) {
	pj_timestamp t_now;
	handle_events(1);
	pj_get_timestamp(&t_now);
	if (pj_elapsed_msec(&t_start, &t_now) >= (wait_before_send + 200))
	    break;
    }


    /* Just wait until both completes, or timed out */
    while (!id1->complete || !id2->complete) {
	pj_timestamp t_now;

	handle_events(1);

	pj_get_timestamp(&t_now);
	if (pj_elapsed_msec(&t_start, &t_now) >= max_total_time) {
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

    /* Check that data is received in offerer */
    if (id1->rx_rtp_cnt != 1) {
	PJ_LOG(3,(THIS_FILE, "....error: data not received in offerer"));
	return -80;
    }
    if (pj_strcmp2(&data_from_answerer, id1->last_rx_rtp_data) != 0) {
	PJ_LOG(3,(THIS_FILE, "....error: data mismatch in offerer"));
	return -82;
    }

    /* And the same in answerer */
    if (id2->rx_rtp_cnt != 1) {
	PJ_LOG(3,(THIS_FILE, "....error: data not received in answerer"));
	return -84;
    }
    if (pj_strcmp2(&data_from_offerer, id2->last_rx_rtp_data) != 0) {
	PJ_LOG(3,(THIS_FILE, "....error: data mismatch in answerer"));
	return -82;
    }


    /* Done */
    PJ_LOG(3,(THIS_FILE, "....success: ICE completed in %d msec", 
	      pj_elapsed_msec(&t_start, &t_end)));

    /* Wait for some more time */
    PJ_LOG(3,(THIS_FILE, ".....waiting.."));
    for (;;) {
	pj_timestamp t_now;

	pj_get_timestamp(&t_now);
	if (pj_elapsed_msec(&t_start, &t_now) > max_total_time)
	    break;

	handle_events(1);
    }


    pj_ice_st_destroy(im1);
    pj_ice_st_destroy(im2);
    return 0;
}


int ice_test(void)
{
    int rc = 0;
    pj_pool_t *pool;
    pj_ioqueue_t *ioqueue;
    pj_timer_heap_t *timer_heap;

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
    rc = perform_ice_test("Direct connection", 500, 1000);
    if (rc != 0)
	goto on_return;

    /* Direct communication with invalid address */
    rc = perform_ice_test("Direct connection with 1 invalid address", 500, 1000);
    if (rc != 0)
	goto on_return;

    /* Direct communication with two components */
    rc = perform_ice_test("Direct connection with two components", 500, 1000);
    if (rc != 0)
	goto on_return;



on_return:
    pj_log_set_level(3);
    pj_ioqueue_destroy(stun_cfg.ioqueue);
    pj_pool_release(pool);
    return rc;
}

