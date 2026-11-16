/* 
 * Copyright (C) 2008-2026 Teluu Inc. (http://www.teluu.com)
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

/**
 * simple_pjsua.c
 *
 * This is a very simple but fully featured SIP user agent, with the 
 * following capabilities:
 *  - SIP registration
 *  - Making and receiving call
 *  - Audio/media to sound device.
 *
 * Usage:
 *  - To make outgoing call, start simple_pjsua with the URL of remote
 *    destination to contact.
 *    E.g.:
 *       simpleua sip:user@remote
 *
 *  - Incoming calls will automatically be answered with 200.
 *
 * Press 'h' to hang up all calls and 'q' to quit. On Zephyr, which has no
 * stdin, the program runs until it is reset, and the URL to call comes
 * from CONFIG_SIMPLE_PJSUA_CALL_URL.
 */

#include <pjsua-lib/pjsua.h>

#define THIS_FILE       "simple_pjsua.c"

/* The CONFIG_SIMPLE_PJSUA_ settings below can each be set from the build,
 * e.g. -DCONFIG_SIMPLE_PJSUA_SIP_DOMAIN='"pbx.example.com"'. On Zephyr they
 * are the sample's Kconfig options, which always define them, so the
 * defaults here apply elsewhere.
 */
#ifndef CONFIG_SIMPLE_PJSUA_SIP_DOMAIN
#  define CONFIG_SIMPLE_PJSUA_SIP_DOMAIN    "example.com"
#endif
#ifndef CONFIG_SIMPLE_PJSUA_USERNAME
#  define CONFIG_SIMPLE_PJSUA_USERNAME      "alice"
#endif
#ifndef CONFIG_SIMPLE_PJSUA_PASSWORD
#  define CONFIG_SIMPLE_PJSUA_PASSWORD      "secret"
#endif
/* The SIP UDP port to listen on. */
#ifndef CONFIG_SIMPLE_PJSUA_SIP_PORT
#  define CONFIG_SIMPLE_PJSUA_SIP_PORT      5060
#endif
/* Calls at once; PJSUA_MAX_CALLS is the most the library allows. */
#ifndef CONFIG_SIMPLE_PJSUA_MAX_CALLS
#  define CONFIG_SIMPLE_PJSUA_MAX_CALLS     PJSUA_MAX_CALLS
#endif
/* Log level on the console. */
#ifndef CONFIG_SIMPLE_PJSUA_LOG_LEVEL
#  define CONFIG_SIMPLE_PJSUA_LOG_LEVEL     4
#endif
/* The null sound device (1): the media runs on a clock with no audio
 * hardware, for a target without a sound device.
 */
#ifndef CONFIG_SIMPLE_PJSUA_NULL_SND_DEV
#  define CONFIG_SIMPLE_PJSUA_NULL_SND_DEV  0
#endif
/* URL to call at startup where there is no command line; "" calls nobody. */
#ifndef CONFIG_SIMPLE_PJSUA_CALL_URL
#  define CONFIG_SIMPLE_PJSUA_CALL_URL      ""
#endif

#define SIP_DOMAIN      CONFIG_SIMPLE_PJSUA_SIP_DOMAIN
#define SIP_USER        CONFIG_SIMPLE_PJSUA_USERNAME
#define SIP_PASSWD      CONFIG_SIMPLE_PJSUA_PASSWORD


/* Callback called by the library upon receiving incoming call */
static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
                             pjsip_rx_data *rdata)
{
    pjsua_call_info ci;

    PJ_UNUSED_ARG(acc_id);
    PJ_UNUSED_ARG(rdata);

    pjsua_call_get_info(call_id, &ci);

    PJ_LOG(3,(THIS_FILE, "Incoming call from %.*s!!",
                         (int)ci.remote_info.slen,
                         ci.remote_info.ptr));

    /* Automatically answer incoming calls with 200/OK */
    pjsua_call_answer(call_id, 200, NULL, NULL);
}

/* Callback called by the library when call's state has changed */
static void on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
    pjsua_call_info ci;

    PJ_UNUSED_ARG(e);

    pjsua_call_get_info(call_id, &ci);
    PJ_LOG(3,(THIS_FILE, "Call %d state=%.*s", call_id,
                         (int)ci.state_text.slen,
                         ci.state_text.ptr));
}

/* Callback called by the library when call's media state has changed */
static void on_call_media_state(pjsua_call_id call_id)
{
    pjsua_call_info ci;

    pjsua_call_get_info(call_id, &ci);

    if (ci.media_status == PJSUA_CALL_MEDIA_ACTIVE) {
        // When media is active, connect call to sound device.
        pjsua_conf_connect(ci.conf_slot, 0);
        pjsua_conf_connect(0, ci.conf_slot);
    }
}

