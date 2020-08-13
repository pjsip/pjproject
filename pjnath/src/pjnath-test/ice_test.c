/* $Id$ */
/*
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#include "server.h"

enum
{
    NO	= 0,
    YES	= 1,
    SRV	= 3,
};

#define NODELAY		0xFFFFFFFF
#define SRV_DOMAIN	"pjsip.lab.domain"
#define MAX_THREADS	16

#define THIS_FILE	"ice_test.c"
#define INDENT		"    "

/* Client flags */
enum
{
    WRONG_TURN	= 1,
    DEL_ON_ERR	= 2,
    CLIENT_IPV4	= 4,
    CLIENT_IPV6	= 8
};

/* Test results */
struct test_result
{
    pj_status_t start_status;	/* start ice successful?	*/	
    pj_status_t	init_status;	/* init successful?		*/
    pj_status_t	nego_status;	/* negotiation successful?	*/
    unsigned	rx_cnt[4];	/* Number of data received	*/
};

/*  Role    comp#   host?   stun?   turn?   flag?  ans_del snd_del des_del */
/* Test session configuration */
struct test_cfg
{
    pj_ice_sess_role role;	/* Role.			*/
    unsigned	comp_cnt;	/* Component count		*/
    unsigned    enable_host;	/* Enable host candidates	*/
    unsigned    enable_stun;	/* Enable srflx candidates	*/
    unsigned    enable_turn;	/* Enable turn candidates	*/
    unsigned	client_flag;	/* Client flags			*/

    unsigned    answer_delay;	/* Delay before sending SDP	*/
    unsigned	send_delay;	/* unused */
    unsigned	destroy_delay;	/* unused */

    struct test_result expected;/* Expected result		*/

    pj_bool_t   nom_regular;	/* Use regular nomination?	*/
    pj_ice_sess_trickle trickle;    /* Trickle ICE mode		*/
};

/* ICE endpoint state */
struct ice_ept
{
    struct test_cfg	 cfg;	/* Configuratino.		*/
    pj_ice_strans	*ice;	/* ICE stream transport		*/
    struct test_result	 result;/* Test result.			*/

    pj_str_t		 ufrag;	/* username fragment.		*/
    pj_str_t		 pass;	/* password			*/

    /* Trickle ICE */
    pj_bool_t		 last_cand; /* Got last candidate?	*/
};

/* Session param */
struct sess_param
{
    unsigned		 worker_cnt;
    unsigned		 worker_timeout;
    pj_bool_t		 worker_quit;

    pj_bool_t		 destroy_after_create;
    pj_bool_t		 destroy_after_one_done;
};

/* The test session */
struct test_sess
{
    pj_pool_t		*pool;
    pj_stun_config	*stun_cfg;
    pj_dns_resolver	*resolver;

    struct sess_param	*param;

    test_server		*server1;   /* Test server for IPv4.	*/
    test_server		*server2;   /* Test server for IPv6.	*/

    pj_thread_t		*worker_threads[MAX_THREADS];

    unsigned		 server_flag;
    struct ice_ept	 caller;
    struct ice_ept	 callee;
};


static void ice_on_rx_data(pj_ice_strans *ice_st,
			   unsigned comp_id,
			   void *pkt, pj_size_t size,
			   const pj_sockaddr_t *src_addr,
			   unsigned src_addr_len);
static void ice_on_ice_complete(pj_ice_strans *ice_st,
			        pj_ice_strans_op op,
			        pj_status_t status);
static void ice_on_new_candidate(pj_ice_strans *ice_st,
				 const pj_ice_sess_cand *cand,
				 pj_bool_t last);

static void destroy_sess(struct test_sess *sess, unsigned wait_msec);

#if USE_IPV6

static pj_bool_t enable_ipv6_test()
{
    pj_sockaddr addr;
    pj_bool_t retval = PJ_TRUE;
    if (pj_gethostip(pj_AF_INET6(), &addr) == PJ_SUCCESS) {
	const pj_in6_addr *a = &addr.ipv6.sin6_addr;
	if (a->s6_addr[0] == 0xFE && (a->s6_addr[1] & 0xC0) == 0x80) {
	    retval = PJ_FALSE;
	    PJ_LOG(3,(THIS_FILE, INDENT "Skipping IPv6 test due to link-local "
		     "address"));
	}
    } else {
	retval = PJ_FALSE;
	PJ_LOG(3,(THIS_FILE, INDENT "Skipping IPv6 test due to fail getting "
		 "IPv6 address"));
    }
    return retval;
}

#endif

static void set_stun_turn_cfg(struct ice_ept *ept, 
				     pj_ice_strans_cfg *ice_cfg, 
				     char *serverip,
				     pj_bool_t use_ipv6) 
{        
    if (ept->cfg.enable_stun & YES) {
	unsigned stun_idx = ice_cfg->stun_tp_cnt++;
	pj_ice_strans_stun_cfg_default(&ice_cfg->stun_tp[stun_idx]);

	if ((ept->cfg.enable_stun & SRV) == SRV) {
	    ice_cfg->stun_tp[stun_idx].server = pj_str(SRV_DOMAIN);
	} else {
	    ice_cfg->stun_tp[stun_idx].server = pj_str(serverip);
	}
	ice_cfg->stun_tp[stun_idx].port = STUN_SERVER_PORT;

	ice_cfg->stun_tp[stun_idx].af = GET_AF(use_ipv6);
    }
    ice_cfg->stun.af = GET_AF(use_ipv6);
    if (ept->cfg.enable_host == 0) {	
	ice_cfg->stun.max_host_cands = 0;
    } else {
	//ice_cfg.stun.no_host_cands = PJ_FALSE;	
	ice_cfg->stun.loop_addr = PJ_TRUE;
    }

    if (ept->cfg.enable_turn & YES) {
	unsigned turn_idx = ice_cfg->turn_tp_cnt++;	
	pj_ice_strans_turn_cfg_default(&ice_cfg->turn_tp[turn_idx]);

	if ((ept->cfg.enable_turn & SRV) == SRV) {
	    ice_cfg->turn_tp[turn_idx].server = pj_str(SRV_DOMAIN);
	} else {
	    ice_cfg->turn_tp[turn_idx].server = pj_str(serverip);
	}
	ice_cfg->turn_tp[turn_idx].port = TURN_SERVER_PORT;
	ice_cfg->turn_tp[turn_idx].conn_type = PJ_TURN_TP_UDP;
	ice_cfg->turn_tp[turn_idx].auth_cred.type = PJ_STUN_AUTH_CRED_STATIC;
	ice_cfg->turn_tp[turn_idx].auth_cred.data.static_cred.realm =
	    pj_str(SRV_DOMAIN);
	if (ept->cfg.client_flag & WRONG_TURN)
	    ice_cfg->turn_tp[turn_idx].auth_cred.data.static_cred.username =
	    pj_str("xxx");
	else
	    ice_cfg->turn_tp[turn_idx].auth_cred.data.static_cred.username =
	    pj_str(TURN_USERNAME);

	ice_cfg->turn_tp[turn_idx].auth_cred.data.static_cred.data_type =
	    PJ_STUN_PASSWD_PLAIN;
	ice_cfg->turn_tp[turn_idx].auth_cred.data.static_cred.data =
	    pj_str(TURN_PASSWD);
	
	ice_cfg->turn_tp[turn_idx].af = GET_AF(use_ipv6);
    }    
}

