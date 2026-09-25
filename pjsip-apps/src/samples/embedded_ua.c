/*
 * Copyright (C) 2008-2026 Teluu Inc. (http://www.teluu.com)
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
 * embedded_ua.c
 *
 * A SIP user agent built directly on pjsip and pjmedia, i.e. without
 * pjsua-lib, and without any console input, so that it can be ported to
 * constrained targets. Controllable via MESSAGE requests.
 * Everything is configured by compile time macros.
 *
 * Specification:
 *  - Up to MAX_CALLS (4) simultaneous calls, each with its own media
 *    transport and audio stream. No conference bridge.
 *  - Audio is looped back: every frame read from the stream is written
 *    straight back to it, so the peer hears itself. No sound device, no
 *    files.
 *  - One audio codec, G.711 (PCMU and PCMA) or G.722, selected by
 *    EMBUA_AUDIO_CODEC, plus the RFC 4733 telephone-event that pjmedia puts
 *    in the SDP. No video.
 *  - SIP over UDP or TLS, selected by EMBUA_SIP_TRANSPORT; TLS when the
 *    stack has it (which also requires PJ_HAS_TCP). The TLS transport has
 *    no certificate: it works as a client towards the registrar, and
 *    incoming requests then arrive over that same connection.
 *  - Mandatory SDES-SRTP when EMBUA_USE_SRTP is set, the default whenever
 *    pjmedia has SRTP: the SDP offers/answers RTP/SAVP with a=crypto, and
 *    an offer without SRTP is rejected with 488. Otherwise plain RTP/AVP.
 *  - With EMBUA_USE_MEDIA_THREAD, the default when pjlib has threads, a
 *    media thread polls pjmedia's own ioqueue and runs the media clock, so
 *    audio keeps flowing while the main thread is busy in a TLS handshake
 *    or a blocking name lookup. Without it everything runs in main(), and
 *    the application builds with PJ_HAS_THREADS 0.
 *  - With EMBUA_USE_REGISTRATION, the default, registers to SIP_SERVER_IP
 *    with digest credentials, and retries when registration fails or its
 *    connection drops. Without it the UA is reached at its own address.
 *  - Incoming calls are answered automatically, with 180 then 200. An
 *    incoming call beyond MAX_CALLS is rejected with 486.
 *  - An incoming MESSAGE (RFC 3428) carries the only commands the
 *    application takes: "call <uri>", e.g. "call sip:alice@192.168.0.2",
 *    places an outgoing call, "mem" dumps the caching pool state (needs
 *    PJ_LOG_MAX_LEVEL >= 3), and "quit" disconnects all calls, unregisters,
 *    and exits. Any other request outside a dialog is answered with 400.
 */

/* Include all headers. */
#include <pjsip.h>
#include <pjmedia.h>
#include <pjmedia-codec.h>
#include <pjsip_ua.h>
#include <pjlib-util.h>
#include <pjlib.h>

#include <stdlib.h>

#define THIS_FILE           "embedded_ua.c"

/* Zephyr's Kconfig setting. Define it here for non Zephyr. */
#ifndef __ZEPHYR__
#  define CONFIG_PJPROJECT_TLS      1
#endif

/* Values for EMBUA_SIP_TRANSPORT, also used as the port to listen on. */
#define SIP_UDP             55060
#define SIP_TLS             55061

/* Values for EMBUA_AUDIO_CODEC. */
#define CODEC_G711          3
#define CODEC_G722          4

/* The EMBUA_ settings below pick what gets built in. Each follows what the
 * libraries provide by default, and each can be overridden from the build,
 * e.g. -DEMBUA_USE_SRTP=0.
 */

/* Audio codec: G.711 (PCMU and PCMA) or G.722. */
#ifndef EMBUA_AUDIO_CODEC
#  if defined(PJMEDIA_HAS_G711_CODEC) && PJMEDIA_HAS_G711_CODEC!=0
#    define EMBUA_AUDIO_CODEC       CODEC_G711
#  elif defined(PJMEDIA_HAS_G722_CODEC) && PJMEDIA_HAS_G722_CODEC!=0
#    define EMBUA_AUDIO_CODEC       CODEC_G722
#  else
#    define EMBUA_AUDIO_CODEC       0   /* none: see main() at the end */
#  endif
#endif

#if EMBUA_AUDIO_CODEC==CODEC_G711 && \
    (!defined(PJMEDIA_HAS_G711_CODEC) || PJMEDIA_HAS_G711_CODEC==0)
#  error EMBUA_AUDIO_CODEC is G.711 but PJMEDIA_HAS_G711_CODEC is disabled
#elif EMBUA_AUDIO_CODEC==CODEC_G722 && \
    (!defined(PJMEDIA_HAS_G722_CODEC) || PJMEDIA_HAS_G722_CODEC==0)
#  error EMBUA_AUDIO_CODEC is G.722 but PJMEDIA_HAS_G722_CODEC is disabled
#endif

/* Mandatory SDES-SRTP, or plain RTP when 0. */
#ifndef EMBUA_USE_SRTP
#  if defined(PJMEDIA_HAS_SRTP) && PJMEDIA_HAS_SRTP!=0 && \
      defined(PJMEDIA_SRTP_HAS_SDES) && PJMEDIA_SRTP_HAS_SDES!=0
#    define EMBUA_USE_SRTP          1
#  else
#    define EMBUA_USE_SRTP          0
#  endif
#endif

#if EMBUA_USE_SRTP && (!defined(PJMEDIA_HAS_SRTP) || PJMEDIA_HAS_SRTP==0 || \
                       !defined(PJMEDIA_SRTP_HAS_SDES) || \
                       PJMEDIA_SRTP_HAS_SDES==0)
#  error EMBUA_USE_SRTP requires PJMEDIA_HAS_SRTP and PJMEDIA_SRTP_HAS_SDES
#endif

/* SIP transport, SIP_UDP or SIP_TLS. TLS runs over TCP, so it also needs
 * PJ_HAS_TCP.
 */
#ifndef EMBUA_SIP_TRANSPORT
#  if defined(PJSIP_HAS_TLS_TRANSPORT) && PJSIP_HAS_TLS_TRANSPORT!=0 && \
      defined(PJ_HAS_TCP) && PJ_HAS_TCP!=0 && \
      defined(CONFIG_PJPROJECT_TLS) && CONFIG_PJPROJECT_TLS!=0
#    define EMBUA_SIP_TRANSPORT     SIP_TLS
#  else
#    define EMBUA_SIP_TRANSPORT     SIP_UDP
#  endif
#endif

#if EMBUA_SIP_TRANSPORT==SIP_TLS
#  if !defined(PJSIP_HAS_TLS_TRANSPORT) || PJSIP_HAS_TLS_TRANSPORT==0 || \
      !defined(PJ_HAS_TCP) || PJ_HAS_TCP==0
