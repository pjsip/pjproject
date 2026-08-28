/*
 * footprint_app.c -- minimal but complete PJSUA application used as the
 * link target for the footprint measurement (see ../footprint-report.md).
 *
 * It references every feature the measured configuration uses -- SIP over
 * TLS (mbedTLS ssl_sock), SDES-SRTP, G.711/G.722 via the conference
 * bridge, null audio device -- so that a --gc-sections link keeps exactly
 * the code a real minimal endpoint would carry, and nothing else.
 *
 * Usage (not needed for the size measurement, but the app is runnable):
 *   footprint_app [dst-uri [cert-file privkey-file]]
 */
#include <stdio.h>
#include <pjsua-lib/pjsua.h>

#define THIS_FILE       "footprint_app.c"

static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
                             pjsip_rx_data *rdata)
{
    PJ_UNUSED_ARG(acc_id);
    PJ_UNUSED_ARG(rdata);
    pjsua_call_answer(call_id, 200, NULL, NULL);
}

static void on_call_media_state(pjsua_call_id call_id)
{
    pjsua_call_info ci;

    pjsua_call_get_info(call_id, &ci);
    if (ci.media_status == PJSUA_CALL_MEDIA_ACTIVE) {
        pjsua_conf_connect(ci.conf_slot, 0);
        pjsua_conf_connect(0, ci.conf_slot);
    }
}

static void on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
    PJ_UNUSED_ARG(call_id);
    PJ_UNUSED_ARG(e);
}

int main(int argc, char *argv[])
{
    pjsua_config cfg;
    pjsua_logging_config log_cfg;
    pjsua_media_config media_cfg;
    pjsua_transport_config tp_cfg;
    pjsua_acc_id acc_id;
    pjsua_transport_id tp_id;
    pj_status_t status;

    status = pjsua_create();
    if (status != PJ_SUCCESS)
        return 1;

    pjsua_config_default(&cfg);
    cfg.max_calls = PJSUA_MAX_CALLS;
    cfg.cb.on_incoming_call = &on_incoming_call;
    cfg.cb.on_call_media_state = &on_call_media_state;
    cfg.cb.on_call_state = &on_call_state;
    cfg.use_srtp = PJMEDIA_SRTP_MANDATORY;
    cfg.srtp_secure_signaling = 1;

    pjsua_logging_config_default(&log_cfg);
    log_cfg.console_level = 4;

    pjsua_media_config_default(&media_cfg);
    media_cfg.clock_rate = 16000;

    status = pjsua_init(&cfg, &log_cfg, &media_cfg);
    if (status != PJ_SUCCESS)
        return 1;

    pjsua_transport_config_default(&tp_cfg);
    tp_cfg.port = 5061;
    if (argc > 3) {
        tp_cfg.tls_setting.cert_file = pj_str(argv[2]);
        tp_cfg.tls_setting.privkey_file = pj_str(argv[3]);
    }
    status = pjsua_transport_create(PJSIP_TRANSPORT_TLS, &tp_cfg, &tp_id);
    if (status != PJ_SUCCESS)
        return 1;

    status = pjsua_acc_add_local(tp_id, PJ_TRUE, &acc_id);
    if (status != PJ_SUCCESS)
        return 1;

    status = pjsua_start();
    if (status != PJ_SUCCESS)
        return 1;

    pjsua_set_null_snd_dev();

    if (argc > 1) {
        pj_str_t uri = pj_str(argv[1]);
        pjsua_call_make_call(acc_id, &uri, NULL, NULL, NULL, NULL);
    }

    for (;;) {
        char line[16];

        if (fgets(line, sizeof(line), stdin) == NULL)
            break;
        if (line[0] == 'q')
            break;
    }

    pjsua_call_hangup_all();
    pjsua_destroy();
    return 0;
}
