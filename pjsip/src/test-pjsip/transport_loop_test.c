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

static int datagram_loop_test()
{
    pjsip_transport *loop, *tp;
    pj_str_t s;
    int i, log_level;
    pj_sockaddr_in addr;
    pj_status_t status;

    PJ_LOG(3,("", "testing datagram loop transport"));

    /* Create loop transport. */
    status = pjsip_loop_start(endpt, &loop);
    if (status != PJ_SUCCESS) {
	app_perror("   error: unable to create datagram loop transport", 
		   status);
	return -10;
    }

    /* Create dummy address. */
    pj_sockaddr_in_init(&addr, pj_cstr(&s, "130.0.0.1"), TEST_UDP_PORT);

    /* Test acquire transport. */
    status = pjsip_endpt_acquire_transport( endpt, PJSIP_TRANSPORT_LOOP_DGRAM,
					    &addr, sizeof(addr), &tp);
    if (status != PJ_SUCCESS) {
	app_perror("   error: unable to acquire transport", status);
	return -20;
    }

    /* Check that this is the right transport. */
    if (tp != loop) {
	return -30;
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

    /* For multithreaded round-trip test to work, delay must be set
     * (otherwise functions will be called recursively until no memory is
     * left in the system)
     */

    /* Put delay. */
    pjsip_loop_set_delay(loop, 1, NULL);

    /* Multi-threaded round-trip test. */
    status = transport_rt_test(PJSIP_TRANSPORT_LOOP_DGRAM, tp, 
			       "sip:bob@130.0.0.1;transport=loop-dgram");
    if (status != 0)
	return status;


    /* Next test will test without delay.
     * This will stress-test the system.
     */
    PJ_LOG(3,("","  performing another multithreaded round-trip test..."));

    /* Remove delay. */
    pjsip_loop_set_delay(loop, 0, NULL);

    /* Ignore errors. */
    log_level = pj_log_get_level();
    pj_log_set_level(2);

    /* Multi-threaded round-trip test. */
    status = transport_rt_test(PJSIP_TRANSPORT_LOOP_DGRAM, tp, 
			       "sip:bob@130.0.0.1;transport=loop-dgram");
    if (status != 0)
	return status;

    /* Restore log level. */
    pj_log_set_level(log_level);

    /* Check that reference counter is one. */
    if (pj_atomic_get(loop->ref_cnt) != 1) {
	return -30;
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