#    error SIP_TLS requires PJSIP_HAS_TLS_TRANSPORT and PJ_HAS_TCP
#  endif
#  if !defined(CONFIG_PJPROJECT_TLS) || CONFIG_PJPROJECT_TLS==0
#    error SIP_TLS requires CONFIG_PJPROJECT_TLS in Kconfig
#  endif
#  define SIP_TRANSPORT_PARAM       ";transport=tls"
#elif EMBUA_SIP_TRANSPORT==SIP_UDP
#  define SIP_TRANSPORT_PARAM       ""
#else
#  error EMBUA_SIP_TRANSPORT must be SIP_UDP or SIP_TLS
#endif

/* Run the media from its own thread, which polls pjmedia's ioqueue, so that
 * neither incoming RTP nor the media clock waits for the main thread.
 */
#ifndef EMBUA_USE_MEDIA_THREAD
#  if PJ_HAS_THREADS
#    define EMBUA_USE_MEDIA_THREAD  1
#  else
#    define EMBUA_USE_MEDIA_THREAD  0
#  endif
#endif

#if EMBUA_USE_MEDIA_THREAD && !PJ_HAS_THREADS
#  error EMBUA_USE_MEDIA_THREAD requires PJ_HAS_THREADS
#endif

/* Register to SIP_SERVER_IP. With 0 the UA is reached at its own address. */
#ifndef EMBUA_USE_REGISTRATION
#  define EMBUA_USE_REGISTRATION    1
#endif

#if EMBUA_AUDIO_CODEC

#define SIP_PORT            EMBUA_SIP_TRANSPORT
#define RTP_START_PORT      44000
#define MAX_CALLS           4

/* PCM loopback buffer size; worst case is G.722, 16 kHz mono 20 ms. */
#if EMBUA_AUDIO_CODEC==CODEC_G711
#  define MAX_FRAME_SAMPLES   160
#  define CODEC_CLOCK_RATE    8000
#else
#  define MAX_FRAME_SAMPLES   320
#  define CODEC_CLOCK_RATE    16000
#endif

/* One frame time, in milliseconds, for the configured codec. The clock is
 * paced per call from the negotiated ptime; this is only the fallback used
 * while no call has media.
 */
#define FRAME_PTIME_MSEC    (MAX_FRAME_SAMPLES * 1000 / CODEC_CLOCK_RATE)

/* Requested registration expiration, in seconds. */
#define SIP_REG_TIMEOUT     (5*60)

/* Delay before retrying a registration that failed or lost its connection. */
#define SIP_REG_RETRY_TIMEOUT   10

/* How long to wait, at most, for the BYE and un-REGISTER transactions to
 * complete before exiting anyway.
 */
#define SIP_SHUTDOWN_TIMEOUT    5

/* Longest the main loop blocks in pjsip_endpt_handle_events(). Every SIP
 * timer is scheduled from this thread and handle_events() already returns
 * as soon as the nearest one is due, so this only bounds how late the
 * REGISTER scheduled in g.reg_due can be. Keep it long: with a media
 * thread the main loop has nothing else to wake up for.
 */
#define MAX_POLL_MSEC       1000

/* Longest the media clock waits when no call has a frame due.
 *
 * With a media thread this cannot be made large: pj_ioqueue_poll() fixes
 * its descriptor set on entry, so a transport registered by the SIP thread
 * while the media thread is already blocked stays invisible until the poll
 * returns, and the first frames of a new call would be that late. One frame
 * time bounds the delay at the price of an idle tick.
 *
 * Without a media thread the same loop registers the transport and polls,
 * so there is nothing to miss and the cap can be the main loop's.
 */
#if EMBUA_USE_MEDIA_THREAD
#  define MEDIA_IDLE_POLL_MSEC  FRAME_PTIME_MSEC
#else
#  define MEDIA_IDLE_POLL_MSEC  MAX_POLL_MSEC
#endif

#define SIP_SERVER_IP       "192.168.0.7"
#define SIP_REGISTRAR       "sip:" SIP_SERVER_IP SIP_TRANSPORT_PARAM
#define SIP_AOR             "sip:" AUTH_USERNAME "@" SIP_SERVER_IP
#define AUTH_USERNAME       "bob"
#define AUTH_PASSWD         "secret"


static struct global_t {
    pj_bool_t             complete;
    pjsip_endpoint       *endpt;
    pj_caching_pool       cp;
    pjmedia_endpt        *med_endpt;
    pjmedia_event_mgr    *event_mgr;
    pj_pool_t            *pool;
#if EMBUA_USE_REGISTRATION
    pjsip_regc           *regc;
    pj_time_val           reg_due;   /* next REGISTER, {0,0} if none due */
    pj_bool_t             unregistering; /* un-REGISTER is in flight  */
#endif
#if EMBUA_USE_MEDIA_THREAD
    pj_thread_t          *media_thread;
    pj_mutex_t           *media_lock;  /* guards the media clock state */
    volatile pj_bool_t    media_quit;
#endif
} g;

/* Serialise the media clock against the SIP thread, which arms and tears
 * down streams from the INVITE callbacks. Only the med_port/med_stream pair
 * needs it: the ioqueue is polled outside the lock, so closing a transport
 * never waits for a thread that is blocked here.
 */
#if EMBUA_USE_MEDIA_THREAD
#  define MEDIA_LOCK()      do { if (g.media_lock) \
                                     pj_mutex_lock(g.media_lock); } while (0)
#  define MEDIA_UNLOCK()    do { if (g.media_lock) \
                                     pj_mutex_unlock(g.media_lock); } while (0)
#else
#  define MEDIA_LOCK()      do {} while (0)
#  define MEDIA_UNLOCK()    do {} while (0)
#endif

static struct call_t
{
    pj_pool_t               *pool;
    pjsip_inv_session       *inv;
    pjmedia_stream          *med_stream;

    /* Media clock, driven by media_poll(). med_port is the stream's port,
     * and is NULL exactly when the call has no media.
     */
    pjmedia_port            *med_port;
    unsigned                 samples_per_frame;
    pj_uint32_t              ts_per_frame; /* one ptime, in timestamp units */
    pj_timestamp             next_tick;    /* when the next frame is due   */

    pjmedia_transport_info  med_tpinfo;
    pjmedia_transport       *med_transport;
    pjmedia_sock_info       med_sock_info;
} g_calls[MAX_CALLS];

/* MESSAGE method (RFC 3428) */
static const pjsip_method message_method =
{
    PJSIP_OTHER_METHOD,
    { "MESSAGE", 7 }
};

static void call_on_media_update( pjsip_inv_session *inv, pj_status_t status);
static void call_on_state_changed( pjsip_inv_session *inv, pjsip_event *e);
static void call_on_forked(pjsip_inv_session *inv, pjsip_event *e);
static pj_bool_t on_rx_request( pjsip_rx_data *rdata );
static void deinit_call(struct call_t *call);
#if EMBUA_USE_REGISTRATION
static void send_unregister(void);
#endif
static unsigned media_poll(void);
static unsigned active_calls(void);
#if EMBUA_USE_MEDIA_THREAD
static int media_thread_proc(void *arg);
#endif


