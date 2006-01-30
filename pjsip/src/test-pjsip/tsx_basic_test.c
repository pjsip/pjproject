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
#include <pjsip.h>
#include <pjlib.h>

#define THIS_FILE   "tsx_basic_test.c"

/* Test transaction layer. */
static int tsx_layer_test(void)
{
    pj_str_t target, from, tsx_key;
    pjsip_tx_data *tdata;
    pjsip_transaction *tsx, *found;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "  transaction layer test"));

    target = pj_str("sip:alice@localhost");
    from = pj_str("sip:bob@localhost");

    status = pjsip_endpt_create_request(endpt, &pjsip_invite_method, &target,
					&from, &target, NULL, NULL, -1, NULL,
					&tdata);
    if (status != PJ_SUCCESS) {
	app_perror("  error: unable to create request", status);
	return -110;
    }

    status = pjsip_tsx_create_uac(NULL, tdata, &tsx);
    if (status != PJ_SUCCESS) {
	app_perror("   error: unable to create transaction", status);
	return -120;
    }

    pj_strdup(tdata->pool, &tsx_key, &tsx->transaction_key);

    found = pjsip_tsx_layer_find_tsx(&tsx_key, PJ_FALSE);
    if (found != tsx) {
	return -130;
    }

    pjsip_tsx_terminate(tsx, PJSIP_SC_REQUEST_TERMINATED);
    flush_events(500);

    if (pjsip_tx_data_dec_ref(tdata) != PJSIP_EBUFDESTROYED) {
	return -140;
    }

    return 0;
}

/* Double terminate test. */
static int double_terminate(void)
{
    pj_str_t target, from, tsx_key;
    pjsip_tx_data *tdata;
    pjsip_transaction *tsx;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "  double terminate test"));

    target = pj_str("sip:alice@localhost;transport=loop-dgram");
    from = pj_str("sip:bob@localhost");

    /* Create request. */
    status = pjsip_endpt_create_request(endpt, &pjsip_invite_method, &target,
					&from, &target, NULL, NULL, -1, NULL,
					&tdata);
    if (status != PJ_SUCCESS) {
	app_perror("  error: unable to create request", status);
	return -10;
    }

    /* Create transaction. */
    status = pjsip_tsx_create_uac(NULL, tdata, &tsx);
    if (status != PJ_SUCCESS) {
	app_perror("   error: unable to create transaction", status);
	return -20;
    }

    /* Save transaction key for later. */
    pj_strdup_with_null(tdata->pool, &tsx_key, &tsx->transaction_key);

    /* Add reference to transmit buffer (tsx_send_msg() will dec txdata). */
    pjsip_tx_data_add_ref(tdata);

    /* Send message to start timeout timer. */
    status = pjsip_tsx_send_msg(tsx, NULL);

    /* Terminate transaction. */
    status = pjsip_tsx_terminate(tsx, PJSIP_SC_REQUEST_TERMINATED);
    if (status != PJ_SUCCESS) {
	app_perror("   error: unable to terminate transaction", status);
	return -30;
    }

    tsx = pjsip_tsx_layer_find_tsx(&tsx_key, PJ_TRUE);
    if (tsx) {
	/* Terminate transaction again. */
	pjsip_tsx_terminate(tsx, PJSIP_SC_REQUEST_TERMINATED);
	if (status != PJ_SUCCESS) {
	    app_perror("   error: unable to terminate transaction", status);
	    return -40;
	}
	pj_mutex_unlock(tsx->mutex);
    }

    flush_events(500);
    if (pjsip_tx_data_dec_ref(tdata) != PJSIP_EBUFDESTROYED) {
	return -50;
    }

    return PJ_SUCCESS;
}

int tsx_basic_test(void)
{
    int status;

    status = tsx_layer_test();
    if (status != 0)
	return status;

    status = double_terminate();
    if (status != 0)
	return status;

    return 0;
}
