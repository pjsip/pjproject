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

#define THIS_FILE   "ice_test.c"


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

static void on_ice_complete(pj_ice_strans *icest, 
			    pj_status_t status)
{
    struct ice_data *id = (struct ice_data*) icest->user_data;
    id->complete = PJ_TRUE;
    id->err_code = status;
    PJ_LOG(3,(THIS_FILE, "     ICE %s complete %s", id->obj_name,
	      (status==PJ_SUCCESS ? "successfully" : "with failure")));
}


static void on_rx_data(pj_ice_strans *icest, unsigned comp_id, 
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
    pj_ice_strans *im;
    pj_ice_strans_cb icest_cb;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "...basic create/destroy"));

    pj_bzero(&icest_cb, sizeof(icest_cb));
    icest_cb.on_ice_complete = &on_ice_complete;
    icest_cb.on_rx_data = &on_rx_data;

    status = pj_ice_strans_create(&stun_cfg, "icetest", 2, NULL, &icest_cb, &im);
    if (status != PJ_SUCCESS)
	return -10;

    pj_ice_strans_destroy(im);

    return 0;
}


static pj_status_t start_ice(pj_ice_strans *ist, pj_ice_strans *remote)
{
    unsigned count;
    pj_ice_sess_cand cand[PJ_ICE_MAX_CAND];
    pj_status_t status;

    count = PJ_ARRAY_SIZE(cand);
    status = pj_ice_strans_enum_cands(remote, &count, cand);
    if (status != PJ_SUCCESS)
	return status;

    return pj_ice_strans_start_ice(ist, &remote->ice->rx_ufrag, &remote->ice->rx_pass,
			       count, cand);
}


struct dummy_cand
{
    unsigned		 comp_id;
    pj_ice_cand_type	 type;
    const char		*addr;
    unsigned		 port;
};

static int init_ice_st(pj_ice_strans *ice_st,
		       pj_bool_t add_valid_comp,
		       unsigned dummy_cnt,
		       struct dummy_cand cand[])
{
    pj_str_t a;
    pj_status_t status;
    unsigned i;

    /* Create components */
    for (i=0; i<ice_st->comp_cnt; ++i) {
	status = pj_ice_strans_create_comp(ice_st, i+1, PJ_ICE_ST_OPT_DONT_ADD_CAND, NULL);
	if (status != PJ_SUCCESS)
	    return -21;
    }

    /* Add dummy candidates */
    for (i=0; i<dummy_cnt; ++i) {
	pj_sockaddr_in addr;

	pj_sockaddr_in_init(&addr, pj_cstr(&a, cand[i].addr), (pj_uint16_t)cand[i].port);
	status = pj_ice_strans_add_cand(ice_st, cand[i].comp_id, cand[i].type,
				    65535, &addr, PJ_FALSE);
	if (status != PJ_SUCCESS)
	    return -22;
    }

    /* Add the real candidate */
    if (add_valid_comp) {
	for (i=0; i<ice_st->comp_cnt; ++i) {
	    status = pj_ice_strans_add_cand(ice_st, i+1, PJ_ICE_CAND_TYPE_HOST, 65535,
					&ice_st->comp[i]->local_addr.ipv4, PJ_TRUE);
	    if (status != PJ_SUCCESS)
		return -23;
	}
    }

    return 0;
}


/* When ICE completes, both agents should agree on the same candidate pair.
 * Check that the remote address selected by agent1 is equal to the
 * local address of selected by agent 2.
 */