static pjsip_module mod_embedded_ua =
{
    NULL, NULL,                     /* prev, next.              */
    { "mod-embedded-ua", 15 },      /* Name.                    */
    -1,                             /* Id                       */
    PJSIP_MOD_PRIORITY_APPLICATION, /* Priority                 */
    NULL,                           /* load()                   */
    NULL,                           /* start()                  */
    NULL,                           /* stop()                   */
    NULL,                           /* unload()                 */
    &on_rx_request,                 /* on_rx_request()          */
    NULL,                           /* on_rx_response()         */
    NULL,                           /* on_tx_request.           */
    NULL,                           /* on_tx_response()         */
    NULL,                           /* on_tsx_state()           */
};


static pj_bool_t logging_on_rx_msg(pjsip_rx_data *rdata)
{
    PJ_LOG(4,(THIS_FILE, "RX %d bytes %s from %s %s:%d:\n"
                         "%.*s\n"
                         "--end msg--",
                         rdata->msg_info.len,
                         pjsip_rx_data_get_info(rdata),
                         rdata->tp_info.transport->type_name,
                         rdata->pkt_info.src_name,
                         rdata->pkt_info.src_port,
                         (int)rdata->msg_info.len,
                         rdata->msg_info.msg_buf));
    return PJ_FALSE;
}

static pj_status_t logging_on_tx_msg(pjsip_tx_data *tdata)
{
    PJ_LOG(4,(THIS_FILE, "TX %ld bytes %s to %s %s:%d:\n"
                         "%.*s\n"
                         "--end msg--",
                         (long)(tdata->buf.cur - tdata->buf.start),
                         pjsip_tx_data_get_info(tdata),
                         tdata->tp_info.transport->type_name,
                         tdata->tp_info.dst_name,
                         tdata->tp_info.dst_port,
                         (int)(tdata->buf.cur - tdata->buf.start),
                         tdata->buf.start));
    return PJ_SUCCESS;
}

/* The module instance. */
static pjsip_module msg_logger =
{
    NULL, NULL,                         /* prev, next.          */
    { "mod-msg-log", 13 },              /* Name.                */
    -1,                                 /* Id                   */
    PJSIP_MOD_PRIORITY_TRANSPORT_LAYER-1,/* Priority            */
    NULL,                               /* load()               */
    NULL,                               /* start()              */
    NULL,                               /* stop()               */
    NULL,                               /* unload()             */
    &logging_on_rx_msg,                 /* on_rx_request()      */
    &logging_on_rx_msg,                 /* on_rx_response()     */
    &logging_on_tx_msg,                 /* on_tx_request.       */
    &logging_on_tx_msg,                 /* on_tx_response()     */
    NULL,                               /* on_tsx_state()       */

};

#define CHKS(expr, ret)  if ((expr)!=PJ_SUCCESS) app_exit(ret);

static int app_perror( const char *sender, const char *title,
                       pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));

    PJ_LOG(3,(sender, "%s: %s [code=%d]", title, errmsg, status));
    return 1;
}

#ifdef __ZEPHYR__
#include <zephyr/sys/printk.h>
static void log_printk(int level, const char *data, int len)
{
    PJ_UNUSED_ARG(level);
    PJ_UNUSED_ARG(len);
    printk("%s", data);
}
#endif

static void deinit(void)
{
    int i;

#if EMBUA_USE_MEDIA_THREAD
    /* Stop the media thread before anything it touches goes away. */
    if (g.media_thread) {
        g.media_quit = PJ_TRUE;
        pj_thread_join(g.media_thread);
        pj_thread_destroy(g.media_thread);
        g.media_thread = NULL;
    }
#endif

    for (i=0; i<MAX_CALLS; ++i)
        deinit_call(&g_calls[i]);

#if EMBUA_USE_MEDIA_THREAD
    if (g.media_lock) {
        pj_mutex_destroy(g.media_lock);
        g.media_lock = NULL;
    }
#endif

#if EMBUA_USE_REGISTRATION
    if (g.regc) {
        pjsip_regc_destroy(g.regc);
    }
#endif

    if (g.event_mgr) {
        pjmedia_event_mgr_destroy(g.event_mgr);
        g.event_mgr = NULL;
    }

#if EMBUA_AUDIO_CODEC==CODEC_G711
    pjmedia_codec_g711_deinit();
#else
    pjmedia_codec_g722_deinit();
#endif

    if (g.med_endpt) {
        pjmedia_endpt_destroy(g.med_endpt);
    }

    if (g.endpt) {
        pjsip_endpt_destroy(g.endpt);
    }

    if (g.pool) {
        pj_pool_release(g.pool);
    }

    pj_caching_pool_destroy(&g.cp);
    pj_bzero(&g, sizeof(g));
    pj_bzero(g_calls, sizeof(g_calls));

    pj_shutdown();
}

static void app_exit(int ret)
{
    PJ_LOG(1,(THIS_FILE, "Error: %d", ret));
    deinit();
    exit(ret);
}

/* Disconnect all active calls, drop the registration, and ask the main loop
 * to stop. The requests sent here are answered during the shutdown loop in
 * main(), which is what actually ends the application.
 */
static void app_quit(void)
{
    int i;

    PJ_LOG(3,(THIS_FILE, "Quitting.."));

    for (i=0; i<MAX_CALLS; ++i) {
        pjsip_inv_session *inv = g_calls[i].inv;
        pjsip_tx_data *tdata;

        if (inv == NULL)
            continue;

        if (pjsip_inv_end_session(inv, PJSIP_SC_OK, NULL, &tdata)==PJ_SUCCESS
            && tdata)
        {
            pjsip_inv_send_msg(inv, tdata);
        }
    }

#if EMBUA_USE_REGISTRATION
    /* Drop our binding, so that the registrar does not keep routing calls to
     * an endpoint that is gone until the registration expires.
     */
    send_unregister();
#endif

    g.complete = PJ_TRUE;
}

/* Digest credential, shared by the registration and the outgoing calls. */
static void init_cred(pjsip_cred_info *cred)
{
    pj_bzero(cred, sizeof(*cred));
    cred->realm = pj_str("*");
    cred->scheme = pj_str("digest");
    cred->username = pj_str(AUTH_USERNAME);
    cred->data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
    cred->data = pj_str(AUTH_PASSWD);
}

#if EMBUA_USE_REGISTRATION
/* Ask the main loop to send a REGISTER in \a delay seconds. */
static void schedule_register(unsigned delay)
{
    pj_gettimeofday(&g.reg_due);
    g.reg_due.sec += delay;
    PJ_LOG(3,(THIS_FILE, "REGISTER scheduled in %u second(s)", delay));
}

/* Send REGISTER now. Once it succeeds, pjsip_regc refreshes the registration
 * by itself, so this is only used for the first attempt and for retries.
 */
static void send_register(void)
{
    pjsip_tx_data *tdata;
    pj_status_t status;

    pj_bzero(&g.reg_due, sizeof(g.reg_due));

    status = pjsip_regc_register(g.regc, PJ_TRUE, &tdata);
    if (status == PJ_SUCCESS)
        status = pjsip_regc_send(g.regc, tdata);

    if (status != PJ_SUCCESS) {
        app_perror(THIS_FILE, "Unable to send REGISTER", status);
        schedule_register(SIP_REG_RETRY_TIMEOUT);
    }
}

