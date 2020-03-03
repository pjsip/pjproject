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
#include <pjlib.h>
#include <pjlib-util.h>
#include <pjnath.h>

#if defined(PJ_EXCLUDE_BENCHMARK_TESTS) && (PJ_EXCLUDE_BENCHMARK_TESTS==1)
#   define WITH_BENCHMARK	    0
#else
#   define WITH_BENCHMARK	    1
#endif

#define INCLUDE_STUN_TEST	    1
#define INCLUDE_ICE_TEST	    1
#define INCLUDE_STUN_SOCK_TEST	    1
#define INCLUDE_TURN_SOCK_TEST	    1
#define INCLUDE_CONCUR_TEST    	    1

#define GET_AF(use_ipv6) (use_ipv6?pj_AF_INET6():pj_AF_INET())

#if defined(PJ_HAS_IPV6) && PJ_HAS_IPV6
#   define USE_IPV6	1
#else
#   define USE_IPV6	0
#endif

#if defined(PJ_HAS_SSL_SOCK) && PJ_HAS_SSL_SOCK
#   define USE_TLS	1
#else
#   define USE_TLS	0
#endif

int stun_test(void);
int sess_auth_test(void);
int stun_sock_test(void);
int turn_sock_test(void);
int ice_test(void);
int concur_test(void);
int test_main(void);

extern void app_perror(const char *title, pj_status_t rc);
extern void app_set_sock_nb(pj_sock_t sock);
extern pj_pool_factory *mem;

int ice_one_conc_test(pj_stun_config *stun_cfg, int err_quit);

////////////////////////////////////
/*
 * Utilities
 */
pj_status_t create_stun_config(pj_pool_t *pool, pj_stun_config *stun_cfg);
void destroy_stun_config(pj_stun_config *stun_cfg);

void poll_events(pj_stun_config *stun_cfg, unsigned msec,
		 pj_bool_t first_event_only);

typedef struct pjlib_state
{
    unsigned	timer_cnt;	/* Number of timer entries */
    unsigned	pool_used_cnt;	/* Number of app pools	    */
} pjlib_state;


void capture_pjlib_state(pj_stun_config *cfg, struct pjlib_state *st);
int check_pjlib_state(pj_stun_config *cfg, 
		      const struct pjlib_state *initial_st);

pj_turn_tp_type get_turn_tp_type(pj_uint32_t flag);

#define ERR_MEMORY_LEAK	    1
#define ERR_TIMER_LEAK	    2