static int verify_address(pj_ice_strans *agent1, pj_ice_strans *agent2,
			  unsigned comp_id)
{
    pj_ice_sess_cand *rcand, *lcand;
    int lcand_id;

    if (agent1->ice->comp[comp_id-1].valid_check == NULL) {
	PJ_LOG(3,(THIS_FILE, "....error: valid_check not set for comp_id %d", comp_id));
	return -60;
    }

    /* Get default remote candidate of agent 1 */
    rcand = agent1->ice->comp[comp_id-1].valid_check->rcand;

    /* Get default local candidate of agent 2 */
    pj_ice_sess_find_default_cand(agent2->ice, comp_id, &lcand_id);
    if (lcand_id < 0)
	return -62;

    lcand = &agent2->ice->lcand[lcand_id];

    if (pj_memcmp(&rcand->addr, &lcand->addr, sizeof(pj_sockaddr_in))!=0) {
	PJ_LOG(3,(THIS_FILE, "....error: the selected addresses are incorrect for comp_id %d", comp_id));
	return -64;
    }

    return 0;
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
			    pj_bool_t expected_success,
			    unsigned comp_cnt,
			    pj_bool_t add_valid_comp,
			    unsigned wait_before_send,
			    unsigned max_total_time,
			    unsigned ocand_cnt,
			    struct dummy_cand ocand[],
			    unsigned acand_cnt,
			    struct dummy_cand acand[])
{
    pj_ice_strans *im1, *im2;
    pj_ice_strans_cb icest_cb;
    struct ice_data *id1, *id2;
    pj_timestamp t_start, t_end;
    unsigned i;
    pj_str_t data_from_offerer, data_from_answerer;
    pj_status_t status;

#define CHECK_COMPLETE()    if (id1->complete && id2->complete) { \
				if (t_end.u32.lo==0) pj_get_timestamp(&t_end); \
			    } else {}

    PJ_LOG(3,(THIS_FILE, "...%s", title));

    pj_bzero(&t_end, sizeof(t_end));

    pj_bzero(&icest_cb, sizeof(icest_cb));
    icest_cb.on_ice_complete = &on_ice_complete;
    icest_cb.on_rx_data = &on_rx_data;

    /* Create first ICE */
    status = pj_ice_strans_create(&stun_cfg, "offerer", comp_cnt, NULL, &icest_cb, &im1);
    if (status != PJ_SUCCESS)
	return -20;

    id1 = PJ_POOL_ZALLOC_T(im1->pool, struct ice_data);
    id1->obj_name = "offerer";
    im1->user_data = id1;

    /* Init components */
    status = init_ice_st(im1, add_valid_comp, ocand_cnt, ocand);
    if (status != 0)
	return status;

    /* Create second ICE */
    status = pj_ice_strans_create(&stun_cfg, "answerer", comp_cnt, NULL, &icest_cb, &im2);
    if (status != PJ_SUCCESS)
	return -25;

    id2 = PJ_POOL_ZALLOC_T(im2->pool, struct ice_data);
    id2->obj_name = "answerer";
    im2->user_data = id2;

    /* Init components */
    status = init_ice_st(im2, add_valid_comp, acand_cnt, acand);
    if (status != 0)
	return status;


    /* Init ICE on im1 */
    status = pj_ice_strans_init_ice(im1, PJ_ICE_SESS_ROLE_CONTROLLING, NULL, NULL);
    if (status != PJ_SUCCESS)
	return -29;

    /* Init ICE on im2 */
    status = pj_ice_strans_init_ice(im2, PJ_ICE_SESS_ROLE_CONTROLLED, NULL, NULL);
    if (status != PJ_SUCCESS)
	return -29;

    /* Start ICE on im2 */
    status = start_ice(im2, im1);
    if (status != PJ_SUCCESS) {
	app_perror("   error starting ICE", status);
	return -30;
    }

    /* Start ICE on im1 */
    status = start_ice(im1, im2);
    if (status != PJ_SUCCESS)
	return -35;

    /* Apply delay to let other checks commence */
    pj_thread_sleep(40);

    /* Mark start time */
    pj_get_timestamp(&t_start);

    /* Poll for wait_before_send msecs before we send the first data */
    if (expected_success) {
	for (;;) {
	    pj_timestamp t_now;

	    handle_events(1);

	    CHECK_COMPLETE();

	    pj_get_timestamp(&t_now);
	    if (pj_elapsed_msec(&t_start, &t_now) >= wait_before_send)
		break;
	}

	/* Send data. It must be successful! */
	data_from_offerer = pj_str("from offerer");
	status = pj_ice_sess_send_data(im1->ice, 1, data_from_offerer.ptr, data_from_offerer.slen);
	if (status != PJ_SUCCESS)
	    return -47;

	data_from_answerer = pj_str("from answerer");
	status = pj_ice_sess_send_data(im2->ice, 1, data_from_answerer.ptr, data_from_answerer.slen);
	if (status != PJ_SUCCESS) {
	    app_perror("   error sending packet", status);
	    return -48;
	}

	/* Poll to allow data to be received */
	for (;;) {
	    pj_timestamp t_now;
	    handle_events(1);
	    CHECK_COMPLETE();
	    pj_get_timestamp(&t_now);
	    if (pj_elapsed_msec(&t_start, &t_now) >= (wait_before_send + 200))
		break;
	}
    }

    /* Just wait until both completes, or timed out */
    while (!id1->complete || !id2->complete) {
	pj_timestamp t_now;

	handle_events(1);

	CHECK_COMPLETE();
	pj_get_timestamp(&t_now);
	if (pj_elapsed_msec(&t_start, &t_now) >= max_total_time) {
	    PJ_LOG(3,(THIS_FILE, "....error: timed-out"));
	    return -50;
	}
    }

    /* Mark end-time */
    CHECK_COMPLETE();

    /* If expected to fail, then just check that both fail */
    if (!expected_success) {
	/* Check status */
	if (id1->err_code == PJ_SUCCESS)
	    return -51;
	if (id2->err_code == PJ_SUCCESS)
	    return -52;
	goto on_return;
    }

    /* Check status */
    if (id1->err_code != PJ_SUCCESS)
	return -53;
    if (id2->err_code != PJ_SUCCESS)
	return -56;

    /* Verify that offerer gets answerer's transport address */
    for (i=0; i<comp_cnt; ++i) {
	status = verify_address(im1, im2, i+1);
	if (status != 0)
	    return status;
    }

    /* And the other way around */
    for (i=0; i<comp_cnt; ++i) {
	status = verify_address(im2, im1, i+1);
	if (status != 0)
	    return status;
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


on_return:

    /* Done */
    PJ_LOG(3,(THIS_FILE, "....success: ICE completed in %d msec, waiting..", 
	      pj_elapsed_msec(&t_start, &t_end)));

    /* Wait for some more time */
    for (;;) {
	pj_timestamp t_now;

	pj_get_timestamp(&t_now);
	if (pj_elapsed_msec(&t_start, &t_now) > max_total_time)
	    break;

	handle_events(1);
    }


    pj_ice_strans_destroy(im1);
    pj_ice_strans_destroy(im2);
    handle_events(100);
    return 0;
}