/* Send un-REGISTER, i.e. a REGISTER with Expires:0, and cancel any pending
 * registration work. pjsip_regc_unregister() also cancels the refresh timer,
 * so no new REGISTER can follow this one.
 */
static void send_unregister(void)
{
    pjsip_tx_data *tdata;
    pj_status_t status;

    pj_bzero(&g.reg_due, sizeof(g.reg_due));

    if (g.regc == NULL)
        return;

    status = pjsip_regc_unregister(g.regc, &tdata);
    if (status == PJ_SUCCESS) {
        /* Set before sending: pjsip_regc_send() may invoke regc_cb() from
         * within the call, e.g. when the request cannot be sent at all.
         */
        g.unregistering = PJ_TRUE;
        status = pjsip_regc_send(g.regc, tdata);
    }

    if (status != PJ_SUCCESS) {
        /* pjsip_regc_send() does not guarantee a callback when it fails, so
         * clear the flag here rather than waiting for one.
         */
        g.unregistering = PJ_FALSE;
        app_perror(THIS_FILE, "Unable to send un-REGISTER", status);
    } else {
        PJ_LOG(3,(THIS_FILE, "Unregistering.."));
    }
}

static void regc_cb(struct pjsip_regc_cbparam *param)
{
    /* Final response to the un-REGISTER sent by app_quit(). Whether or not
     * the registrar accepted it, this is as far as we can get, so release
     * the shutdown loop instead of scheduling a retry.
     */
    if (g.unregistering) {
        PJ_LOG(3,(THIS_FILE, "Un-REGISTER status: %d", param->code));
        g.unregistering = PJ_FALSE;
        return;
    }

    /* On success pjsip_regc schedules the refresh itself. On failure it does
     * not, so come back later rather than staying unregistered forever.
     */
    if (param->code/100 == 2) {
        PJ_LOG(3,(THIS_FILE, "REGISTER status: %d, expires in %u second(s)",
                  param->code, param->expiration));
    } else {
        PJ_LOG(3,(THIS_FILE, "REGISTER status: %d", param->code));
        schedule_register(SIP_REG_RETRY_TIMEOUT);
    }
}

/* Global transport state notification.
 *
 * pjsip_regc holds a reference to the transport that carries the
 * registration, which is what keeps a TCP/TLS connection from being closed
 * by PJSIP_TRANSPORT_IDLE_TIME between two REGISTERs. When the connection is
 * closed from the network side that reference must be released, and we need
 * a new registration to establish a new connection.
 */
static void on_tp_state_changed(pjsip_transport *tp,
                                pjsip_transport_state state,
                                const pjsip_transport_state_info *info)
{
    pjsip_regc_info reg_info;

    PJ_UNUSED_ARG(info);

    /* Shutting down: the registration is being dropped on purpose, and a
     * transport going away now must not schedule a new REGISTER.
     */
    if (g.complete || g.unregistering)
        return;

    if (state != PJSIP_TP_STATE_DISCONNECTED &&
        state != PJSIP_TP_STATE_SHUTDOWN)
    {
        return;
    }

    if (g.regc == NULL)
        return;

    if (pjsip_regc_get_info(g.regc, &reg_info) != PJ_SUCCESS ||
        reg_info.transport != tp)
    {
        return;
    }

    PJ_LOG(3,(THIS_FILE, "Registration transport %s is down", tp->obj_name));

    /* Release first: pjsip_transport_shutdown() below notifies this callback
     * again, and by then the registration no longer refers to this transport.
     */
    pjsip_regc_release_transport(g.regc);

    /* Make sure the transport manager will not hand this transport out
     * again, so that the next REGISTER opens a new connection.
     */
    pjsip_transport_shutdown(tp);

    /* This callback can run with the transport manager locked, so only
     * schedule the REGISTER here and let the main loop send it.
     */
    schedule_register(SIP_REG_RETRY_TIMEOUT);
}
#endif  /* EMBUA_USE_REGISTRATION */

/* Generate Contact URI */
static pj_status_t create_contact(char *buf, int buf_size)
{
    pj_sockaddr hostaddr;
    char hostip[PJ_INET_ADDRSTRLEN];
    pj_status_t status;

    if ((status=pj_gethostip(PJ_AF_INET, &hostaddr)) != PJ_SUCCESS)
        return status;

    pj_sockaddr_print(&hostaddr, hostip, sizeof(hostip), 2);
    pj_ansi_snprintf(buf, buf_size, "<sip:%s@%s:%d%s>",
                     AUTH_USERNAME, hostip, SIP_PORT, SIP_TRANSPORT_PARAM);
    return PJ_SUCCESS;
}