/* Create ICE stream transport */
static int create_ice_strans(struct test_sess *test_sess,
			     struct ice_ept *ept,			     
			     pj_ice_strans **p_ice)
{
    pj_ice_strans *ice;
    pj_ice_strans_cb ice_cb;
    pj_ice_strans_cfg ice_cfg;
    pj_sockaddr hostip;
    char serveripv4[PJ_INET6_ADDRSTRLEN];
    char serveripv6[PJ_INET6_ADDRSTRLEN];
    pj_status_t status;
    unsigned flag = (ept->cfg.client_flag)?ept->cfg.client_flag:CLIENT_IPV4;

    status = pj_gethostip(pj_AF_INET(), &hostip);
    if (status != PJ_SUCCESS)
	return -1030;

    pj_sockaddr_print(&hostip, serveripv4, sizeof(serveripv4), 0);

    if (flag & CLIENT_IPV6) {
	status = pj_gethostip(pj_AF_INET6(), &hostip);    
	if (status != PJ_SUCCESS)
	    return -1031;

	pj_sockaddr_print(&hostip, serveripv6, sizeof(serveripv6), 0);
    }

    /* Init callback structure */
    pj_bzero(&ice_cb, sizeof(ice_cb));
    ice_cb.on_rx_data = &ice_on_rx_data;
    ice_cb.on_ice_complete = &ice_on_ice_complete;
    ice_cb.on_new_candidate = &ice_on_new_candidate;

    /* Init ICE stream transport configuration structure */
    pj_ice_strans_cfg_default(&ice_cfg);
    ice_cfg.opt.trickle = ept->cfg.trickle;
    pj_memcpy(&ice_cfg.stun_cfg, test_sess->stun_cfg, sizeof(pj_stun_config));
    if ((ept->cfg.enable_stun & SRV)==SRV || (ept->cfg.enable_turn & SRV)==SRV)
	ice_cfg.resolver = test_sess->resolver;

    if (flag & CLIENT_IPV4) {
	set_stun_turn_cfg(ept, &ice_cfg, serveripv4, PJ_FALSE);
    }

    if (flag & CLIENT_IPV6) {
	set_stun_turn_cfg(ept, &ice_cfg, serveripv6, PJ_TRUE);
    }

    /* Create ICE stream transport */
    status = pj_ice_strans_create(NULL, &ice_cfg, ept->cfg.comp_cnt,
				  (void*)ept, &ice_cb,
				  &ice);
    if (status != PJ_SUCCESS) {
	app_perror(INDENT "err: pj_ice_strans_create()", status);
	return status;
    }

    pj_create_unique_string(test_sess->pool, &ept->ufrag);
    pj_create_unique_string(test_sess->pool, &ept->pass);

    /* Looks alright */
    *p_ice = ice;
    return PJ_SUCCESS;
}

/* Create test session */
static int create_sess(pj_stun_config *stun_cfg,
		       unsigned server_flag,
		       struct test_cfg *caller_cfg,
		       struct test_cfg *callee_cfg,
		       struct sess_param *test_param,
		       struct test_sess **p_sess)
{
    pj_pool_t *pool;
    struct test_sess *sess;
    pj_str_t ns_ip;
    pj_uint16_t ns_port;
    unsigned flags;
    pj_status_t status = PJ_SUCCESS;

    /* Create session structure */
    pool = pj_pool_create(mem, "testsess", 512, 512, NULL);
    sess = PJ_POOL_ZALLOC_T(pool, struct test_sess);
    sess->pool = pool;
    sess->stun_cfg = stun_cfg;
    sess->param = test_param;

    pj_memcpy(&sess->caller.cfg, caller_cfg, sizeof(*caller_cfg));
    sess->caller.result.init_status = sess->caller.result.nego_status = PJ_EPENDING;

    pj_memcpy(&sess->callee.cfg, callee_cfg, sizeof(*callee_cfg));
    sess->callee.result.init_status = sess->callee.result.nego_status = PJ_EPENDING;

    /* Create server */
    flags = server_flag;
    if (flags & SERVER_IPV4) {
	status = create_test_server(stun_cfg, (flags & ~SERVER_IPV6), 
				    SRV_DOMAIN, &sess->server1);
    }

    if ((status == PJ_SUCCESS) && (flags & SERVER_IPV6)) {
	status = create_test_server(stun_cfg, (flags & ~SERVER_IPV4), 
				    SRV_DOMAIN, &sess->server2);
    }

    if (status != PJ_SUCCESS) {
	app_perror(INDENT "error: create_test_server()", status);
	destroy_sess(sess, 500);
	return -10;
    }
    if (flags & SERVER_IPV4) {
	sess->server1->turn_respond_allocate =
	    sess->server1->turn_respond_refresh = PJ_TRUE;
    }

    if (flags & SERVER_IPV6) {
	sess->server2->turn_respond_allocate =
	    sess->server2->turn_respond_refresh = PJ_TRUE;
    }

    /* Create resolver */
    if ((sess->callee.cfg.enable_stun & SRV)==SRV || 
	(sess->callee.cfg.enable_turn & SRV)==SRV ||
	(sess->caller.cfg.enable_stun & SRV)==SRV || 
	(sess->caller.cfg.enable_turn & SRV)==SRV) 
    {
	status = pj_dns_resolver_create(mem, NULL, 0, stun_cfg->timer_heap,
					stun_cfg->ioqueue, &sess->resolver);
	if (status != PJ_SUCCESS) {
	    app_perror(INDENT "error: pj_dns_resolver_create()", status);
	    destroy_sess(sess, 500);
	    return -20;
	}

	ns_ip =  (flags & SERVER_IPV6)?pj_str("::1"):pj_str("127.0.0.1");
	ns_port = (pj_uint16_t)DNS_SERVER_PORT;
	status = pj_dns_resolver_set_ns(sess->resolver, 1, &ns_ip, &ns_port);
	if (status != PJ_SUCCESS) {
	    app_perror(INDENT "error: pj_dns_resolver_set_ns()", status);
	    destroy_sess(sess, 500);
	    return -21;
	}
    }

    /* Create caller ICE stream transport */
    status = create_ice_strans(sess, &sess->caller, &sess->caller.ice);
    if (status != PJ_SUCCESS) {
	destroy_sess(sess, 500);
	return -30;
    }

    /* Create callee ICE stream transport */
    status = create_ice_strans(sess, &sess->callee, &sess->callee.ice);
    if (status != PJ_SUCCESS) {
	destroy_sess(sess, 500);
	return -40;
    }

    *p_sess = sess;
    return 0;
}

/* Destroy test session */
static void destroy_sess(struct test_sess *sess, unsigned wait_msec)
{
    unsigned i;

    if (sess->caller.ice) {
	pj_ice_strans_destroy(sess->caller.ice);
	sess->caller.ice = NULL;
    }

    if (sess->callee.ice) {
	pj_ice_strans_destroy(sess->callee.ice);
	sess->callee.ice = NULL;
    }

    sess->param->worker_quit = PJ_TRUE;
    for (i=0; i<sess->param->worker_cnt; ++i) {
	if (sess->worker_threads[i])
	    pj_thread_join(sess->worker_threads[i]);
    }

    poll_events(sess->stun_cfg, wait_msec, PJ_FALSE);

    if (sess->resolver) {
	pj_dns_resolver_destroy(sess->resolver, PJ_FALSE);
	sess->resolver = NULL;
    }

    if (sess->server1) {
	destroy_test_server(sess->server1);
	sess->server1 = NULL;
    }

    if (sess->server2) {
	destroy_test_server(sess->server2);
	sess->server2 = NULL;
    }

    pj_pool_safe_release(&sess->pool);
}

