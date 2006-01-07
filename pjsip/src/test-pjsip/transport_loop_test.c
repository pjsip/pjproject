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

#include "test.h"
#include <pjsip_core.h>
#include <pjlib.h>

#define THIS_FILE   "transport_loop_test.c"

static int datagram_loop_test()
{
    pjsip_transport *loop;
    int i, pkt_lost;
    pj_sockaddr_in addr;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "testing datagram loop transport"));

    /* Test acquire transport. */
    status = pjsip_endpt_acquire_transport( endpt, PJSIP_TRANSPORT_LOOP_DGRAM,
					    &addr, sizeof(addr), &loop);
    if (status != PJ_SUCCESS) {
	app_perror("   error: loop transport is not configured", status);
	return -20;
    }

    /* Test basic transport attributes */
    status = generic_transport_test(loop);
    if (status != PJ_SUCCESS)
	return status;

    /* Basic transport's send/receive loopback test. */
    for (i=0; i<2; ++i) {
	status = transport_send_recv_test(PJSIP_TRANSPORT_LOOP_DGRAM, loop, 
					  "sip:bob@130.0.0.1;transport=loop-dgram");
	if (status != 0)
	    return status;
    }

    /* Multi-threaded round-trip test. */
    status = transport_rt_test(PJSIP_TRANSPORT_LOOP_DGRAM, loop, 
			       "sip:bob@130.0.0.1;transport=loop-dgram",
			       &pkt_lost);
    if (status != 0)
	return status;

    if (pkt_lost != 0) {
	PJ_LOG(3,(THIS_FILE, "   error: %d packet(s) was lost", pkt_lost));
	return -40;
    }

    /* Put delay. */
    PJ_LOG(3,(THIS_FILE,"  setting network delay to 10 ms"));
    pjsip_loop_set_delay(loop, 10);

    /* Multi-threaded round-trip test. */
    status = transport_rt_test(PJSIP_TRANSPORT_LOOP_DGRAM, loop, 
			       "sip:bob@130.0.0.1;transport=loop-dgram",
			       &pkt_lost);
    if (status != 0)
	return status;

    if (pkt_lost != 0) {
	PJ_LOG(3,(THIS_FILE, "   error: %d packet(s) was lost", pkt_lost));
	return -50;
    }

    /* Restore delay. */
    pjsip_loop_set_delay(loop, 0);

    /* Check that reference counter is one. */
    if (pj_atomic_get(loop->ref_cnt) != 1) {
	return -50;
    }

    /* Decrement reference. */
    pjsip_transport_dec_ref(loop);

    return 0;
}

int transport_loop_test(void)
{
    int status;

    status = datagram_loop_test();
    if (status != 0)
	return status;

    return 0;
}