static void init(void)
{
    pj_sockaddr addr;
    pjsip_inv_callback inv_cb;
#if EMBUA_SIP_TRANSPORT==SIP_TLS
    pjsip_tls_setting tls_setting;
#endif

#ifdef __ZEPHYR__
    pj_log_set_log_func(log_printk);
#endif

    CHKS(pj_init(), 10);
    pj_log_set_level(5);
    CHKS(pjlib_util_init(), 20);
    pj_caching_pool_init(&g.cp, &pj_pool_factory_default_policy, 0);

    g.pool = pj_pool_create(&g.cp.factory, "app%p", 1000, 1000, NULL);

    /* Create global endpoint: */
    {
        const pj_str_t *hostname;
        const char *endpt_name;

        hostname = pj_gethostname();
        endpt_name = hostname->ptr;

        CHKS( pjsip_endpt_create(&g.cp.factory, endpt_name,  &g.endpt), 30);
    }

#if EMBUA_USE_REGISTRATION
    CHKS( pjsip_tpmgr_set_state_cb(pjsip_endpt_get_tpmgr(g.endpt),
                                   &on_tp_state_changed), 35);
#endif

    pj_sockaddr_init(PJ_AF_INET, &addr, NULL, (pj_uint16_t)SIP_PORT);
#if EMBUA_SIP_TRANSPORT==SIP_TLS
    pjsip_tls_setting_default(&tls_setting);
    CHKS( pjsip_tls_transport_start2(g.endpt, &tls_setting, &addr, NULL, 1,
                                     NULL), 50);
#else
    CHKS( pjsip_udp_transport_start( g.endpt, &addr.ipv4, NULL, 1, NULL), 40);
#endif

    CHKS( pjsip_tsx_layer_init_module(g.endpt), 60 );
    CHKS( pjsip_ua_init_module( g.endpt, NULL ), 70 );

    /* Init invite session module. */
    pj_bzero(&inv_cb, sizeof(inv_cb));
    inv_cb.on_state_changed = &call_on_state_changed;
    inv_cb.on_new_session = &call_on_forked;
    inv_cb.on_media_update = &call_on_media_update;
    CHKS( pjsip_inv_usage_init(g.endpt, &inv_cb), 80 );

    /* pjsip_inv_create_uac() attaches a 100rel handler unconditionally. */
    CHKS( pjsip_100rel_init_module(g.endpt), 85 );

    CHKS( pjsip_endpt_register_module( g.endpt, &mod_embedded_ua), 90 );
    CHKS( pjsip_endpt_register_module( g.endpt, &msg_logger), 100 );

    /* We accept MESSAGE, so advertise it in Allow. A registrar that filters
     * contacts by method (e.g. Kamailio's registrar with method_filtering=1)
     * drops our binding when routing an inbound MESSAGE otherwise.
     */
    CHKS( pjsip_endpt_add_capability( g.endpt, &mod_embedded_ua, PJSIP_H_ALLOW,
                                      NULL, 1, &message_method.name), 105 );

#if EMBUA_USE_MEDIA_THREAD
    /* Give pjmedia its own ioqueue and no worker thread of its own: the
     * media thread created below polls that ioqueue itself, so incoming RTP
     * and the media clock are serviced by the same thread and neither has
     * to wait for the SIP thread.
     */
    CHKS( pjmedia_endpt_create(&g.cp.factory, NULL, 0, &g.med_endpt), 110);
#else
    /* No media worker thread: the media transports are polled by the SIP
     * endpoint's ioqueue, from pjsip_endpt_handle_events() in main().
     */
    CHKS( pjmedia_endpt_create(&g.cp.factory,
                               pjsip_endpt_get_ioqueue(g.endpt),
                               0, &g.med_endpt), 110);
#endif

#if EMBUA_AUDIO_CODEC==CODEC_G711
    CHKS( pjmedia_codec_g711_init(g.med_endpt), 130);
#else
    CHKS( pjmedia_codec_g722_init(g.med_endpt), 140);
#endif

    /* Likewise no event worker thread. Safe because nothing this application
     * uses publishes with PJMEDIA_EVENT_PUBLISH_POST_EVENT.
     */
    CHKS( pjmedia_event_mgr_create(g.pool, PJMEDIA_EVENT_MGR_NO_THREAD,
                                   &g.event_mgr), 150);

#if EMBUA_USE_MEDIA_THREAD
    /* Start the media before registering: the first REGISTER can block the
     * main thread in a name lookup or a TLS handshake.
     */
    CHKS( pj_mutex_create_recursive(g.pool, "media", &g.media_lock), 160);
    CHKS( pj_thread_create(g.pool, "media", &media_thread_proc, NULL,
                           0, 0, &g.media_thread), 170);
#endif

#if EMBUA_USE_REGISTRATION
    {
        pj_str_t registrar_uri = pj_str(SIP_REGISTRAR);
        pj_str_t aor = pj_str(SIP_AOR);
        char contact_buf[80];
        pj_str_t contact;
        pjsip_cred_info cred;

        CHKS( pjsip_regc_create(g.endpt, NULL, &regc_cb, &g.regc), 200);
        CHKS( create_contact(contact_buf, sizeof(contact_buf)), 210 );
        contact = pj_str(contact_buf);

        CHKS( pjsip_regc_init(g.regc, &registrar_uri, &aor, &aor, 1,
                              &contact, SIP_REG_TIMEOUT), 220);

        init_cred(&cred);
        CHKS( pjsip_regc_set_credentials(g.regc, 1, &cred), 230);

        send_register();
    }
#endif
}



/* Number of call slots in use. */
static unsigned active_calls(void)
{
    unsigned i, n = 0;

    for (i=0; i<MAX_CALLS; ++i) {
        if (g_calls[i].pool != NULL)
            ++n;
    }
    return n;
}

/* Has everything app_quit() started finished? A call slot is freed by
 * deinit_call() when its INVITE session reaches DISCONNECTED, i.e. once the
 * BYE (or CANCEL) transaction is over.
 */
static pj_bool_t shutdown_complete(void)
{
#if EMBUA_USE_REGISTRATION
    if (g.unregistering)
        return PJ_FALSE;
#endif
    return active_calls() == 0;
}

int main(void)
{
    pj_time_val deadline;

    init();

    /* Loop until the application is asked to quit */
    for (;!g.complete;) {
        pj_time_val timeout;

        timeout.sec = 0;
#if EMBUA_USE_MEDIA_THREAD
        /* The media thread paces itself; nothing here needs a short block. */
        timeout.msec = MAX_POLL_MSEC;
#else
        /* Media first: it decides how long we may block below. */
        timeout.msec = media_poll();
#endif

        pjsip_endpt_handle_events(g.endpt, &timeout);

#if EMBUA_USE_REGISTRATION
        /* Send a REGISTER if one is due. Doing it here rather than from a
         * callback keeps it out of the transport manager's lock.
         */
        if (g.reg_due.sec) {
            pj_time_val now;

            pj_gettimeofday(&now);
            if (PJ_TIME_VAL_GTE(now, g.reg_due))
                send_register();
        }
#endif
    }

    /* Let the BYE and un-REGISTER transactions complete, but do not hang
     * here if the peers or the registrar never answer.
     */
    pj_gettimeofday(&deadline);
    deadline.sec += SIP_SHUTDOWN_TIMEOUT;
    for (;;) {
        pj_time_val now, timeout;

        if (shutdown_complete())
            break;

        timeout.sec = 0;
#if EMBUA_USE_MEDIA_THREAD
        timeout.msec = MAX_POLL_MSEC;
#else
        timeout.msec = media_poll();
#endif

        pjsip_endpt_handle_events(g.endpt, &timeout);
        pj_gettimeofday(&now);
        if (PJ_TIME_VAL_GTE(now, deadline)) {
            PJ_LOG(3,(THIS_FILE, "Timed out waiting for shutdown to "
                                 "complete"));
            break;
        }
    }

    deinit();
    return 0;
}

/* Stop the media clock and destroy the audio stream, keeping the media
 * transport so that it can be reused, e.g. by a re-INVITE.
 */
static void stop_media(struct call_t *call)
{
    MEDIA_LOCK();

    /* Clear first: media_poll() skips the call as soon as this is NULL, and
     * the port belongs to the stream destroyed just below.
     */
    call->med_port = NULL;

    if (call->med_stream) {
        pjmedia_stream_destroy(call->med_stream);
        call->med_stream = NULL;
    }

    MEDIA_UNLOCK();
}

/* Release everything owned by the call and free its slot. */
static void deinit_call(struct call_t *call)
{
    if (call->pool == NULL)
        return;

    stop_media(call);

    if (call->med_transport) {
        pjmedia_transport_media_stop(call->med_transport);
        pjmedia_transport_close(call->med_transport);
        call->med_transport = NULL;
    }

    if (call->inv) {
        call->inv->mod_data[mod_embedded_ua.id] = NULL;
        call->inv = NULL;
    }

    pj_pool_release(call->pool);
    call->pool = NULL;
}

/* Set up a call, for both the UAC and the UAS side: take a free call slot,
 * create the media transport, and build our local SDP. rem_sdp is the offer
 * we are answering, or NULL when the local SDP is an offer.
 */