static void ice_on_rx_data(pj_ice_strans *ice_st,
			   unsigned comp_id,
			   void *pkt, pj_size_t size,
			   const pj_sockaddr_t *src_addr,
			   unsigned src_addr_len)
{
    struct ice_ept *ept;

    PJ_UNUSED_ARG(pkt);
    PJ_UNUSED_ARG(size);
    PJ_UNUSED_ARG(src_addr);
    PJ_UNUSED_ARG(src_addr_len);

    ept = (struct ice_ept*) pj_ice_strans_get_user_data(ice_st);
    ept->result.rx_cnt[comp_id]++;
}


static void ice_on_ice_complete(pj_ice_strans *ice_st,
			        pj_ice_strans_op op,
			        pj_status_t status)
{
    struct ice_ept *ept;

    ept = (struct ice_ept*) pj_ice_strans_get_user_data(ice_st);
    if (!ept)
	return;

    switch (op) {
    case PJ_ICE_STRANS_OP_INIT:
	ept->result.init_status = status;
	if (status != PJ_SUCCESS && (ept->cfg.client_flag & DEL_ON_ERR)) {
	    pj_ice_strans_destroy(ice_st);
	    ept->ice = NULL;
	}
	break;
    case PJ_ICE_STRANS_OP_NEGOTIATION:
	ept->result.nego_status = status;
	break;
    case PJ_ICE_STRANS_OP_KEEP_ALIVE:
	/* keep alive failed? */
	break;
    default:
	pj_assert(!"Unknown op");
    }
}

static void ice_on_new_candidate(pj_ice_strans *ice_st,
				 const pj_ice_sess_cand *cand,
				 pj_bool_t last)
{
    struct ice_ept *ept;
    char buf1[PJ_INET6_ADDRSTRLEN+10];
    char buf2[PJ_INET6_ADDRSTRLEN+10];

    ept = (struct ice_ept*) pj_ice_strans_get_user_data(ice_st);
    if (!ept)
	return;

    ept->last_cand = last;

    if (cand) {
	PJ_LOG(4,(THIS_FILE, INDENT "%p: discovered a new candidate "
		  "comp=%d, type=%s, addr=%s, baseaddr=%s, end=%d",
		  ept->ice, cand->comp_id,
		  pj_ice_get_cand_type_name(cand->type),
		  pj_sockaddr_print(&cand->addr, buf1, sizeof(buf1), 3),
		  pj_sockaddr_print(&cand->base_addr, buf2, sizeof(buf2), 3),
		  last));
    } else if (ept->ice && last) {
	PJ_LOG(4,(THIS_FILE, INDENT "%p: end of candidate", ept->ice));
    }
}

/* Start ICE negotiation on the endpoint, based on parameter from
 * the other endpoint.
 */
static pj_status_t start_ice(struct ice_ept *ept, const struct ice_ept *remote)
{
    pj_ice_sess_cand rcand[32];
    unsigned rcand_cnt = 0;
    pj_status_t status;

    /* Enum remote candidates */
    if (ept->cfg.trickle == PJ_ICE_SESS_TRICKLE_DISABLED) {
	unsigned i;
	for (i=0; i<remote->cfg.comp_cnt; ++i) {
	    unsigned cnt = PJ_ARRAY_SIZE(rcand) - rcand_cnt;
	    status = pj_ice_strans_enum_cands(remote->ice, i+1, &cnt, rcand+rcand_cnt);
	    if (status != PJ_SUCCESS) {
		app_perror(INDENT "err: pj_ice_strans_enum_cands()", status);
		return status;
	    }
	    rcand_cnt += cnt;
	}
    }

    status = pj_ice_strans_start_ice(ept->ice, &remote->ufrag, &remote->pass,
				     rcand_cnt, rcand);

    if (status != ept->cfg.expected.start_status) {
	app_perror(INDENT "err: pj_ice_strans_start_ice()", status);
	return status;
    }

    return status;
}


/* Check that the pair in both agents are matched */
static int check_pair(const struct ice_ept *ept1, const struct ice_ept *ept2,
		      int start_err)
{
    unsigned i, min_cnt, max_cnt;

    if (ept1->cfg.comp_cnt < ept2->cfg.comp_cnt) {
	min_cnt = ept1->cfg.comp_cnt;
	max_cnt = ept2->cfg.comp_cnt;
    } else {
	min_cnt = ept2->cfg.comp_cnt;
	max_cnt = ept1->cfg.comp_cnt;
    }

    /* Must have valid pair for common components */
    for (i=0; i<min_cnt; ++i) {
	const pj_ice_sess_check *c1;
	const pj_ice_sess_check *c2;

	c1 = pj_ice_strans_get_valid_pair(ept1->ice, i+1);
	if (c1 == NULL) {
	    PJ_LOG(3,(THIS_FILE, INDENT "err: unable to get valid pair for ice1 "
			  "component %d", i+1));
	    return start_err - 2;
	}

	c2 = pj_ice_strans_get_valid_pair(ept2->ice, i+1);
	if (c2 == NULL) {
	    PJ_LOG(3,(THIS_FILE, INDENT "err: unable to get valid pair for ice2 "
			  "component %d", i+1));
	    return start_err - 4;
	}

	if (pj_sockaddr_cmp(&c1->rcand->addr, &c2->lcand->addr) != 0) {
	    PJ_LOG(3,(THIS_FILE, INDENT "err: candidate pair does not match "
			  "for component %d", i+1));
	    return start_err - 6;
	}
    }

    /* Extra components must not have valid pair */
    for (; i<max_cnt; ++i) {
	if (ept1->cfg.comp_cnt>i &&
	    pj_ice_strans_get_valid_pair(ept1->ice, i+1) != NULL)
	{
	    PJ_LOG(3,(THIS_FILE, INDENT "err: ice1 shouldn't have valid pair "
		          "for component %d", i+1));
	    return start_err - 8;
	}
	if (ept2->cfg.comp_cnt>i &&
	    pj_ice_strans_get_valid_pair(ept2->ice, i+1) != NULL)
	{
	    PJ_LOG(3,(THIS_FILE, INDENT "err: ice2 shouldn't have valid pair "
		          "for component %d", i+1));
	    return start_err - 9;
	}
    }

    return 0;
}


#define WAIT_UNTIL(timeout,expr, RC)  { \
				pj_time_val t0, t; \
				pj_gettimeofday(&t0); \
				RC = -1; \
				for (;;) { \
				    poll_events(stun_cfg, 10, PJ_FALSE); \
				    pj_gettimeofday(&t); \
				    if (expr) { \
					RC = PJ_SUCCESS; \
					break; \
				    } \
				    PJ_TIME_VAL_SUB(t, t0); \
				    if ((unsigned)PJ_TIME_VAL_MSEC(t) >= (timeout)) \
					break; \
				} \
			    }

int worker_thread_proc(void *data)
{
    pj_status_t rc;
    struct test_sess *sess = (struct test_sess *) data;
    pj_stun_config *stun_cfg = sess->stun_cfg;

    /* Wait until negotiation is complete on both endpoints */
#define ALL_DONE    (sess->param->worker_quit || \
			(sess->caller.result.nego_status!=PJ_EPENDING && \
			 sess->callee.result.nego_status!=PJ_EPENDING))
    WAIT_UNTIL(sess->param->worker_timeout, ALL_DONE, rc);
    PJ_UNUSED_ARG(rc);
    return 0;
}