/* Display error and exit application */
static void error_exit(const char *title, pj_status_t status)
{
    pjsua_perror(THIS_FILE, title, status);
    pjsua_destroy();
    exit(1);
}

/*
 * The program, with the URL to call, or NULL.
 */
static int simple_pjsua(const char *call_url)
{
    pjsua_acc_id acc_id;
    pj_status_t status;

    /* Create pjsua first! */
    status = pjsua_create();
    if (status != PJ_SUCCESS) error_exit("Error in pjsua_create()", status);

    /* If URL is specified, it's got to be a valid SIP URL */
    if (call_url) {
        status = pjsua_verify_url(call_url);
        if (status != PJ_SUCCESS) error_exit("Invalid URL to call", status);
    }

    /* Init pjsua */
    {
        pjsua_config cfg;
        pjsua_logging_config log_cfg;

        pjsua_config_default(&cfg);
        cfg.max_calls = CONFIG_SIMPLE_PJSUA_MAX_CALLS;
        cfg.cb.on_incoming_call = &on_incoming_call;
        cfg.cb.on_call_media_state = &on_call_media_state;
        cfg.cb.on_call_state = &on_call_state;

        pjsua_logging_config_default(&log_cfg);
        log_cfg.console_level = CONFIG_SIMPLE_PJSUA_LOG_LEVEL;

        status = pjsua_init(&cfg, &log_cfg, NULL);
        if (status != PJ_SUCCESS) error_exit("Error in pjsua_init()", status);
    }

    /* Add UDP transport. */
    {
        pjsua_transport_config cfg;

        pjsua_transport_config_default(&cfg);
        cfg.port = CONFIG_SIMPLE_PJSUA_SIP_PORT;
        status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &cfg, NULL);
        if (status != PJ_SUCCESS) error_exit("Error creating transport", status);
    }

    /* Initialization is done, now start pjsua */
    status = pjsua_start();
    if (status != PJ_SUCCESS) error_exit("Error starting pjsua", status);

#if CONFIG_SIMPLE_PJSUA_NULL_SND_DEV
    status = pjsua_set_null_snd_dev();
    if (status != PJ_SUCCESS) error_exit("Error setting null sound device", status);
#endif

    /* Register to SIP server by creating SIP account. */
    {
        pjsua_acc_config cfg;

        pjsua_acc_config_default(&cfg);
        cfg.id = pj_str("sip:" SIP_USER "@" SIP_DOMAIN);
        cfg.reg_uri = pj_str("sip:" SIP_DOMAIN);
        cfg.cred_count = 1;
        cfg.cred_info[0].realm = pj_str(SIP_DOMAIN);
        cfg.cred_info[0].scheme = pj_str("digest");
        cfg.cred_info[0].username = pj_str(SIP_USER);
        cfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
        cfg.cred_info[0].data = pj_str(SIP_PASSWD);

        status = pjsua_acc_add(&cfg, PJ_TRUE, &acc_id);
        if (status != PJ_SUCCESS) error_exit("Error adding account", status);
    }

    /* If URL is specified, make call to the URL. */
    if (call_url) {
        pj_str_t uri = pj_str((char*)call_url);
        status = pjsua_call_make_call(acc_id, &uri, 0, NULL, NULL, NULL);
        if (status != PJ_SUCCESS) error_exit("Error making call", status);
    }

#if defined(__ZEPHYR__)
    /* No stdin: the worker thread runs the calls until the board is reset */
    for (;;)
        pj_thread_sleep(1000);
#else
    /* Wait until user press "q" to quit. */
    for (;;) {
        char option[10];

        puts("Press 'h' to hangup all calls, 'q' to quit");
        if (fgets(option, sizeof(option), stdin) == NULL) {
            puts("EOF while reading stdin, will quit now..");
            break;
        }

        if (option[0] == 'q')
            break;

        if (option[0] == 'h')
            pjsua_call_hangup_all();
    }
#endif  /* __ZEPHYR__ */

    /* Destroy pjsua */
    pjsua_destroy();

    return 0;
}

#if defined(__ZEPHYR__)
int main(void)
{
    const char *url = CONFIG_SIMPLE_PJSUA_CALL_URL;
    return simple_pjsua(*url ? url : NULL);
}
#else
/*
 * main()
 *
 * argv[1] may contain URL to call.
 */
int main(int argc, char *argv[])
{
    return simple_pjsua(argc > 1 ? argv[1] : NULL);
}
#endif  /* __ZEPHYR__ */