static pj_status_t create_call(const pjmedia_sdp_session *rem_sdp,
                               struct call_t **p_call,
                               pjmedia_sdp_session **p_sdp)
{
    struct call_t *call = NULL;
#if EMBUA_USE_SRTP
    pjmedia_srtp_setting srtp_opt;
#endif
    pjmedia_transport *udp_tp;
    pjmedia_sdp_session *sdp;
    int i;
    pj_status_t status;

    for (i=0; i<MAX_CALLS; ++i) {
        if (g_calls[i].pool == NULL) {
            call = &g_calls[i];
            break;
        }
    }
    if (call == NULL)
        return PJ_ETOOMANY;

    pj_bzero(call, sizeof(*call));
    call->pool = pj_pool_create(&g.cp.factory, "call%p", 512, 512, NULL);
    if (call->pool == NULL)
        return PJ_ENOMEM;

    /* One RTP/RTCP port pair per call slot. */
    status = pjmedia_transport_udp_create(g.med_endpt, NULL,
                                          RTP_START_PORT + i*2, 0, &udp_tp);
    if (status != PJ_SUCCESS)
        goto on_error;

#if EMBUA_USE_SRTP
    /* Wrap it in SDES-SRTP. The SRTP transport takes ownership of the UDP
     * transport, so closing the former closes both.
     */
    pjmedia_srtp_setting_default(&srtp_opt);
    srtp_opt.close_member_tp = PJ_TRUE;
    srtp_opt.use = PJMEDIA_SRTP_MANDATORY;
    srtp_opt.keying_count = 1;
    srtp_opt.keying[0] = PJMEDIA_SRTP_KEYING_SDES;

    status = pjmedia_transport_srtp_create(g.med_endpt, udp_tp, &srtp_opt,
                                           &call->med_transport);
    if (status != PJ_SUCCESS) {
        pjmedia_transport_close(udp_tp);
        goto on_error;
    }
#else
    /* Plain RTP/AVP. */
    call->med_transport = udp_tp;
#endif

    pjmedia_transport_info_init(&call->med_tpinfo);
    pjmedia_transport_get_info(call->med_transport, &call->med_tpinfo);
    pj_memcpy(&call->med_sock_info, &call->med_tpinfo.sock_info,
              sizeof(pjmedia_sock_info));

    status = pjmedia_endpt_create_sdp(g.med_endpt, call->pool, 1,
                                      &call->med_sock_info, &sdp);
    if (status != PJ_SUCCESS)
        goto on_error;

    /* Let the transport put its a=crypto (and RTP/SAVP) into our SDP. */
    status = pjmedia_transport_media_create(call->med_transport, call->pool,
                                            0, rem_sdp, 0);
    if (status != PJ_SUCCESS)
        goto on_error;

    status = pjmedia_transport_encode_sdp(call->med_transport, call->pool,
                                          sdp, rem_sdp, 0);
    if (status != PJ_SUCCESS)
        goto on_error;

    *p_call = call;
    *p_sdp = sdp;
    return PJ_SUCCESS;

on_error:
    deinit_call(call);
    return status;
}

/* Callback when INVITE session state has changed. */
static void call_on_state_changed( pjsip_inv_session *inv,
                                   pjsip_event *e)
{
    struct call_t *call = (struct call_t*)inv->mod_data[mod_embedded_ua.id];
    PJ_UNUSED_ARG(e);

    if (call == NULL)
        return;

    if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {
        PJ_LOG(3,(THIS_FILE, "Call %d DISCONNECTED [reason=%d (%s)]",
                  (int)(call - g_calls), inv->cause,
                  pjsip_get_status_text(inv->cause)->ptr));
        deinit_call(call);
    } else {
        PJ_LOG(3,(THIS_FILE, "Call state changed to %s",
                  pjsip_inv_state_name(inv->state)));
    }
}

/* This callback is called when dialog has forked. */
static void call_on_forked(pjsip_inv_session *inv, pjsip_event *e)
{
    /* To be done... */
    PJ_UNUSED_ARG(inv);
    PJ_UNUSED_ARG(e);
}

/* Place an outgoing call to the given URI. */
static pj_status_t make_call(const pj_str_t *dst_uri)
{
    struct call_t *call = NULL;
    pjsip_dialog *dlg = NULL;
    pjmedia_sdp_session *local_sdp;
    pjsip_tx_data *tdata;
    pj_str_t local_uri = pj_str(SIP_AOR);
    pjsip_cred_info cred;
    char contact_buf[80];
    pj_str_t contact;
    pj_status_t status;

    status = create_call(NULL, &call, &local_sdp);
    if (status != PJ_SUCCESS) {
        app_perror(THIS_FILE, "Unable to create call", status);
        return status;
    }

    status = create_contact(contact_buf, sizeof(contact_buf));
    if (status != PJ_SUCCESS)
        goto on_error;
    contact = pj_str(contact_buf);

    status = pjsip_dlg_create_uac(pjsip_ua_instance(), &local_uri, &contact,
                                  dst_uri, NULL, &dlg);
    if (status != PJ_SUCCESS)
        goto on_error;

    /* The PBX will normally challenge our INVITE. */
    init_cred(&cred);
    status = pjsip_auth_clt_set_credentials(&dlg->auth_sess, 1, &cred);
    if (status != PJ_SUCCESS)
        goto on_error;

    status = pjsip_inv_create_uac(dlg, local_sdp, 0, &call->inv);
    if (status != PJ_SUCCESS)
        goto on_error;

    call->inv->mod_data[mod_embedded_ua.id] = call;

    status = pjsip_inv_invite(call->inv, &tdata);
    if (status != PJ_SUCCESS)
        goto on_error;

    status = pjsip_inv_send_msg(call->inv, tdata);
    if (status != PJ_SUCCESS)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "Calling %.*s", (int)dst_uri->slen, dst_uri->ptr));
    return PJ_SUCCESS;

on_error:
    app_perror(THIS_FILE, "Unable to make call", status);
    if (call->inv)
        pjsip_inv_terminate(call->inv, PJSIP_SC_INTERNAL_SERVER_ERROR,
                            PJ_FALSE);
    else if (dlg)
        pjsip_dlg_terminate(dlg);
    deinit_call(call);
    return status;
}

/* Handle an incoming MESSAGE. The commands understood are "call <uri>",
 * e.g. "call sip:alice@192.168.0.2", which places an outgoing call, "mem",
 * which dumps memory usage, and "quit", which shuts the application down.
 */