static int perform_test2(const char *title,
			 pj_stun_config *stun_cfg,
                         unsigned server_flag,
		         struct test_cfg *caller_cfg,
		         struct test_cfg *callee_cfg,
		         struct sess_param *test_param)
{
    pjlib_state pjlib_state;
    struct test_sess *sess;
    unsigned i;
    int rc;
    char add_title1[16];
    char add_title2[16];
    pj_bool_t client_mix_test = ((callee_cfg->client_flag &
				 (CLIENT_IPV4+CLIENT_IPV6)) !=
				 (caller_cfg->client_flag &
				 (CLIENT_IPV4+CLIENT_IPV6)));

    sprintf(add_title1, "%s%s%s", (server_flag & SERVER_IPV4)?"IPv4":"", 
	    ((server_flag & SERVER_IPV4)&&(server_flag & SERVER_IPV6))?"+":"",
	    (server_flag & SERVER_IPV6)?"IPv6":"");
    
    sprintf(add_title2, "%s", client_mix_test?"Mix test":"");

    PJ_LOG(3,(THIS_FILE, INDENT "%s (%s) %s", title, add_title1, add_title2));

    capture_pjlib_state(stun_cfg, &pjlib_state);

    rc = create_sess(stun_cfg, server_flag, caller_cfg, callee_cfg, test_param,
		     &sess);
    if (rc != 0)
	return rc;

#define ALL_READY   (sess->caller.result.init_status!=PJ_EPENDING && \
		     sess->callee.result.init_status!=PJ_EPENDING)

    /* Wait until both ICE transports are initialized */
    WAIT_UNTIL(30000, ALL_READY, rc);

    if (!ALL_READY) {
	PJ_LOG(3,(THIS_FILE, INDENT "err: init timed-out"));
	destroy_sess(sess, 500);
	return -100;
    }

    if (sess->caller.result.init_status != sess->caller.cfg.expected.init_status) {
	app_perror(INDENT "err: caller init", sess->caller.result.init_status);
	destroy_sess(sess, 500);
	return -102;
    }
    if (sess->callee.result.init_status != sess->callee.cfg.expected.init_status) {
	app_perror(INDENT "err: callee init", sess->callee.result.init_status);
	destroy_sess(sess, 500);
	return -104;
    }

    /* Failure condition */
    if (sess->caller.result.init_status != PJ_SUCCESS ||
	sess->callee.result.init_status != PJ_SUCCESS)
    {
	rc = 0;
	goto on_return;
    }
    /* Init ICE on caller */
    rc = pj_ice_strans_init_ice(sess->caller.ice, sess->caller.cfg.role,
				&sess->caller.ufrag, &sess->caller.pass);
    if (rc != PJ_SUCCESS) {
	app_perror(INDENT "err: caller pj_ice_strans_init_ice()", rc);
	destroy_sess(sess, 500);
	return -100;
    }

    /* Init ICE on callee */
    rc = pj_ice_strans_init_ice(sess->callee.ice, sess->callee.cfg.role,
				&sess->callee.ufrag, &sess->callee.pass);
    if (rc != PJ_SUCCESS) {
	app_perror(INDENT "err: callee pj_ice_strans_init_ice()", rc);
	destroy_sess(sess, 500);
	return -110;
    }
    /* Start ICE on callee */
    rc = start_ice(&sess->callee, &sess->caller);
    if (rc != PJ_SUCCESS) {
	int retval = (rc == sess->callee.cfg.expected.start_status)?0:-120;
	destroy_sess(sess, 500);	
	return retval;
    }
    /* Wait for callee's answer_delay */
    poll_events(stun_cfg, sess->callee.cfg.answer_delay, PJ_FALSE);
    /* Start ICE on caller */
    rc = start_ice(&sess->caller, &sess->callee);
    if (rc != PJ_SUCCESS) {
	int retval = (rc == sess->caller.cfg.expected.start_status)?0:-130;
	destroy_sess(sess, 500);
	return retval;
    }

    for (i=0; i<sess->param->worker_cnt; ++i) {
	pj_status_t status;

	status = pj_thread_create(sess->pool, "worker_thread",
				  worker_thread_proc, sess, 0, 0,
				  &sess->worker_threads[i]);
	if (status != PJ_SUCCESS) {
	    PJ_LOG(3,(THIS_FILE, INDENT "err: create thread"));
	    destroy_sess(sess, 500);
	    return -135;
	}
    }

    if (sess->param->destroy_after_create)
	goto on_destroy;

    if (sess->param->destroy_after_one_done) {
	while (sess->caller.result.init_status==PJ_EPENDING &&
	       sess->callee.result.init_status==PJ_EPENDING)
	{
	    if (sess->param->worker_cnt)
		pj_thread_sleep(0);
	    else
		poll_events(stun_cfg, 0, PJ_FALSE);
	}
	goto on_destroy;
    }

    WAIT_UNTIL(30000, ALL_DONE, rc);
    if (!ALL_DONE) {
	PJ_LOG(3,(THIS_FILE, INDENT "err: negotiation timed-out"));
	destroy_sess(sess, 500);
	return -140;
    }

    if (sess->caller.result.nego_status != sess->caller.cfg.expected.nego_status) {
	app_perror(INDENT "err: caller negotiation failed", sess->caller.result.nego_status);
	destroy_sess(sess, 500);
	return -150;
    }

    if (sess->callee.result.nego_status != sess->callee.cfg.expected.nego_status) {
	app_perror(INDENT "err: callee negotiation failed", sess->callee.result.nego_status);
	destroy_sess(sess, 500);
	return -160;
    }

    /* Verify that both agents have agreed on the same pair */
    rc = check_pair(&sess->caller, &sess->callee, -170);
    if (rc != 0) {
	destroy_sess(sess, 500);
	return rc;
    }
    rc = check_pair(&sess->callee, &sess->caller, -180);
    if (rc != 0) {
	destroy_sess(sess, 500);
	return rc;
    }

    /* Looks like everything is okay */
on_destroy:

    /* Destroy ICE stream transports first to let it de-allocate
     * TURN relay (otherwise there'll be timer/memory leak, unless
     * we wait for long time in the last poll_events() below).
     */
    if (sess->caller.ice) {
	pj_ice_strans_destroy(sess->caller.ice);
	sess->caller.ice = NULL;
    }

    if (sess->callee.ice) {
	pj_ice_strans_destroy(sess->callee.ice);
	sess->callee.ice = NULL;
    }

on_return:
    /* Wait.. */
    poll_events(stun_cfg, 200, PJ_FALSE);

    /* Now destroy everything */
    destroy_sess(sess, 500);

    /* Flush events */
    poll_events(stun_cfg, 100, PJ_FALSE);

    rc = check_pjlib_state(stun_cfg, &pjlib_state);
    if (rc != 0) {
	return rc;
    }

    return rc;
}

static void set_client_server_flag(unsigned server_flag,
				   unsigned caller_flag,
				   unsigned callee_flag,
				   unsigned *res_server_flag,
				   unsigned *res_caller_flag,
				   unsigned *res_callee_flag)
{
    enum {
	RST_CLT_FLAG = CLIENT_IPV4+CLIENT_IPV6,
	RST_SRV_FLAG = SERVER_IPV4+SERVER_IPV6
    };

    *res_server_flag = (*res_server_flag & ~RST_SRV_FLAG) | server_flag;
    *res_caller_flag = (*res_caller_flag & ~RST_CLT_FLAG) | caller_flag;
    *res_callee_flag = (*res_callee_flag & ~RST_CLT_FLAG) | callee_flag;
}

static int perform_test(const char *title,
                        pj_stun_config *stun_cfg,
                        unsigned server_flag,
                        struct test_cfg *caller_cfg,
                        struct test_cfg *callee_cfg)
{
    struct sess_param test_param;
    int rc;
    int expected_caller_start_ice = caller_cfg->expected.start_status;
    int expected_callee_start_ice = callee_cfg->expected.start_status;