int ice_test(void)
{
    int rc = 0;
    pj_pool_t *pool;
    pj_ioqueue_t *ioqueue;
    pj_timer_heap_t *timer_heap;
    enum { D1=500, D2=5000, D3=15000 };
    struct dummy_cand ocand[] = 
    {
	{1, PJ_ICE_CAND_TYPE_SRFLX, "127.1.1.1", 65534 },
	{2, PJ_ICE_CAND_TYPE_SRFLX, "127.1.1.1", 65535 },
    };
    struct dummy_cand acand[] =
    {
	{1, PJ_ICE_CAND_TYPE_SRFLX, "127.2.2.2", 65534 },
	{2, PJ_ICE_CAND_TYPE_SRFLX, "127.2.2.2", 65535 },
    };

    pool = pj_pool_create(mem, NULL, 4000, 4000, NULL);
    pj_ioqueue_create(pool, 12, &ioqueue);
    pj_timer_heap_create(pool, 100, &timer_heap);
    
    pj_stun_config_init(&stun_cfg, mem, 0, ioqueue, timer_heap);

#if 0
    pj_log_set_level(5);
#endif

    //goto test;

    /* Basic create/destroy */
    rc = ice_basic_create_destroy_test();
    if (rc != 0)
	goto on_return;

    /* Direct communication */
    rc = perform_ice_test("Simple test (1 component)", PJ_TRUE, 1, PJ_TRUE, D1, D2, 0, NULL, 0, NULL);
    if (rc != 0)
	goto on_return;

    /* Failure case (all checks fail) */
#if 0
    /* Cannot just add an SRFLX candidate; it needs a base */
    rc = perform_ice_test("Failure case (all checks fail)", PJ_FALSE, 1, PJ_FALSE, D3, D3, 1, ocand, 1, acand);
    if (rc != 0)
	goto on_return;
#endif

    /* Direct communication with invalid address */
    rc = perform_ice_test("With 1 unreachable address", PJ_TRUE, 1, PJ_TRUE, D1, D2, 1, ocand, 0, NULL);
    if (rc != 0)
	goto on_return;

    /* Direct communication with invalid address */
    rc = perform_ice_test("With 2 unreachable addresses (one each)", PJ_TRUE, 1, PJ_TRUE, D1, D2, 1, ocand, 1, acand);
    if (rc != 0)
	goto on_return;

    /* Direct communication with two components */
//test:
    rc = perform_ice_test("With two components (RTP and RTCP)", PJ_TRUE, 2, PJ_TRUE, D1, D2, 0, NULL, 0, NULL);
    if (rc != 0)
	goto on_return;

    goto on_return;

    /* Direct communication with mismatch number of components */

    /* Direct communication with 2 components and 2 invalid address */
    rc = perform_ice_test("With 2 two components and 2 unreachable address", PJ_TRUE, 2, PJ_TRUE, D1, D2, 1, ocand, 1, acand);
    if (rc != 0)
	goto on_return;



on_return:
    pj_log_set_level(3);
    pj_ioqueue_destroy(stun_cfg.ioqueue);
    pj_pool_release(pool);
    return rc;
}

