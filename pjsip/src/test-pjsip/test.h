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
#ifndef __TEST_H__
#define __TEST_H__

#include <pjsip/sip_types.h>

extern pjsip_endpoint *endpt;

#define TEST_UDP_PORT	    15060
#define TEST_UDP_PORT_STR   "15060"

/* The tests */
int uri_test(void);
int msg_test(void);
int txdata_test(void);
int transport_udp_test(void);
int transport_loop_test(void);
int tsx_basic_test(void);
int tsx_uac_test(void);
int tsx_uas_test(void);

/* Transport test helpers (transport_test.c). */
int generic_transport_test(pjsip_transport *tp);
int transport_send_recv_test( pjsip_transport_type_e tp_type,
			      pjsip_transport *ref_tp,
			      char *target_url );
int transport_rt_test( pjsip_transport_type_e tp_type,
		       pjsip_transport *ref_tp,
		       char *target_url,
		       int *pkt_lost);

/* Test main entry */
int  test_main(void);

/* Test utilities. */
void app_perror(const char *msg, pj_status_t status);
int init_msg_logger(void);
int msg_logger_set_enabled(pj_bool_t enabled);
void flush_events(unsigned duration);

/* Settings. */
extern int log_level;

#endif	/* __TEST_H__ */
