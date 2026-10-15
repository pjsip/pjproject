/*
 * Copyright (C) 2026 Teluu Inc. (http://www.teluu.com)
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

/*
 * pjsua-level tests for the presence subscription status.
 *
 * Scenarios covered:
 *   1. A buddy subscribed to the local account's presence receives a
 *      NOTIFY with a PIDF body, then a NOTIFY without body. The empty
 *      NOTIFY must leave the buddy's presence status unchanged and still
 *      readable.
 */

#include "test.h"
#include <pjsua-lib/pjsua.h>
#include <pjsua-lib/pjsua_internal.h>
#include <pjsip.h>
#include <pjlib.h>

#define THIS_FILE   "pjsua_pres_test.c"


/*****************************************************************************
 * Sub-test 1: empty_notify_test
 *****************************************************************************/
static int empty_notify_test(pjsua_transport_id tp_id, int port)
{
    pjsua_acc_config   acc_cfg;
    pjsua_buddy_config bud_cfg;
    pjsua_buddy_info   info;
    pjsua_acc_id       acc_id;
    pjsua_buddy_id     buddy_id;
    pjrpid_element     elem;
    pjsua_srv_pres    *uapres;
    pjsip_tx_data     *tdata = NULL;
    char               bud_uri[64];
    char               note_buf[1024];
    pj_str_t           note;
    pj_status_t        status;
    unsigned           elapsed;
    int                rc = 0;

    PJ_LOG(3, (THIS_FILE, "  pjsua pres: NOTIFY without body"));

    pj_bzero(&info, sizeof(info));

    pjsua_acc_config_default(&acc_cfg);
    acc_cfg.id       = pj_str("sip:pjsua-pres-test@pjsip.org");
    acc_cfg.register_on_acc_add = PJ_FALSE;
    acc_cfg.allow_contact_rewrite = PJ_FALSE;
    acc_cfg.transport_id = tp_id;

    status = pjsua_acc_add(&acc_cfg, PJ_TRUE, &acc_id);
    if (status != PJ_SUCCESS)
        return -3310;

    /* A note large enough to need its own allocation in the status pool */
    pj_memset(note_buf, 'n', sizeof(note_buf));
    note.ptr = note_buf;
    note.slen = sizeof(note_buf);

    pj_bzero(&elem, sizeof(elem));
    elem.type = PJRPID_ELEMENT_TYPE_PERSON;
    elem.activity = PJRPID_ACTIVITY_BUSY;
    elem.note = note;
    pjsua_acc_set_online_status2(acc_id, PJ_TRUE, &elem);

    pj_ansi_snprintf(bud_uri, sizeof(bud_uri),
                     "sip:pjsua-pres-test@127.0.0.1:%d", port);
    pjsua_buddy_config_default(&bud_cfg);
    bud_cfg.uri = pj_str(bud_uri);
    bud_cfg.subscribe = PJ_TRUE;

    status = pjsua_buddy_add(&bud_cfg, &buddy_id);
    if (status != PJ_SUCCESS) {
        pjsua_acc_del(acc_id);
        return -3320;
    }

    /* Wait for the first NOTIFY carrying the PIDF body */
    for (elapsed = 0; elapsed < 5000; elapsed += 50) {
        pjsua_handle_events(50);
        if (pjsua_buddy_get_info(buddy_id, &info) == PJ_SUCCESS &&
            info.status == PJSUA_BUDDY_STATUS_ONLINE)
        {
            break;
        }
    }
    if (info.status != PJSUA_BUDDY_STATUS_ONLINE) {
        rc = -3330;
        goto on_return;
    }

    /* Send a NOTIFY without body from the server subscription */
    PJSUA_LOCK();
    uapres = pjsua_var.acc[acc_id].pres_srv_list.next;
    if (uapres != &pjsua_var.acc[acc_id].pres_srv_list &&
        pjsip_pres_current_notify(uapres->sub, &tdata) == PJ_SUCCESS)
    {
        tdata->msg->body = NULL;
        status = pjsip_pres_send_request(uapres->sub, tdata);
    } else {
        status = PJ_ENOTFOUND;
    }
    PJSUA_UNLOCK();
    if (status != PJ_SUCCESS) {
        rc = -3340;
        goto on_return;
    }

    for (elapsed = 0; elapsed < 1000; elapsed += 50)
        pjsua_handle_events(50);

    /* Presence status is unchanged */
    status = pjsua_buddy_get_info(buddy_id, &info);
    if (status != PJ_SUCCESS || info.status != PJSUA_BUDDY_STATUS_ONLINE ||
        pj_strcmp(&info.status_text, &note) != 0)
    {
        PJ_LOG(3, (THIS_FILE, "    error: presence status changed"));
        rc = -3350;
    }

on_return:
    pjsua_buddy_del(buddy_id);
    for (elapsed = 0; elapsed < 500; elapsed += 50)
        pjsua_handle_events(50);
    pjsua_acc_del(acc_id);
    return rc;
}


/*****************************************************************************
 * Main entry point
 *****************************************************************************/
int pjsua_pres_test(void)
{
    pjsua_config           ua_cfg;
    pjsua_logging_config   log_cfg;
    pjsua_transport_config tp_cfg;
    pjsua_transport_id     tp_id;
    pj_uint16_t            port;
    pj_bool_t              old_tcp_switch;
    pj_status_t            status;
    int rc = 0;

    PJ_LOG(3, (THIS_FILE, "pjsua presence test"));

    /* The pjsip test framework's global endpoint owns the tsx layer
     * singleton, which pjsua would try to register on its own endpoint.
     * Destroy it here and recreate it via restore_test_endpt() afterwards.
     */
    pjsip_endpt_destroy(endpt);
    endpt = NULL;

    status = pjsua_create();
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "  pjsua_create failed (%d)", status));
        restore_test_endpt();
        return -3301;
    }

    pjsua_config_default(&ua_cfg);
    /* No worker threads: everything runs from pjsua_handle_events() */
    ua_cfg.thread_cnt = 0;

    pjsua_logging_config_default(&log_cfg);
    log_cfg.level         = 3;
    log_cfg.console_level = 3;

    status = pjsua_init(&ua_cfg, &log_cfg, NULL);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "  pjsua_init failed (%d)", status));
        pjsua_destroy();
        restore_test_endpt();
        return -3302;
    }

    pjsua_transport_config_default(&tp_cfg);
    tp_cfg.port = 0;  /* ephemeral */

    status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &tp_cfg, &tp_id);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "  pjsua_transport_create failed (%d)", status));
        pjsua_destroy();
        restore_test_endpt();
        return -3303;
    }

    status = pjsua_start();
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "  pjsua_start failed (%d)", status));
        pjsua_destroy();
        restore_test_endpt();
        return -3304;
    }

    {
        pjsua_transport_info ti;
        pjsua_transport_get_info(tp_id, &ti);
        port = pj_sockaddr_get_port(&ti.local_addr);
    }

    /* The PIDF NOTIFY is larger than the UDP threshold: keep it on UDP */
    old_tcp_switch = pjsip_cfg()->endpt.disable_tcp_switch;
    pjsip_cfg()->endpt.disable_tcp_switch = PJ_TRUE;

    /* ---- Run sub-tests ---- */
    rc = empty_notify_test(tp_id, (int)port);

    pjsip_cfg()->endpt.disable_tcp_switch = old_tcp_switch;

    pjsua_handle_events(500);
    pjsua_destroy2(PJSUA_DESTROY_NO_RX_MSG);

    restore_test_endpt();

    return rc;
}