static void handle_message(pjsip_rx_data *rdata)
{
    static const pj_str_t CMD_CALL = { "call", 4 };
    static const pj_str_t CMD_QUIT = { "quit", 4 };
    static const pj_str_t CMD_MEM  = { "mem", 3 };
    pjsip_msg_body *body = rdata->msg_info.msg->body;
    char uri_buf[PJSIP_MAX_URL_SIZE];
    pj_str_t text, uri;
#if EMBUA_SIP_TRANSPORT == SIP_TLS
    pj_str_t tp_param = pj_str("transport=");
#endif

    /* Accept the MESSAGE regardless of what it contains. This must be
     * stateful: the transaction then absorbs any retransmission, which
     * would otherwise be processed again and place a second call.
     */
    pjsip_endpt_respond(g.endpt, NULL, rdata, PJSIP_SC_OK, NULL,
                        NULL, NULL, NULL);

    if (body == NULL || body->len == 0)
        return;

    text.ptr = (char*)body->data;
    text.slen = body->len;
    pj_strtrim(&text);

    /* "quit": shut the application down. */
    if (pj_stricmp(&text, &CMD_QUIT) == 0) {
        app_quit();
        return;
    }

    /* "mem": dump the caching pool, i.e. every live pool with its used size
     * and capacity. Needs PJ_LOG_MAX_LEVEL >= 3, since cpool_dump_status()
     * compiles to nothing below that.
     */
    if (pj_stricmp(&text, &CMD_MEM) == 0) {
        pj_pool_factory_dump(&g.cp.factory, PJ_TRUE);
        return;
    }

    /* "call <uri>": place an outgoing call to <uri>. */
    if (text.slen <= CMD_CALL.slen ||
        pj_strnicmp(&text, &CMD_CALL, CMD_CALL.slen) != 0 ||
        !pj_isspace(text.ptr[CMD_CALL.slen]))
    {
        PJ_LOG(3,(THIS_FILE, "Ignoring MESSAGE body '%.*s'",
                  (int)text.slen, text.ptr));
        return;
    }

    uri.ptr = text.ptr + CMD_CALL.slen;
    uri.slen = text.slen - CMD_CALL.slen;
    pj_strtrim(&uri);

    /* Leave room for the transport parameter appended below. */
    if (uri.slen == 0 || uri.slen + 16 >= (pj_ssize_t)sizeof(uri_buf)) {
        PJ_LOG(3,(THIS_FILE, "Ignoring MESSAGE: bad call URI"));
        return;
    }

    pj_memcpy(uri_buf, uri.ptr, uri.slen);
    uri_buf[uri.slen] = '\0';

#if EMBUA_SIP_TRANSPORT == SIP_TLS
    /* We only listen on TLS, so the INVITE must go out on TLS too. */
    if (pj_stristr(&uri, &tp_param) == NULL)
        pj_ansi_strxcat(uri_buf, ";transport=tls", sizeof(uri_buf));
#endif

    uri = pj_str(uri_buf);
    make_call(&uri);
}

/*
 * Callback when incoming requests outside any transactions and any
 * dialogs are received. We're interested in incoming INVITE, which we
 * answer, and in incoming MESSAGE, which may ask us to place a call.
 * Any other request is rejected with 400 response.
 */
static pj_bool_t on_rx_request( pjsip_rx_data *rdata )
{
#define RESPOND_ERR(sc) {status_code = sc; goto on_error; }
    struct call_t *call = NULL;
    pjsip_status_code status_code;
    pjsip_dialog *dlg = NULL;
    pjmedia_sdp_session *local_sdp;
    const pjmedia_sdp_session *rem_sdp;
    char contact_buf[80];
    pj_str_t contact_uri;
    pjsip_tx_data *tdata = NULL;
    unsigned options = 0;
    pj_status_t status;

    if (pjsip_method_cmp(&rdata->msg_info.msg->line.req.method,
                         &message_method) == 0)
    {
        handle_message(rdata);
        return PJ_TRUE;
    }

    /* Respond (statelessly) any non-INVITE requests with 400 */
    if (rdata->msg_info.msg->line.req.method.id != PJSIP_INVITE_METHOD) {
        if (rdata->msg_info.msg->line.req.method.id != PJSIP_ACK_METHOD)
            RESPOND_ERR(PJSIP_SC_BAD_REQUEST);
        return PJ_TRUE;
    }

    /* The remote offer, or NULL if this INVITE carries no SDP. */
    rem_sdp = pjsip_rdata_get_sdp_info(rdata)->sdp;

    status = create_call(rem_sdp, &call, &local_sdp);
    if (status == PJ_ETOOMANY) {
        RESPOND_ERR(PJSIP_SC_BUSY_HERE);
    } else if (status != PJ_SUCCESS) {
        /* Typically the offer is not SRTP capable. */
        app_perror(THIS_FILE, "Unable to accept call", status);
        RESPOND_ERR(PJSIP_SC_NOT_ACCEPTABLE_HERE);
    }

    status = pjsip_inv_verify_request(rdata, &options, local_sdp, NULL,
                                      g.endpt, &tdata);
    if (status != PJ_SUCCESS) {
        deinit_call(call);
        pjsip_endpt_send_response2( g.endpt, rdata, tdata, NULL, NULL);
        return PJ_TRUE;
    }

    status = create_contact(contact_buf, sizeof(contact_buf));
    if (status != PJ_SUCCESS)
        RESPOND_ERR(PJSIP_SC_INTERNAL_SERVER_ERROR);
    contact_uri = pj_str(contact_buf);

    status = pjsip_dlg_create_uas_and_inc_lock( pjsip_ua_instance(), rdata,
                                                &contact_uri, &dlg);
    if (status != PJ_SUCCESS)
        RESPOND_ERR(PJSIP_SC_INTERNAL_SERVER_ERROR);

    status = pjsip_inv_create_uas( dlg, rdata, local_sdp, 0, &call->inv);
    if (status != PJ_SUCCESS)
        RESPOND_ERR(PJSIP_SC_INTERNAL_SERVER_ERROR);

    call->inv->mod_data[mod_embedded_ua.id] = call;

    /* Invite session has been created, decrement & release dialog lock. */
    pjsip_dlg_dec_lock(dlg);
    dlg = NULL;

    status = pjsip_inv_initial_answer(call->inv, rdata, PJSIP_SC_RINGING,
                                      NULL, NULL, &tdata);
    if (status == PJ_SUCCESS)
        status = pjsip_inv_send_msg(call->inv, tdata);

    if (status == PJ_SUCCESS)
        status = pjsip_inv_answer( call->inv, PJSIP_SC_OK, NULL,
                                   NULL, &tdata);
    if (status == PJ_SUCCESS)
        status = pjsip_inv_send_msg(call->inv, tdata);

    if (status != PJ_SUCCESS) {
        app_perror(THIS_FILE, "Unable to answer call", status);
        pjsip_inv_terminate(call->inv, PJSIP_SC_INTERNAL_SERVER_ERROR,
                            PJ_FALSE);
        deinit_call(call);
    }

    return PJ_TRUE;

on_error:
    if (dlg != NULL)
        pjsip_dlg_dec_lock(dlg);
    if (call != NULL)
        deinit_call(call);
    pjsip_endpt_respond( g.endpt, NULL, rdata, status_code, NULL,
                         NULL, NULL, NULL);
    return PJ_TRUE;
#undef RESPOND_ERR
}

/* Media clock, called from the media thread, or from the main loop when
 * there is none. For every call whose turn has come, read the decoded audio
 * from the stream and put it straight back, i.e. loopback the incoming
 * audio to the remote party.
 *
 * Returns how many milliseconds the caller may block before the earliest
 * next frame is due, so that the poll paces the media rather than a per-call
 * thread sleeping. While any call has media this is the real time to its
 * next frame, i.e. at most one ptime; MEDIA_IDLE_POLL_MSEC applies only
 * when no call has any. One frame buffer serves all calls, because only one
 * call is being processed at any moment.
 */