    set_client_server_flag(SERVER_IPV4, CLIENT_IPV4, CLIENT_IPV4,
			   &server_flag, &caller_cfg->client_flag, 
			   &callee_cfg->client_flag);


    pj_bzero(&test_param, sizeof(test_param));

    rc = perform_test2(title, stun_cfg, server_flag, caller_cfg,
		       callee_cfg, &test_param);

#if USE_IPV6
    if (enable_ipv6_test()) {

	/* Test for IPV6. */
	if (rc == PJ_SUCCESS) {
	    pj_bzero(&test_param, sizeof(test_param));
	    set_client_server_flag(SERVER_IPV6, CLIENT_IPV6, CLIENT_IPV6,
				   &server_flag, &caller_cfg->client_flag,
				   &callee_cfg->client_flag);

	    rc = perform_test2(title, stun_cfg, server_flag, caller_cfg,
			       callee_cfg, &test_param);
	}

	/* Test for IPV4+IPV6. */
	if (rc == PJ_SUCCESS) {
	    pj_bzero(&test_param, sizeof(test_param));
	    set_client_server_flag(SERVER_IPV4+SERVER_IPV6,
				   CLIENT_IPV4+CLIENT_IPV6,
				   CLIENT_IPV4+CLIENT_IPV6,
				   &server_flag,
				   &caller_cfg->client_flag,
				   &callee_cfg->client_flag);

	    rc = perform_test2(title, stun_cfg, server_flag, caller_cfg,
			       callee_cfg, &test_param);
	}

	/* Test controller(IPV4) vs controlled(IPV6). */
	if (rc == PJ_SUCCESS) {
	    pj_bzero(&test_param, sizeof(test_param));
	    set_client_server_flag(SERVER_IPV4+SERVER_IPV6,
				   CLIENT_IPV4,
				   CLIENT_IPV6,
				   &server_flag,
				   &caller_cfg->client_flag,
				   &callee_cfg->client_flag);
	    caller_cfg->expected.start_status = PJ_ENOTFOUND;
	    callee_cfg->expected.start_status = PJ_ENOTFOUND;

	    rc = perform_test2(title, stun_cfg, server_flag, caller_cfg,
			       callee_cfg, &test_param);
	}
    }
#endif
    callee_cfg->expected.start_status = expected_callee_start_ice;
    caller_cfg->expected.start_status = expected_caller_start_ice;

    return rc;
}

#define ROLE1	PJ_ICE_SESS_ROLE_CONTROLLED
#define ROLE2	PJ_ICE_SESS_ROLE_CONTROLLING

int ice_test(void)
{
    pj_pool_t *pool;
    pj_stun_config stun_cfg;
    unsigned i;
    int rc;
    struct sess_cfg_t {
	const char	*title;
	unsigned	 server_flag;
	struct test_cfg	 ua1;
	struct test_cfg	 ua2;
    } sess_cfg[] =
    {
	/*  Role    comp#   host?   stun?   turn?   flag?  ans_del snd_del des_del */
	{
	    "hosts candidates only",
	    0x1FFF,
	    {ROLE1, 1,	    YES,    NO,	    NO,	    NO,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS, PJ_SUCCESS}},
	    {ROLE2, 1,	    YES,    NO,	    NO,	    NO,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS, PJ_SUCCESS}}
	},
	{
	    "host and srflxes",
	    0x1FFF,
	    {ROLE1, 1,	    YES,    YES,    NO,	    NO,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS, PJ_SUCCESS}},
	    {ROLE2, 1,	    YES,    YES,    NO,	    NO,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS, PJ_SUCCESS}}
	},
	{
	    "host vs relay",
	    0x1FFF,
	    {ROLE1, 1,	    YES,    NO,    NO,	    NO,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS, PJ_SUCCESS}},
	    {ROLE2, 1,	    NO,     NO,    YES,	    NO,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS, PJ_SUCCESS}}
	},
	{
	    "relay vs host",
	    0x1FFF,
	    {ROLE1, 1,	    NO,	    NO,   YES,	    NO,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS, PJ_SUCCESS}},
	    {ROLE2, 1,	   YES,     NO,    NO,	    NO,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS, PJ_SUCCESS}}
	},
	{
	    "relay vs relay",
	    0x1FFF,
	    {ROLE1, 1,	    NO,	    NO,   YES,	    NO,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS, PJ_SUCCESS}},
	    {ROLE2, 1,	    NO,     NO,   YES,	    NO,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS, PJ_SUCCESS}}
	},
	{
	    "all candidates",
	    0x1FFF,
	    {ROLE1, 1,	   YES,	   YES,   YES,	    NO,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS, PJ_SUCCESS}},
	    {ROLE2, 1,	   YES,    YES,   YES,	    NO,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS, PJ_SUCCESS}}
	},
    };

    pool = pj_pool_create(mem, NULL, 512, 512, NULL);
    rc = create_stun_config(pool, &stun_cfg);
    if (rc != PJ_SUCCESS) {
	pj_pool_release(pool);
	return -7;
    }

    /* Simple test first with host candidate */
    if (1) {
	struct sess_cfg_t cfg =
	{
	    "Basic with host candidates",
	    0x0,
	    /*  Role    comp#   host?   stun?   turn?   flag?  ans_del snd_del des_del */
	    {ROLE1,	1,	YES,     NO,	    NO,	    0,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS, PJ_SUCCESS}},
	    {ROLE2,	1,	YES,     NO,	    NO,	    0,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS, PJ_SUCCESS}}
	};

	rc = perform_test(cfg.title, &stun_cfg, cfg.server_flag,
			  &cfg.ua1, &cfg.ua2);
	if (rc != 0)
	    goto on_return;

	cfg.ua1.comp_cnt = 2;
	cfg.ua2.comp_cnt = 2;
	rc = perform_test("Basic with host candidates, 2 components",
			  &stun_cfg, cfg.server_flag,
			  &cfg.ua1, &cfg.ua2);
	if (rc != 0)
	    goto on_return;
    }

    /* Simple test first with srflx candidate */
    if (1) {
	struct sess_cfg_t cfg =
	{
	    "Basic with srflx candidates",
	    0xFFFF,
	    /*  Role    comp#   host?   stun?   turn?   flag?  ans_del snd_del des_del */
	    {ROLE1,	1,	YES,    YES,	    NO,	    0,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS, PJ_SUCCESS}},
	    {ROLE2,	1,	YES,    YES,	    NO,	    0,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS, PJ_SUCCESS}}
	};

	rc = perform_test(cfg.title, &stun_cfg, cfg.server_flag,
			  &cfg.ua1, &cfg.ua2);
	if (rc != 0)
	    goto on_return;

	cfg.ua1.comp_cnt = 2;
	cfg.ua2.comp_cnt = 2;

	rc = perform_test("Basic with srflx candidates, 2 components",
			  &stun_cfg, cfg.server_flag,
			  &cfg.ua1, &cfg.ua2);
	if (rc != 0)
	    goto on_return;
    }

    /* Simple test with relay candidate */
    if (1) {
	struct sess_cfg_t cfg =
	{
	    "Basic with relay candidates",
	    0xFFFF,
	    /*  Role    comp#   host?   stun?   turn?   flag?  ans_del snd_del des_del */
	    {ROLE1,	1,	 NO,     NO,	  YES,	    0,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS, PJ_SUCCESS}},
	    {ROLE2,	1,	 NO,     NO,	  YES,	    0,	    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS, PJ_SUCCESS}}
	};

	rc = perform_test(cfg.title, &stun_cfg, cfg.server_flag,
			  &cfg.ua1, &cfg.ua2);
	if (rc != 0)
	    goto on_return;

	cfg.ua1.comp_cnt = 2;
	cfg.ua2.comp_cnt = 2;

	rc = perform_test("Basic with relay candidates, 2 components",
			  &stun_cfg, cfg.server_flag,
			  &cfg.ua1, &cfg.ua2);
	if (rc != 0)
	    goto on_return;
    }

    /* Failure test with STUN resolution */
    if (1) {
	struct sess_cfg_t cfg =
	{
	    "STUN resolution failure",
	    0x0,
	    /*  Role    comp#   host?   stun?   turn?   flag?  ans_del snd_del des_del */
	    {ROLE1,	2,	 NO,    YES,	    NO,	    0,	    0,	    0,	    0, {PJ_SUCCESS, PJNATH_ESTUNTIMEDOUT, -1}},
	    {ROLE2,	2,	 NO,    YES,	    NO,	    0,	    0,	    0,	    0, {PJ_SUCCESS, PJNATH_ESTUNTIMEDOUT, -1}}
	};

	rc = perform_test(cfg.title, &stun_cfg, cfg.server_flag,
			  &cfg.ua1, &cfg.ua2);
	if (rc != 0)
	    goto on_return;

	cfg.ua1.client_flag |= DEL_ON_ERR;
	cfg.ua2.client_flag |= DEL_ON_ERR;

	rc = perform_test("STUN resolution failure with destroy on callback",
			  &stun_cfg, cfg.server_flag,
			  &cfg.ua1, &cfg.ua2);
	if (rc != 0)
	    goto on_return;
    }

    /* Failure test with TURN resolution */
    if (1) {
	struct sess_cfg_t cfg =
	{
	    "TURN allocation failure",
	    0xFFFF,
	    /*  Role    comp#   host?   stun?   turn?   flag?  ans_del snd_del des_del */
	    {ROLE1,	2,	 NO,    NO,	YES, WRONG_TURN,    0,	    0,	    0, {PJ_SUCCESS, PJ_STATUS_FROM_STUN_CODE(401), -1}},
	    {ROLE2,	2,	 NO,    NO,	YES, WRONG_TURN,    0,	    0,	    0, {PJ_SUCCESS, PJ_STATUS_FROM_STUN_CODE(401), -1}}
	};

	rc = perform_test(cfg.title, &stun_cfg, cfg.server_flag,
			  &cfg.ua1, &cfg.ua2);
	if (rc != 0)
	    goto on_return;

	cfg.ua1.client_flag |= DEL_ON_ERR;
	cfg.ua2.client_flag |= DEL_ON_ERR;

	rc = perform_test("TURN allocation failure with destroy on callback",
			  &stun_cfg, cfg.server_flag,
			  &cfg.ua1, &cfg.ua2);
	if (rc != 0)
	    goto on_return;
    }


    /* STUN failure, testing TURN deallocation */
    if (1) {
	struct sess_cfg_t cfg =
	{
	    "STUN failure, testing TURN deallocation",
	    0xFFFF & (~(CREATE_STUN_SERVER)),
	    /*  Role    comp#   host?   stun?   turn?   flag?  ans_del snd_del des_del */
	    {ROLE1,	1,	 YES,    YES,	YES,	0,    0,	    0,	    0, {PJ_SUCCESS, PJNATH_ESTUNTIMEDOUT, -1}},
	    {ROLE2,	1,	 YES,    YES,	YES,	0,    0,	    0,	    0, {PJ_SUCCESS, PJNATH_ESTUNTIMEDOUT, -1}}
	};

	rc = perform_test(cfg.title, &stun_cfg, cfg.server_flag,
			  &cfg.ua1, &cfg.ua2);
	if (rc != 0)
	    goto on_return;

	cfg.ua1.client_flag |= DEL_ON_ERR;
	cfg.ua2.client_flag |= DEL_ON_ERR;

	rc = perform_test("STUN failure, testing TURN deallocation (cb)",
			  &stun_cfg, cfg.server_flag,
			  &cfg.ua1, &cfg.ua2);
	if (rc != 0)
	    goto on_return;
    }

    rc = 0;
    /* Iterate each test item */
    for (i=0; i<PJ_ARRAY_SIZE(sess_cfg); ++i) {
	struct sess_cfg_t *cfg = &sess_cfg[i];
	unsigned delay[] = { 50, 2000 };
	unsigned d;

	PJ_LOG(3,(THIS_FILE, "  %s", cfg->title));

	/* For each test item, test with various answer delay */
	for (d=0; d<PJ_ARRAY_SIZE(delay); ++d) {
	    struct role_t {
		pj_ice_sess_role	ua1;
		pj_ice_sess_role	ua2;
	    } role[] =
	    {
		{ ROLE1, ROLE2},
		{ ROLE2, ROLE1},
		{ ROLE1, ROLE1},
		{ ROLE2, ROLE2}
	    };
	    unsigned j;

	    cfg->ua1.answer_delay = delay[d];
	    cfg->ua2.answer_delay = delay[d];

	    /* For each test item, test with role conflict scenarios */
	    for (j=0; j<PJ_ARRAY_SIZE(role); ++j) {
		unsigned k1;

		cfg->ua1.role = role[j].ua1;
		cfg->ua2.role = role[j].ua2;

		/* For each test item, test with different number of components */
		for (k1=1; k1<=2; ++k1) {
		    unsigned k2;

		    cfg->ua1.comp_cnt = k1;

		    for (k2=1; k2<=2; ++k2) {
			char title[120];

			sprintf(title,
				"%s/%s, %dms answer delay, %d vs %d components",
				pj_ice_sess_role_name(role[j].ua1),
				pj_ice_sess_role_name(role[j].ua2),
				delay[d], k1, k2);

			cfg->ua2.comp_cnt = k2;
			rc = perform_test(title, &stun_cfg, cfg->server_flag,
					  &cfg->ua1, &cfg->ua2);
			if (rc != 0)
			    goto on_return;
		    }
		}
	    }
	}
    }

on_return:
    destroy_stun_config(&stun_cfg);
    pj_pool_release(pool);
    return rc;
}

int ice_one_conc_test(pj_stun_config *stun_cfg, int err_quit)
{
    struct sess_cfg_t {
	const char	*title;
	unsigned	 server_flag;
	struct test_cfg	 ua1;
	struct test_cfg	 ua2;
    } cfg =
    {
	"Concurrency test",
	0x1FFF,
	/*  Role    comp#   host?   stun?   turn?   flag?  ans_del snd_del des_del */
	{ROLE1,	1,	YES,     YES,	    YES,    CLIENT_IPV4,    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS, PJ_SUCCESS}},
	{ROLE2,	1,	YES,     YES,	    YES,    CLIENT_IPV4,    0,	    0,	    0, {PJ_SUCCESS, PJ_SUCCESS, PJ_SUCCESS}}
    };
    struct sess_param test_param;
    int rc;


    /* test a: destroy as soon as nego starts */
    cfg.title = "    ice test a: immediate destroy";
    pj_bzero(&test_param, sizeof(test_param));
    test_param.worker_cnt = 4;
    test_param.worker_timeout = 1000;
    test_param.destroy_after_create = PJ_TRUE;

    rc = perform_test2(cfg.title, stun_cfg, cfg.server_flag,
                       &cfg.ua1, &cfg.ua2, &test_param);
    if (rc != 0 && err_quit)
        return rc;

    /* test b: destroy as soon as one is done */
    cfg.title = "    ice test b: destroy after 1 success";
    test_param.destroy_after_create = PJ_FALSE;
    test_param.destroy_after_one_done = PJ_TRUE;

    rc = perform_test2(cfg.title, stun_cfg, cfg.server_flag,
                       &cfg.ua1, &cfg.ua2, &test_param);
    if (rc != 0 && err_quit)
        return rc;

    /* test c: normal */
    cfg.title = "    ice test c: normal flow";
    pj_bzero(&test_param, sizeof(test_param));
    test_param.worker_cnt = 4;
    test_param.worker_timeout = 1000;

    rc = perform_test2(cfg.title, stun_cfg, cfg.server_flag,
                       &cfg.ua1, &cfg.ua2, &test_param);
    if (rc != 0 && err_quit)
        return rc;

    return 0;
}