static unsigned media_poll(void)
{
    pj_int16_t frame_buf[MAX_FRAME_SAMPLES];
    unsigned delay = MEDIA_IDLE_POLL_MSEC;
    pj_timestamp now;
    int i;

    pj_get_timestamp(&now);

    MEDIA_LOCK();

    for (i=0; i<MAX_CALLS; ++i) {
        struct call_t *call = &g_calls[i];
        unsigned msec;

        if (call->med_port == NULL)
            continue;

        if (pj_cmp_timestamp(&now, &call->next_tick) >= 0) {
            pjmedia_frame frame;

            frame.buf = frame_buf;
            frame.size = call->samples_per_frame * 2;
            frame.type = PJMEDIA_FRAME_TYPE_AUDIO;

            if (pjmedia_port_get_frame(call->med_port, &frame) != PJ_SUCCESS)
            {
                frame.type = PJMEDIA_FRAME_TYPE_NONE;
                frame.size = 0;
            }

            pjmedia_port_put_frame(call->med_port, &frame);

            /* Next frame one ptime later, without accumulating drift. If we
             * are already past it, the loop fell behind: resynchronise
             * rather than trying to catch up frame by frame.
             */
            pj_add_timestamp32(&call->next_tick, call->ts_per_frame);
            if (pj_cmp_timestamp(&now, &call->next_tick) > 0)
                call->next_tick = now;
        }

        msec = (pj_cmp_timestamp(&now, &call->next_tick) < 0)?
                pj_elapsed_msec(&now, &call->next_tick) : 0;
        if (msec < delay)
            delay = msec;
    }

    MEDIA_UNLOCK();

    return delay;
}

#if EMBUA_USE_MEDIA_THREAD
/* The media thread. It runs the media clock and polls pjmedia's ioqueue,
 * i.e. it both produces the outgoing frames and receives the incoming RTP,
 * so a busy main thread cannot stall the audio. media_poll() says how long
 * the ioqueue may block, which is what paces the clock.
 */
static int media_thread_proc(void *arg)
{
    pj_ioqueue_t *ioqueue = pjmedia_endpt_get_ioqueue(g.med_endpt);

    PJ_UNUSED_ARG(arg);

    while (!g.media_quit) {
        pj_time_val timeout;

        timeout.sec = 0;
        timeout.msec = media_poll();

        pj_ioqueue_poll(ioqueue, &timeout);
    }

    return 0;
}
#endif

/* Callback after SDP negotiation */
static void call_on_media_update( pjsip_inv_session *inv,
                                  pj_status_t status)
{
    struct call_t *call = (struct call_t*) inv->mod_data[mod_embedded_ua.id];
    pjmedia_stream_info stream_info;
    const pjmedia_sdp_session *local_sdp;
    const pjmedia_sdp_session *remote_sdp;
    unsigned samples_per_frame, ptime;
    pjmedia_port *port;
    pj_timestamp freq;

    if (status != PJ_SUCCESS) {
        app_perror(THIS_FILE, "SDP negotiation has failed", status);

        /* Here we should disconnect call if we're not in the middle
         * of initializing an UAS dialog and if this is not a re-INVITE.
         */
        return;
    }

    if (call == NULL)
        return;

    /* This may be a re-INVITE, so drop the previous stream first. */
    stop_media(call);

    /* Get local and remote SDP.
     * We need both SDPs to create a media session.
     */
    status = pjmedia_sdp_neg_get_active_local(inv->neg, &local_sdp);
    if (status != PJ_SUCCESS) {
        app_perror(THIS_FILE, "Unable to get local SDP", status);
        return;
    }

    status = pjmedia_sdp_neg_get_active_remote(inv->neg, &remote_sdp);
    if (status != PJ_SUCCESS) {
        app_perror(THIS_FILE, "Unable to get remote SDP", status);
        return;
    }

    /* Activate SRTP with the keys that have just been negotiated. */
    status = pjmedia_transport_media_start(call->med_transport, call->pool,
                                           local_sdp, remote_sdp, 0);
    if (status != PJ_SUCCESS) {
        app_perror( THIS_FILE, "pjmedia_transport_media_start() error", status);
        return;
    }

    /* Create stream info based on the media audio SDP. */
    status = pjmedia_stream_info_from_sdp(&stream_info, call->pool,
                                          g.med_endpt,
                                          local_sdp, remote_sdp, 0);
    if (status != PJ_SUCCESS) {
        app_perror(THIS_FILE,"pjmedia_stream_info_from_sdp() error",status);
        return;
    }

    /* If required, we can also change some settings in the stream info,
     * (such as jitter buffer settings, codec settings, etc) before we
     * create the stream.
     */

    /* Create new audio media stream, passing the stream info, and also the
     * media transport that we created earlier.
     */
    status = pjmedia_stream_create(g.med_endpt, call->pool, &stream_info,
                                   call->med_transport, NULL,
                                   &call->med_stream);
    if (status != PJ_SUCCESS) {
        app_perror( THIS_FILE, "pjmedia_stream_create() error", status);
        return;
    }

    /* Start the audio stream */
    status = pjmedia_stream_start(call->med_stream);
    if (status != PJ_SUCCESS) {
        app_perror( THIS_FILE, "pjmedia_stream_start() error", status);
        return;
    }

    /* Arm the media clock. Setting med_port last is what makes media_poll()
     * start servicing this call.
     */
    status = pjmedia_stream_get_port(call->med_stream, &port);
    if (status != PJ_SUCCESS) {
        app_perror( THIS_FILE, "pjmedia_stream_get_port() error", status);
        return;
    }

    samples_per_frame = PJMEDIA_PIA_SPF(&port->info);
    ptime = PJMEDIA_PIA_PTIME(&port->info);
    if (samples_per_frame > MAX_FRAME_SAMPLES || ptime == 0) {
        PJ_LOG(1,(THIS_FILE, "Call %d unsupported frame: %u samples/%u ms",
                  (int)(call - g_calls), samples_per_frame, ptime));
        return;
    }

    MEDIA_LOCK();

    pj_get_timestamp_freq(&freq);
    call->samples_per_frame = samples_per_frame;
    call->ts_per_frame = (pj_uint32_t)(freq.u64 * ptime / 1000);
    pj_get_timestamp(&call->next_tick);
    call->med_port = port;

    MEDIA_UNLOCK();

    PJ_LOG(3,(THIS_FILE, "Call %d media started (%u samples/%u ms)",
              (int)(call - g_calls), samples_per_frame, ptime));
}

#else   /* EMBUA_AUDIO_CODEC */

#include <stdio.h>

int main(void)
{
    puts("Error: embedded_ua needs G.711 or G.722 "
         "(PJMEDIA_HAS_G711_CODEC or PJMEDIA_HAS_G722_CODEC)");
    return 1;
}

#endif  /* EMBUA_AUDIO_CODEC */