int ice_conc_test(void)
{
    const unsigned LOOP = 100;
    pj_pool_t *pool;
    pj_stun_config stun_cfg;
    unsigned i;
    int rc;

    pool = pj_pool_create(mem, NULL, 512, 512, NULL);
    rc = create_stun_config(pool, &stun_cfg);
    if (rc != PJ_SUCCESS) {
	pj_pool_release(pool);
	return -7;
    }

    for (i = 0; i < LOOP; i++) {
	PJ_LOG(3,(THIS_FILE, INDENT "Test %d of %d", i+1, LOOP));
	rc = ice_one_conc_test(&stun_cfg, PJ_TRUE);
	if (rc)
	    break;
    }

    /* Avoid compiler warning */
    goto on_return;

on_return:
    destroy_stun_config(&stun_cfg);
    pj_pool_release(pool);

    return rc;
}

struct timer_data
{
    struct test_sess	*sess;
    unsigned		 caller_last_cand_cnt[PJ_ICE_MAX_COMP];
    unsigned		 callee_last_cand_cnt[PJ_ICE_MAX_COMP];
};


/* Timer callback to check & signal new candidates */
static void timer_new_cand(pj_timer_heap_t *th, pj_timer_entry *te)
{
    struct timer_data *data = (struct timer_data*)te->user_data;
    struct test_sess *sess = data->sess;
    struct ice_ept *caller = &sess->caller;
    struct ice_ept *callee = &sess->callee;
    pj_bool_t caller_last_cand, callee_last_cand;
    unsigned i, ncomp;
    pj_status_t rc;

    /* ICE transport may have been destroyed */
    if (!caller->ice || !callee->ice)
	return;

    caller_last_cand = caller->last_cand;
    callee_last_cand = callee->last_cand;
    //PJ_LOG(3,(THIS_FILE, INDENT "End-of-cand status: caller=%d callee=%d",
    //		caller_last_cand, callee_last_cand));

    ncomp = PJ_MIN(caller->cfg.comp_cnt, callee->cfg.comp_cnt);
    for (i = 0; i < ncomp; ++i) {
	pj_ice_sess_cand cand[PJ_ICE_ST_MAX_CAND];
	unsigned j, cnt;

	/* Check caller candidates */
	cnt = PJ_ICE_ST_MAX_CAND;
	rc = pj_ice_strans_enum_cands(caller->ice, i+1, &cnt, cand);
	if (rc != PJ_SUCCESS) {
	    app_perror(INDENT "err: caller pj_ice_strans_enum_cands()", rc);
	    continue;
	}

	if (cnt > data->caller_last_cand_cnt[i]) {
	    unsigned new_cnt = cnt - data->caller_last_cand_cnt[i];

	    /* Update remote with new candidates */
	    rc = pj_ice_strans_update_check_list(callee->ice,
						 &caller->ufrag,
						 &caller->pass,
						 new_cnt, &cand[cnt - new_cnt],
						 caller_last_cand && (i==ncomp-1));
	    if (rc != PJ_SUCCESS) {
		app_perror(INDENT "err: callee pj_ice_strans_update_check_list()", rc);
		continue;
	    }

	    data->caller_last_cand_cnt[i] = cnt;
	    PJ_LOG(4,(THIS_FILE, INDENT "Updated callee with %d new candidates %s",
		      new_cnt, (caller_last_cand?"(last)":"")));

	    for (j = 0; j < new_cnt; ++j) {
		pj_ice_sess_cand *c = &cand[cnt - new_cnt + j];
		char buf1[PJ_INET6_ADDRSTRLEN+10];
		char buf2[PJ_INET6_ADDRSTRLEN+10];
		PJ_LOG(4,(THIS_FILE, INDENT
			  "%d: comp=%d, type=%s, addr=%s, baseaddr=%s",
			  j+1, c->comp_id,
			  pj_ice_get_cand_type_name(c->type),
			  pj_sockaddr_print(&c->addr, buf1, sizeof(buf1), 3),
			  pj_sockaddr_print(&c->base_addr, buf2, sizeof(buf2), 3)
			  ));
	    }
	}

	/* Check callee candidates */
	cnt = PJ_ICE_ST_MAX_CAND;
	rc = pj_ice_strans_enum_cands(callee->ice, i+1, &cnt, cand);
	if (rc != PJ_SUCCESS) {
	    app_perror(INDENT "err: caller pj_ice_strans_enum_cands()", rc);
	    continue;
	}

	if (cnt > data->callee_last_cand_cnt[i]) {
	    unsigned new_cnt = cnt - data->callee_last_cand_cnt[i];

	    /* Update remote with new candidates */
	    rc = pj_ice_strans_update_check_list(caller->ice,
						 &callee->ufrag,
						 &callee->pass,
						 new_cnt, &cand[cnt - new_cnt],
						 callee_last_cand && (i==ncomp-1));
	    if (rc != PJ_SUCCESS) {
		app_perror(INDENT "err: caller pj_ice_strans_update_check_list()", rc);
		continue;
	    }

	    data->callee_last_cand_cnt[i] = cnt;
	    PJ_LOG(4,(THIS_FILE, INDENT "Updated caller with %d new candidates %s",
		      new_cnt, (callee_last_cand?"(last)":"")));

	    for (j = 0; j < new_cnt; ++j) {
		pj_ice_sess_cand *c = &cand[cnt - new_cnt + j];
		char buf1[PJ_INET6_ADDRSTRLEN+10];
		char buf2[PJ_INET6_ADDRSTRLEN+10];
		PJ_LOG(4,(THIS_FILE, INDENT
			  "%d: comp=%d, type=%s, addr=%s, baseaddr=%s",
			  j+1, c->comp_id,
			  pj_ice_get_cand_type_name(c->type),
			  pj_sockaddr_print(&c->addr, buf1, sizeof(buf1), 3),
			  pj_sockaddr_print(&c->base_addr, buf2, sizeof(buf2), 3)
			  ));
	    }
	}
    }

    if (!caller_last_cand || !callee_last_cand) {
	/* Reschedule until all candidates are gathered */
	pj_time_val timeout = {0, 10};
	pj_time_val_normalize(&timeout);
	pj_timer_heap_schedule(th, te, &timeout);
	//PJ_LOG(3,(THIS_FILE, INDENT "Rescheduled new candidate check"));
    }
}


static int perform_trickle_test(const char *title,
				pj_stun_config *stun_cfg,
				unsigned server_flag,
				struct test_cfg *caller_cfg,
				struct test_cfg *callee_cfg,
				struct sess_param *test_param)
{
    pjlib_state pjlib_state;
    struct test_sess *sess;
    struct timer_data timer_data;
    pj_timer_entry te_new_cand;
    int rc;

    PJ_LOG(3,(THIS_FILE, "%s, %d vs %d components",
	      title, caller_cfg->comp_cnt, callee_cfg->comp_cnt));

    capture_pjlib_state(stun_cfg, &pjlib_state);

    rc = create_sess(stun_cfg, server_flag, caller_cfg, callee_cfg,
		     test_param, &sess);
    if (rc != 0)
	return rc;

    /* Init ICE on caller */
    rc = pj_ice_strans_init_ice(sess->caller.ice, sess->caller.cfg.role,
				&sess->caller.ufrag, &sess->caller.pass);
    if (rc != PJ_SUCCESS) {
	app_perror(INDENT "err: caller pj_ice_strans_init_ice()", rc);
	rc = -100;
	goto on_return;
    }

    /* Init ICE on callee */
    rc = pj_ice_strans_init_ice(sess->callee.ice, sess->callee.cfg.role,
				&sess->callee.ufrag, &sess->callee.pass);
    if (rc != PJ_SUCCESS) {
	app_perror(INDENT "err: callee pj_ice_strans_init_ice()", rc);
	rc = -110;
	goto on_return;
    }

    /* Start ICE on callee */
    rc = start_ice(&sess->callee, &sess->caller);
    if (rc != PJ_SUCCESS) {
	int retval = (rc == sess->callee.cfg.expected.start_status)?0:-120;
	rc = retval;
	goto on_return;
    }

    /* Start ICE on caller */
    rc = start_ice(&sess->caller, &sess->callee);
    if (rc != PJ_SUCCESS) {
	int retval = (rc == sess->caller.cfg.expected.start_status)?0:-130;
	rc = retval;
	goto on_return;
    }

    /* Start polling new candidate */
    //if (!sess->caller.last_cand || !sess->callee.last_cand)
    {
	pj_time_val timeout = {0, 10};

	pj_bzero(&timer_data, sizeof(timer_data));
	timer_data.sess = sess;

	pj_time_val_normalize(&timeout);
	pj_timer_entry_init(&te_new_cand, 0, &timer_data, &timer_new_cand);
	pj_timer_heap_schedule(stun_cfg->timer_heap, &te_new_cand, &timeout);
    }

    WAIT_UNTIL(30000, ALL_DONE, rc);
    if (!ALL_DONE) {
	PJ_LOG(3,(THIS_FILE, INDENT "err: negotiation timed-out"));
	rc = -140;
	goto on_return;
    }

    if (rc != 0)
	goto on_return;

    if (sess->caller.result.nego_status != sess->caller.cfg.expected.nego_status) {
	app_perror(INDENT "err: caller negotiation failed", sess->caller.result.nego_status);
	rc = -150;
	goto on_return;
    }

    if (sess->callee.result.nego_status != sess->callee.cfg.expected.nego_status) {
	app_perror(INDENT "err: callee negotiation failed", sess->callee.result.nego_status);
	rc = -160;
	goto on_return;
    }

    /* Verify that both agents have agreed on the same pair */
    rc = check_pair(&sess->caller, &sess->callee, -170);
    if (rc != 0) {
	goto on_return;
    }
    rc = check_pair(&sess->callee, &sess->caller, -180);
    if (rc != 0) {
	goto on_return;
    }

    /* Looks like everything is okay */

    /* Destroy ICE stream transports first to let it de-allocate
     * TURN relay (otherwise there'll be timer/memory leak, unless
     * we wait for long time in the last poll_events() below).
     */
    if (sess->caller.ice) {
	pj_ice_strans_destroy(sess->caller.ice);
	sess->caller.ice = NULL;
    }

    if (sess->callee.ice) {
	pj_ice_strans_destroy(sess->callee.ice);
	sess->callee.ice = NULL;
    }

on_return:
    /* Wait.. */
    poll_events(stun_cfg, 200, PJ_FALSE);

    /* Now destroy everything */
    destroy_sess(sess, 500);

    /* Flush events */
    poll_events(stun_cfg, 100, PJ_FALSE);

    if (rc == 0)
	rc = check_pjlib_state(stun_cfg, &pjlib_state);

    return rc;
}


/* Simple trickle ICE test */
int trickle_ice_test(void)
{
    pj_pool_t *pool;
    pj_stun_config stun_cfg;
    struct sess_param test_param;
    unsigned i;
    int rc;

    struct sess_cfg_t {
	const char	*title;
	unsigned	 server_flag;
	struct test_cfg	 ua1;
	struct test_cfg	 ua2;
    } cfg[] = {
    {
	"With host-only",
	0x1FFF,
	/*Role  comp# host? stun? turn? flag?        ans_del snd_del des_del */
	{ROLE1, 1,    YES,  NO,   NO,   CLIENT_IPV4, 0,      0,      0,      {PJ_SUCCESS, PJ_SUCCESS, PJ_SUCCESS}},
	{ROLE2, 1,    YES,  NO,   NO,   CLIENT_IPV4, 0,      0,      0,      {PJ_SUCCESS, PJ_SUCCESS, PJ_SUCCESS}}
    },
    {
	"With turn-only",
	0x1FFF,
	/*Role  comp# host? stun? turn? flag?        ans_del snd_del des_del */
	{ROLE1, 1,    NO,   NO,   YES,  CLIENT_IPV4, 0,      0,      0,      {PJ_SUCCESS, PJ_SUCCESS, PJ_SUCCESS}},
	{ROLE2, 1,    NO,   NO,   YES,  CLIENT_IPV4, 0,      0,      0,      {PJ_SUCCESS, PJ_SUCCESS, PJ_SUCCESS}}
    },
    {
	/* STUN candidates will be pruned */
	"With host+turn",
	0x1FFF,
	/*Role  comp# host? stun? turn? flag?        ans_del snd_del des_del */
	{ROLE1, 1,    YES,  YES,  YES,  CLIENT_IPV4, 0,      0,      0,      {PJ_SUCCESS, PJ_SUCCESS, PJ_SUCCESS}},
	{ROLE2, 1,    YES,  YES,  YES,  CLIENT_IPV4, 0,      0,      0,      {PJ_SUCCESS, PJ_SUCCESS, PJ_SUCCESS}}
    }};

    PJ_LOG(3,(THIS_FILE, "Trickle ICE"));
    pj_log_push_indent();

    pool = pj_pool_create(mem, NULL, 512, 512, NULL);

    rc = create_stun_config(pool, &stun_cfg);
    if (rc != PJ_SUCCESS) {
	pj_pool_release(pool);
	pj_log_pop_indent();
	return -10;
    }

    for (i = 0; i < PJ_ARRAY_SIZE(cfg) && !rc; ++i) {
	unsigned c1, c2;
	cfg[i].ua1.trickle = PJ_ICE_SESS_TRICKLE_FULL;
	cfg[i].ua2.trickle = PJ_ICE_SESS_TRICKLE_FULL;
	for (c1 = 1; c1 <= 2 && !rc; ++c1) {
	    for (c2 = 1; c2 <= 2 && !rc; ++c2) {
		pj_bzero(&test_param, sizeof(test_param));
		cfg[i].ua1.comp_cnt = c1;
		cfg[i].ua2.comp_cnt = c2;
		rc = perform_trickle_test(cfg[i].title,
					  &stun_cfg,
					  cfg[i].server_flag,
					  &cfg[i].ua1,
					  &cfg[i].ua2,
					  &test_param);
	    }
	}
    }

    destroy_stun_config(&stun_cfg);
    pj_pool_release(pool);
    pj_log_pop_indent();

    return rc;
}
