/* $Id$
 *
 */

#include <pjlib.h>
#include <pjsip_core.h>
#include <pjsip_ua.h>
#include <pjsip_simple.h>
#include <pjmedia.h>
#include <ctype.h>
#include <stdlib.h>
#include <pj/stun.h>

#define START_PORT	    5060
#define MAX_BUDDIES	    32
#define THIS_FILE	    "main.c"
#define MAX_PRESENTITY	    32
#define PRESENCE_TIMEOUT    60

/* By default we'll have one worker thread, except when threading 
 * is disabled. 
 */
#if PJ_HAS_THREADS
#  define WORKER_COUNT	1
#else
#  define WORKER_COUNT	0
#endif

/* Global variable. */
static struct
{
    /* Control. */
    pj_pool_factory *pf;
    pjsip_endpoint  *endpt;
    pj_pool_t	    *pool;
    pjsip_user_agent *user_agent;
    int		     worker_cnt;
    int		     worker_quit_flag;

    /* User info. */
    char	     user_id[64];
    pj_str_t	     local_uri;
    pj_str_t	     contact;
    pj_str_t	     real_contact;

    /* Dialog. */
    pjsip_dlg	    *cur_dlg;

    /* Authentication. */
    int		     cred_count;
    pjsip_cred_info  cred_info[4];

    /* Media stack. */
    pj_bool_t	     null_audio;
    pj_med_mgr_t    *mmgr;

    /* Misc. */
    int		     app_log_level;
    char	    *log_filename;
    FILE	    *log_file;

    /* Proxy URLs */
    pj_str_t	     proxy;
    pj_str_t	     outbound_proxy;

    /* UA auto options. */
    int		     auto_answer;	/* -1 to disable. */
    int		     auto_hangup;	/* -1 to disable */

    /* Registration. */
    pj_str_t	     registrar_uri;
    pjsip_regc	    *regc;
    pj_int32_t	     reg_timeout;
    pj_timer_entry   regc_timer;

    /* STUN */
    pj_str_t	     stun_srv1;
    int		     stun_port1;
    pj_str_t	     stun_srv2;
    int		     stun_port2;

    /* UDP sockets and their public address. */
    int		     sip_port;
    pj_sock_t	     sip_sock;
    pj_sockaddr_in   sip_sock_name;
    pj_sock_t	     rtp_sock;
    pj_sockaddr_in   rtp_sock_name;
    pj_sock_t	     rtcp_sock;
    pj_sockaddr_in   rtcp_sock_name;

    /* SIMPLE */
    pj_bool_t	     hide_status;
    pj_bool_t	     offer_x_ms_msg;
    int		     im_counter;
    int		     buddy_cnt;
    pj_str_t	     buddy[MAX_BUDDIES];
    pj_bool_t	     buddy_status[MAX_BUDDIES];
    pj_bool_t	     no_presence;
    pjsip_presentity *buddy_pres[MAX_BUDDIES];

    int		    pres_cnt;
    pjsip_presentity *pres[MAX_PRESENTITY];

} global;

enum { AUTO_ANSWER, AUTO_HANGUP };

/* This is the data that will be 'attached' on per dialog basis. */
struct dialog_data
{
    /* Media session. */
    pj_media_session_t *msession;

    /* x-ms-chat session. */
    pj_bool_t		x_ms_msg_session;

    /* Cached SDP body, updated when media session changed. */
    pjsip_msg_body *body;

    /* Timer. */
    pj_bool_t	     has_auto_timer;
    pj_timer_entry   auto_timer;
};

/*
 * These are the callbacks to be registered to dialog to receive notifications
 * about various events in the dialog.
 */
static void dlg_on_all_events	(pjsip_dlg *dlg, pjsip_dlg_event_e dlg_evt,
				 pjsip_event *event );
static void dlg_on_before_tx	(pjsip_dlg *dlg, pjsip_transaction *tsx, 
				 pjsip_tx_data *tdata, int retransmission);
static void dlg_on_tx_msg	(pjsip_dlg *dlg, pjsip_transaction *tsx, 
				 pjsip_tx_data *tdata);
static void dlg_on_rx_msg	(pjsip_dlg *dlg, pjsip_transaction *tsx, 
				 pjsip_rx_data *rdata);
static void dlg_on_incoming	(pjsip_dlg *dlg, pjsip_transaction *tsx,
				 pjsip_rx_data *rdata);
static void dlg_on_calling	(pjsip_dlg *dlg, pjsip_transaction *tsx,
				 pjsip_tx_data *tdata);
static void dlg_on_provisional	(pjsip_dlg *dlg, pjsip_transaction *tsx,
				 pjsip_event *event);
static void dlg_on_connecting	(pjsip_dlg *dlg, pjsip_event *event);
static void dlg_on_established	(pjsip_dlg *dlg, pjsip_event *event);
static void dlg_on_disconnected	(pjsip_dlg *dlg, pjsip_event *event);
static void dlg_on_terminated	(pjsip_dlg *dlg);
static void dlg_on_mid_call_evt	(pjsip_dlg *dlg, pjsip_event *event);

/* The callback structure that will be registered to UA layer. */
struct pjsip_dlg_callback dlg_callback = {
    &dlg_on_all_events,
    &dlg_on_before_tx,
    &dlg_on_tx_msg,
    &dlg_on_rx_msg,
    &dlg_on_incoming,
    &dlg_on_calling,
    &dlg_on_provisional,
    &dlg_on_connecting,
    &dlg_on_established,
    &dlg_on_disconnected,
    &dlg_on_terminated,
    &dlg_on_mid_call_evt
};


/* 
 * Auxiliary things are put in misc.c, so that this main.c file is more 
 * readable. 
 */
#include "misc.c"

static void dlg_auto_timer_callback( pj_timer_heap_t *timer_heap,
				     struct pj_timer_entry *entry)
{
    pjsip_dlg *dlg = entry->user_data;
    struct dialog_data *dlg_data = dlg->user_data;
    
    PJ_UNUSED_ARG(timer_heap)

    dlg_data->has_auto_timer = 0;

    if (entry->id == AUTO_ANSWER) {
	pjsip_tx_data *tdata = pjsip_dlg_answer(dlg, 200);
	if (tdata) {
	    struct dialog_data *dlg_data = global.cur_dlg->user_data;
	    tdata->msg->body = dlg_data->body;
	    pjsip_dlg_send_msg(dlg, tdata);
	}
    } else {
	pjsip_tx_data *tdata = pjsip_dlg_disconnect(dlg, 500);
	if (tdata) 
	    pjsip_dlg_send_msg(dlg, tdata);
    }
}

static void update_registration(pjsip_regc *regc, int renew)
{
    pjsip_tx_data *tdata;

    PJ_LOG(3,(THIS_FILE, "Performing SIP registration..."));

    if (renew) {
	tdata = pjsip_regc_register(regc, 1);
    } else {
	tdata = pjsip_regc_unregister(regc);
    }

    pjsip_regc_send( regc, tdata );
}

static void regc_cb(struct pjsip_regc_cbparam *param)
{
    /*
     * Print registration status.
     */
    if (param->code < 0 || param->code >= 300) {
	PJ_LOG(2, (THIS_FILE, "SIP registration failed, status=%d (%s)", 
		   param->code, pjsip_get_status_text(param->code)->ptr));
	global.regc = NULL;

    } else if (PJSIP_IS_STATUS_IN_CLASS(param->code, 200)) {
	PJ_LOG(3, (THIS_FILE, "SIP registration success, status=%d (%s), "
			      "will re-register in %d seconds", 
			      param->code,
			      pjsip_get_status_text(param->code)->ptr,
			      param->expiration));

    } else {
	PJ_LOG(4, (THIS_FILE, "SIP registration updated status=%d", param->code));
    }
}

static void pres_on_received_request(pjsip_presentity *pres, pjsip_rx_data *rdata,
				     int *timeout)
{
    int state;
    int i;
    char url[PJSIP_MAX_URL_SIZE];
    int urllen;

    PJ_UNUSED_ARG(rdata)

    if (*timeout > 0) {
	state = PJSIP_EVENT_SUB_STATE_ACTIVE;
	if (*timeout > 300)
	    *timeout = 300;
    } else {
	state = PJSIP_EVENT_SUB_STATE_TERMINATED;
    }

    urllen = pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR, rdata->from->uri, url, sizeof(url)-1);
    if (urllen < 1) {
	pj_native_strcpy(url, "<unknown>");
    } else {
	url[urllen] = '\0';
    }
    PJ_LOG(3,(THIS_FILE, "Received presence request from %s, sub_state=%s", 
			 url, 
			 (state==PJSIP_EVENT_SUB_STATE_ACTIVE?"active":"terminated")));

    for (i=0; i<global.pres_cnt; ++i)
	if (global.pres[i] == pres)
	    break;
    if (i == global.pres_cnt)
	global.pres[global.pres_cnt++] = pres;

    pjsip_presence_set_credentials( pres, global.cred_count, global.cred_info );
    pjsip_presence_notify(pres, state, !global.hide_status);

}

static void pres_on_received_refresh(pjsip_presentity *pres, pjsip_rx_data *rdata)
{
    pres_on_received_request(pres, rdata, &pres->sub->default_interval);
}

/* This is called by presence framework when we receives presence update
 * of a resource (buddy).
 */
static void pres_on_received_update(pjsip_presentity *pres, pj_bool_t is_open)
{
    int buddy_index = (int)pres->user_data;

    global.buddy_status[buddy_index] = is_open;
    PJ_LOG(3,(THIS_FILE, "Presence update: %s is %s", 
			 global.buddy[buddy_index].ptr,
			 (is_open ? "Online" : "Offline")));
}

/* This is called when the subscription is terminated. */
static void pres_on_terminated(pjsip_presentity *pres, const pj_str_t *reason)
{
    if (pres->sub->role == PJSIP_ROLE_UAC) {
	int buddy_index = (int)pres->user_data;
	PJ_LOG(3,(THIS_FILE, "Presence subscription for %s is terminated (reason=%.*s)", 
			    global.buddy[buddy_index].ptr,
			    reason->slen, reason->ptr));
	global.buddy_pres[buddy_index] = NULL;
	global.buddy_status[buddy_index] = 0;
    } else {
	int i;
	PJ_LOG(3,(THIS_FILE, "Notifier terminated (reason=%.*s)", 
			    reason->slen, reason->ptr));
	pjsip_presence_notify(pres, PJSIP_EVENT_SUB_STATE_TERMINATED, 1);
	for (i=0; i<global.pres_cnt; ++i) {
	    if (global.pres[i] == pres) {
		int j;
		global.pres[i] = NULL;
		for (j=i+1; j<global.pres_cnt; ++j)
		    global.pres[j-1] = global.pres[j];
		global.pres_cnt--;
		break;
	    }
	}
    }
    pjsip_presence_destroy(pres);
}


/* Callback attached to SIP body to print the body to message buffer. */
static int print_msg_body(pjsip_msg_body *msg_body, char *buf, pj_size_t size)
{
    pjsip_msg_body *body = msg_body;
    return pjsdp_print ((pjsdp_session_desc*)body->data, buf, size);
}

/* When media session has changed, call this function to update the cached body
 * information in the dialog. 
 */
static pjsip_msg_body *create_msg_body (pjsip_dlg *dlg, pj_bool_t is_ack_msg)
{
    struct dialog_data *dlg_data = dlg->user_data;
    pjsdp_session_desc *sdp;

    sdp = pj_media_session_create_sdp (dlg_data->msession, dlg->pool, is_ack_msg);
    if (!sdp) {
	dlg_data->body = NULL;
	return NULL;
    }

    /* For outgoing INVITE, if we offer "x-ms-message" line, then add a new
     * "m=" line in the SDP.
     */
    if (dlg_data->x_ms_msg_session >= 0 && 
	dlg_data->x_ms_msg_session >= (int)sdp->media_count) 
    {
	pjsdp_media_desc *m = pj_pool_calloc(dlg->pool, 1, sizeof(*m));
	sdp->media[sdp->media_count] = m;
	dlg_data->x_ms_msg_session = sdp->media_count++;
    }

    /*
     * For "x-ms-message" line, remove all attributes and connection line etc.
     */
    if (dlg_data->x_ms_msg_session >= 0) {
	pjsdp_media_desc *m = sdp->media[dlg_data->x_ms_msg_session];
	if (m) {
	    m->desc.media = pj_str("x-ms-message");
	    m->desc.port = 5060;
	    m->desc.transport = pj_str("sip");
	    m->desc.fmt_count = 1;
	    m->desc.fmt[0] = pj_str("null");
	    m->attr_count = 0;
	    m->conn = NULL;
	}
    }

    dlg_data->body = pj_pool_calloc(dlg->pool, 1, sizeof(*dlg_data->body));
    dlg_data->body->content_type.type = pj_str("application");
    dlg_data->body->content_type.subtype = pj_str("sdp");
    dlg_data->body->len = 0;	/* ignored */
    dlg_data->body->print_body = &print_msg_body;

    dlg_data->body->data = sdp;
    return dlg_data->body;
}

/* This callback will be called on every occurence of events in dialogs */
static void dlg_on_all_events(pjsip_dlg *dlg, pjsip_dlg_event_e dlg_evt,
			      pjsip_event *event )
{
    PJ_UNUSED_ARG(dlg_evt)
    PJ_UNUSED_ARG(event)

    PJ_LOG(4, (THIS_FILE, "dlg_on_all_events %p", dlg));
}

/* This callback is called before each outgoing msg is sent (including 
 * retransmission). Application can override this notification if it wants
 * to modify the message before transmission or if it wants to do something
 * else for each transmission.
 */
static void dlg_on_before_tx(pjsip_dlg *dlg, pjsip_transaction *tsx, 
			     pjsip_tx_data *tdata, int ret_cnt)
{
    PJ_UNUSED_ARG(tsx)
    PJ_UNUSED_ARG(tdata)

    if (ret_cnt > 0) {
	PJ_LOG(3, (THIS_FILE, "Dialog %s: retransmitting message (cnt=%d)", 
			      dlg->obj_name, ret_cnt));
    }
}

/* This callback is called after a message is sent. */
static void dlg_on_tx_msg(pjsip_dlg *dlg, pjsip_transaction *tsx, 
			  pjsip_tx_data *tdata)
{
    PJ_UNUSED_ARG(tsx)
    PJ_UNUSED_ARG(tdata)

    PJ_LOG(4, (THIS_FILE, "dlg_on_tx_msg %p", dlg));
}

/* This callback is called on receipt of incoming message. */
static void dlg_on_rx_msg(pjsip_dlg *dlg, pjsip_transaction *tsx, 
			  pjsip_rx_data *rdata)
{
    PJ_UNUSED_ARG(tsx)
    PJ_UNUSED_ARG(rdata)
    PJ_LOG(4, (THIS_FILE, "dlg_on_tx_msg %p", dlg));
}

/* This callback is called after dialog has sent INVITE */
static void dlg_on_calling(pjsip_dlg *dlg, pjsip_transaction *tsx,
			   pjsip_tx_data *tdata)
{
    PJ_UNUSED_ARG(tsx)
    PJ_UNUSED_ARG(tdata)

    pj_assert(tdata->msg->type == PJSIP_REQUEST_MSG &&
	      tdata->msg->line.req.method.id == PJSIP_INVITE_METHOD &&
	      tsx->method.id == PJSIP_INVITE_METHOD);

    PJ_LOG(3, (THIS_FILE, "Dialog %s: start calling...", dlg->obj_name));
}

static void create_session_from_sdp( pjsip_dlg *dlg, pjsdp_session_desc *sdp)
{
    struct dialog_data *dlg_data = dlg->user_data;
    pj_bool_t sdp_x_ms_msg_index = -1;
    int i;
    int mcnt;
    const pj_media_stream_info *mi[PJSDP_MAX_MEDIA];
    int has_active;
    pj_media_sock_info sock_info;

    /* Find "m=x-ms-message" line in the SDP. */
    for (i=0; i<(int)sdp->media_count; ++i) {
	if (pj_stricmp2(&sdp->media[i]->desc.media, "x-ms-message")==0)
	    sdp_x_ms_msg_index = i;
    }

    /*
     * Create media session.
     */
    pj_memset(&sock_info, 0, sizeof(sock_info));
    sock_info.rtp_sock = global.rtp_sock;
    sock_info.rtcp_sock = global.rtcp_sock;
    pj_memcpy(&sock_info.rtp_addr_name, &global.rtp_sock_name, sizeof(pj_sockaddr_in));

    dlg_data->msession = pj_media_session_create_from_sdp (global.mmgr, sdp, &sock_info);

    /* A session will always be created, unless there is memory
     * alloc problem.
     */
    pj_assert(dlg_data->msession);

    /* See if we can take the offer by checking that we have at least
     * one media stream active.
     */
    mcnt = pj_media_session_enum_streams(dlg_data->msession, PJSDP_MAX_MEDIA, mi);
    for (i=0, has_active=0; i<mcnt; ++i) {
	if (mi[i]->fmt_cnt>0 && mi[i]->dir!=PJ_MEDIA_DIR_NONE) {
	    has_active = 1;
	    break;
	}
    }

    if (!has_active && sdp_x_ms_msg_index==-1) {
	pjsip_tx_data *tdata;

	/* Unable to accept remote's SDP. 
	 * Answer with 488 (Not Acceptable Here)
	 */
	/* Create 488 response. */
	tdata = pjsip_dlg_answer(dlg, PJSIP_SC_NOT_ACCEPTABLE_HERE);

	/* Send response. */
	if (tdata)
	    pjsip_dlg_send_msg(dlg, tdata);
	return;
    }

    dlg_data->x_ms_msg_session = sdp_x_ms_msg_index;

    /* Create msg body to be used later in 2xx/response */
    create_msg_body(dlg, 0);

}

/* This callback is called after an INVITE is received. */
static void dlg_on_incoming(pjsip_dlg *dlg, pjsip_transaction *tsx,
			    pjsip_rx_data *rdata)
{
    struct dialog_data *dlg_data;
    pjsip_msg *msg;
    pjsip_tx_data *tdata;
    char buf[128];
    int len;

    PJ_UNUSED_ARG(tsx)

    pj_assert(rdata->msg->type == PJSIP_REQUEST_MSG &&
	      rdata->msg->line.req.method.id == PJSIP_INVITE_METHOD &&
	      tsx->method.id == PJSIP_INVITE_METHOD);

    /*
     * Notify user!
     */
    PJ_LOG(3, (THIS_FILE, ""));
    PJ_LOG(3, (THIS_FILE, "INCOMING CALL ON DIALOG %s!!", dlg->obj_name));
    PJ_LOG(3, (THIS_FILE, ""));
    len = pjsip_uri_print( PJSIP_URI_IN_FROMTO_HDR, 
			   (pjsip_name_addr*)dlg->remote.info->uri, 
			   buf, sizeof(buf)-1);
    if (len > 0) {
	buf[len] = '\0';
	PJ_LOG(3,(THIS_FILE, "From:\t%s", buf));
    }
    len = pjsip_uri_print( PJSIP_URI_IN_FROMTO_HDR, 
			   (pjsip_name_addr*)dlg->local.info->uri, 
			   buf, sizeof(buf)-1);
    if (len > 0) {
	buf[len] = '\0';
	PJ_LOG(3,(THIS_FILE, "To:\t%s", buf));
    }
    PJ_LOG(3, (THIS_FILE, "Press 'a' to answer, or 'h' to hangup!!", dlg->obj_name));
    PJ_LOG(3, (THIS_FILE, ""));

    /*
     * Process incoming dialog.
     */

    dlg_data = pj_pool_calloc(dlg->pool, 1, sizeof(struct dialog_data));
    dlg->user_data = dlg_data;

    /* Update contact. */
    pjsip_dlg_set_contact(dlg, &global.contact);

    /* Initialize credentials. */
    pjsip_dlg_set_credentials(dlg, global.cred_count, global.cred_info);

    /* Create media session if the request has "application/sdp" body. */
    msg = rdata->msg;
    if (msg->body && 
	pj_stricmp2(&msg->body->content_type.type, "application")==0 &&
	pj_stricmp2(&msg->body->content_type.subtype, "sdp")==0)
    {
	pjsdp_session_desc *sdp;

	/* Parse SDP body, and instantiate media session based on remote's SDP.
	 * Then create our SDP body from the session.
	 */
	sdp = pjsdp_parse (msg->body->data, msg->body->len, rdata->pool);
	if (!sdp)
	    goto send_answer;

	create_session_from_sdp(dlg, sdp);

    } else if (msg->body) {
	/* The request has a message body other than "application/sdp" */
	pjsip_accept_hdr *accept;

	/* Create response. */
	tdata = pjsip_dlg_answer(dlg, PJSIP_SC_UNSUPPORTED_MEDIA_TYPE);

	/* Add "Accept" header. */
	accept = pjsip_accept_hdr_create(tdata->pool);
	accept->values[0] = pj_str("application/sdp");
	accept->count = 1;
	pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)accept);

	/* Send response. */
	pjsip_dlg_send_msg(dlg, tdata);
	return;

    } else {
	/* The request has no message body. We can take this request, but
	 * no media session will be activated.
	 */
	/* Nothing to do here. */
    }

send_answer:
    /* Immediately answer with 100 (or 180? */
    tdata = pjsip_dlg_answer( dlg, PJSIP_SC_RINGING );
    pjsip_dlg_send_msg(dlg, tdata);

    /* Set current dialog to this dialog if we don't currently have
     * current dialog.
     */
    if (global.cur_dlg == NULL) {
	global.cur_dlg = dlg;
    }

    /* Auto-answer if option is specified. */
    if (global.auto_answer >= 0) {
	pj_time_val delay = { 0, 0};
	struct dialog_data *dlg_data = dlg->user_data;

	PJ_LOG(4, (THIS_FILE, "Scheduling auto-answer in %d seconds", 
			      global.auto_answer));

	delay.sec = global.auto_answer;
	dlg_data->auto_timer.user_data = dlg;
	dlg_data->auto_timer.id = AUTO_ANSWER;
	dlg_data->auto_timer.cb = &dlg_auto_timer_callback;
	dlg_data->has_auto_timer = 1;
	pjsip_endpt_schedule_timer(dlg->ua->endpt, &dlg_data->auto_timer, &delay);
    }
}

/* This callback is called when dialog has sent/received a provisional response
 * to INVITE.
 */
static void dlg_on_provisional(pjsip_dlg *dlg, pjsip_transaction *tsx,
			       pjsip_event *event)
{
    const char *action;

    pj_assert((event->src_type == PJSIP_EVENT_TX_MSG &&
	       event->src.tdata->msg->type == PJSIP_RESPONSE_MSG &&
	       event->src.tdata->msg->line.status.code/100 == 1 &&
	       tsx->method.id == PJSIP_INVITE_METHOD) 
	       ||
	       (event->src_type == PJSIP_EVENT_RX_MSG &&
	       event->src.rdata->msg->type == PJSIP_RESPONSE_MSG &&
	       event->src.rdata->msg->line.status.code/100 == 1 &&
	       tsx->method.id == PJSIP_INVITE_METHOD));

    if (event->src_type == PJSIP_EVENT_TX_MSG)
	action = "Sending";
    else
	action = "Received";

    PJ_LOG(3, (THIS_FILE, "Dialog %s: %s %d (%s)", 
			  dlg->obj_name, action, tsx->status_code,
			  pjsip_get_status_text(tsx->status_code)->ptr));
}

/* This callback is called when 200 response to INVITE is sent/received. */
static void dlg_on_connecting(pjsip_dlg *dlg, pjsip_event *event)
{
    struct dialog_data *dlg_data = dlg->user_data;
    const char *action;

    pj_assert((event->src_type == PJSIP_EVENT_TX_MSG &&
	       event->src.tdata->msg->type == PJSIP_RESPONSE_MSG &&
	       event->src.tdata->msg->line.status.code/100 == 2)
	       ||
	       (event->src_type == PJSIP_EVENT_RX_MSG &&
	       event->src.rdata->msg->type == PJSIP_RESPONSE_MSG &&
	       event->src.rdata->msg->line.status.code/100 == 2));

    if (event->src_type == PJSIP_EVENT_RX_MSG)
	action = "Received";
    else
	action = "Sending";

    PJ_LOG(3, (THIS_FILE, "Dialog %s: %s 200 (OK)", dlg->obj_name, action));

    if (event->src_type == PJSIP_EVENT_RX_MSG) {
	/* On receipt of 2xx response, negotiate our media capability
	 * and start media.
	 */
	pjsip_msg *msg = event->src.rdata->msg;
	pjsip_msg_body *body;
	pjsdp_session_desc *sdp;

	/* Get SDP from message. */

	/* Ignore if no SDP body is present. */
	body = msg->body;
	if (!body) {
	    PJ_LOG(3, (THIS_FILE, "Dialog %s: the 200/OK response has no body!",
				  dlg->obj_name));
	    return;
	}

	if (pj_stricmp2(&body->content_type.type, "application") != 0 &&
	    pj_stricmp2(&body->content_type.subtype, "sdp") != 0) 
	{
	    PJ_LOG(3, (THIS_FILE, "Dialog %s: the 200/OK response has no SDP body!",
				   dlg->obj_name));
	    return;
	}

	/* Got what seems to be a SDP content. Parse it. */
	sdp = pjsdp_parse (body->data, body->len, event->src.rdata->pool);
	if (!sdp) {
	    PJ_LOG(3, (THIS_FILE, "Dialog %s: SDP syntax error!",
				  dlg->obj_name));
	    return;
	}

	/* Negotiate media session with remote's media capability. */
	if (pj_media_session_update (dlg_data->msession, sdp) != 0) {
	    PJ_LOG(3, (THIS_FILE, "Dialog %s: media session update error!",
				  dlg->obj_name));
	    return;
	}

	/* Update the saved SDP body because media session has changed. 
	 * Also set ack flag to '1', because we only want to send one format/
	 * codec for each media streams.
	 */
	create_msg_body(dlg, 1);

	/* Activate media. */
	pj_media_session_activate (dlg_data->msession);

    } else {
	pjsip_msg *msg = event->src.tdata->msg;

	if (msg->body) {
	    /* On transmission of 2xx response, start media session. */
	    pj_media_session_activate (dlg_data->msession);
	}
    }

}

/* This callback is called when ACK to initial INVITE is sent/received. */
static void dlg_on_established(pjsip_dlg *dlg, pjsip_event *event)
{
    const char *action;

    pj_assert((event->src_type == PJSIP_EVENT_TX_MSG &&
	       event->src.tdata->msg->type == PJSIP_REQUEST_MSG &&
	       event->src.tdata->msg->line.req.method.id == PJSIP_ACK_METHOD)
	       ||
	       (event->src_type == PJSIP_EVENT_RX_MSG &&
	       event->src.rdata->msg->type == PJSIP_REQUEST_MSG &&
	       event->src.rdata->msg->line.req.method.id == PJSIP_ACK_METHOD));

    if (event->src_type == PJSIP_EVENT_RX_MSG)
	action = "Received";
    else
	action = "Sending";

    PJ_LOG(3, (THIS_FILE, "Dialog %s: %s ACK, dialog is ESTABLISHED", 
			  dlg->obj_name, action));

    /* Attach SDP body for outgoing ACK. */
    if (event->src_type == PJSIP_EVENT_TX_MSG &&
	event->src.tdata->msg->line.req.method.id == PJSIP_ACK_METHOD)
    {
	struct dialog_data *dlg_data = dlg->user_data;
	event->src.tdata->msg->body = dlg_data->body;
    }

    /* Auto-hangup if option is specified. */
    if (global.auto_hangup >= 0) {
	pj_time_val delay = { 0, 0};
	struct dialog_data *dlg_data = dlg->user_data;

	PJ_LOG(4, (THIS_FILE, "Scheduling auto-hangup in %d seconds", 
			      global.auto_hangup));

	delay.sec = global.auto_hangup;
	dlg_data->auto_timer.user_data = dlg;
	dlg_data->auto_timer.id = AUTO_HANGUP;
	dlg_data->auto_timer.cb = &dlg_auto_timer_callback;
	dlg_data->has_auto_timer = 1;
	pjsip_endpt_schedule_timer(dlg->ua->endpt, &dlg_data->auto_timer, &delay);
    }
}


/* This callback is called when dialog is disconnected (because of final
 * response, BYE, or timer).
 */
static void dlg_on_disconnected(pjsip_dlg *dlg, pjsip_event *event)
{
    struct dialog_data *dlg_data = dlg->user_data;
    int status_code;
    const pj_str_t *reason;
    
    PJ_UNUSED_ARG(event)

    /* Cancel auto-answer/auto-hangup timer. */
    if (dlg_data->has_auto_timer) {
	pjsip_endpt_cancel_timer(dlg->ua->endpt, &dlg_data->auto_timer);
	dlg_data->has_auto_timer = 0;
    }

    if (dlg->invite_tsx)
	status_code = dlg->invite_tsx->status_code;
    else
	status_code = 200;

    if (event->obj.tsx->method.id == PJSIP_INVITE_METHOD) {
	if (event->src_type == PJSIP_EVENT_RX_MSG)
	    reason = &event->src.rdata->msg->line.status.reason;
	else if (event->src_type == PJSIP_EVENT_TX_MSG)
	    reason = &event->src.tdata->msg->line.status.reason;
	else
	    reason = pjsip_get_status_text(event->obj.tsx->status_code);
    } else {
	reason = &event->obj.tsx->method.name;
    }

    PJ_LOG(3, (THIS_FILE, "Dialog %s: DISCONNECTED! Reason=%d (%.*s)", 
			  dlg->obj_name, status_code, 
			  reason->slen, reason->ptr));

    if (dlg_data->msession) {
	pj_media_session_destroy (dlg_data->msession);
	dlg_data->msession = NULL;
    }
}

/* This callback is called when dialog is about to be destroyed. */
static void dlg_on_terminated(pjsip_dlg *dlg)
{
    PJ_LOG(3, (THIS_FILE, "Dialog %s: terminated!", dlg->obj_name));

    /* If current dialog is equal to this dialog, update it. */
    if (global.cur_dlg == dlg) {
	global.cur_dlg = global.cur_dlg->next;
	if (global.cur_dlg == (void*)&global.user_agent->dlg_list) {
	    global.cur_dlg = NULL;
	}
    }
}

/* This callback is called for any requests when dialog is established. */
static void dlg_on_mid_call_evt	(pjsip_dlg *dlg, pjsip_event *event)
{
    pjsip_transaction *tsx = event->obj.tsx;

    if (event->src_type == PJSIP_EVENT_RX_MSG &&
	event->src.rdata->msg->type == PJSIP_REQUEST_MSG) 
    {
	if (event->src.rdata->cseq->method.id == PJSIP_INVITE_METHOD) {
	    /* Re-invitation. */
	    pjsip_tx_data *tdata;

	    PJ_LOG(3,(THIS_FILE, "Dialog %s: accepting re-invitation (dummy)",
				 dlg->obj_name));
	    tdata = pjsip_dlg_answer(dlg, 200);
	    if (tdata) {
		struct dialog_data *dlg_data = dlg->user_data;
		tdata->msg->body = dlg_data->body;
		pjsip_dlg_send_msg(dlg, tdata);
	    }
	} else {
	    /* Don't worry, endpoint will answer with 500 or whetever. */
	}

    } else if (tsx->status_code/100 == 2) {
	PJ_LOG(3,(THIS_FILE, "Dialog %s: outgoing %.*s success: %d (%s)",
		  dlg->obj_name, 
		  tsx->method.name.slen, tsx->method.name.ptr,
		  tsx->status_code, 
		  pjsip_get_status_text(tsx->status_code)->ptr));


    } else if (tsx->status_code >= 300) {
	pj_bool_t report_failure = PJ_TRUE;

	/* Check for authentication failures. */
	if (tsx->status_code==401 || tsx->status_code==407) {
	    pjsip_tx_data *tdata;
	    tdata = pjsip_auth_reinit_req( global.endpt,
					   dlg->pool, &dlg->auth_sess,
					   dlg->cred_count, dlg->cred_info,
					   tsx->last_tx, event->src.rdata );
	    if (tdata) {
		int rc;
		rc = pjsip_dlg_send_msg( dlg, tdata);
		report_failure = (rc != 0);
	    }
	}
	if (report_failure) {
	    const pj_str_t *reason;
	    if (event->src_type == PJSIP_EVENT_RX_MSG) {
		reason = &event->src.rdata->msg->line.status.reason;
	    } else {
		reason = pjsip_get_status_text(tsx->status_code);
	    }
	    PJ_LOG(2,(THIS_FILE, "Dialog %s: outgoing request failed: %d (%.*s)",
		      dlg->obj_name, tsx->status_code, 
		      reason->slen, reason->ptr));
	}
    }
}

/* Initialize sockets and optionally get the public address via STUN. */
static pj_status_t init_sockets()
{
    enum { 
	RTP_START_PORT = 4000,
	RTP_RANDOM_START = 2,
	RTP_RETRY = 10 
    };
    enum {
	SIP_SOCK,
	RTP_SOCK,
	RTCP_SOCK,
    };
    int i;
    int rtp_port;
    pj_sock_t sock[3];
    pj_sockaddr_in mapped_addr[3];

    for (i=0; i<3; ++i)
	sock[i] = PJ_INVALID_SOCKET;

    /* Create and bind SIP UDP socket. */
    sock[SIP_SOCK] = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, 0);
    if (sock[SIP_SOCK] == PJ_INVALID_SOCKET) {
	PJ_LOG(2,(THIS_FILE, "Unable to create socket"));
	goto on_error;
    }
    if (pj_sock_bind_in(sock[SIP_SOCK], 0, (pj_uint16_t)global.sip_port) != 0) {
	PJ_LOG(2,(THIS_FILE, "Unable to bind to SIP port"));
	goto on_error;
    }

    /* Initialize start of RTP port to try. */
    rtp_port = RTP_START_PORT + (pj_rand() % RTP_RANDOM_START) / 2;

    /* Loop retry to bind RTP and RTCP sockets. */
    for (i=0; i<RTP_RETRY; ++i, rtp_port += 2) {

	/* Create and bind RTP socket. */
	sock[RTP_SOCK] = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, 0);
	if (sock[RTP_SOCK] == PJ_INVALID_SOCKET)
	    goto on_error;
	if (pj_sock_bind_in(sock[RTP_SOCK], 0, (pj_uint16_t)rtp_port) != 0) {
	    pj_sock_close(sock[RTP_SOCK]); sock[RTP_SOCK] = PJ_INVALID_SOCKET;
	    continue;
	}

	/* Create and bind RTCP socket. */
	sock[RTCP_SOCK] = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, 0);
	if (sock[RTCP_SOCK] == PJ_INVALID_SOCKET)
	    goto on_error;
	if (pj_sock_bind_in(sock[RTCP_SOCK], 0, (pj_uint16_t)(rtp_port+1)) != 0) {
	    pj_sock_close(sock[RTP_SOCK]); sock[RTP_SOCK] = PJ_INVALID_SOCKET;
	    pj_sock_close(sock[RTCP_SOCK]); sock[RTCP_SOCK] = PJ_INVALID_SOCKET;
	    continue;
	}

	/*
	 * If we're configured to use STUN, then find out the mapped address,
	 * and make sure that the mapped RTCP port is adjacent with the RTP.
	 */
	if (global.stun_port1 == 0) {
	    pj_str_t hostname;
	    pj_sockaddr_in addr;

	    /* Get local IP address. */
	    char hostname_buf[PJ_MAX_HOSTNAME];
	    if (gethostname(hostname_buf, sizeof(hostname_buf)))
		goto on_error;
	    hostname = pj_str(hostname_buf);

	    pj_memset( &addr, 0, sizeof(addr));
	    addr.sin_family = PJ_AF_INET;
	    if (pj_sockaddr_set_str_addr( &addr, &hostname) != PJ_SUCCESS)
		goto on_error;

	    for (i=0; i<3; ++i)
		pj_memcpy(&mapped_addr[i], &addr, sizeof(addr));

	    mapped_addr[SIP_SOCK].sin_port = pj_htons((pj_uint16_t)global.sip_port);
	    mapped_addr[RTP_SOCK].sin_port = pj_htons((pj_uint16_t)rtp_port);
	    mapped_addr[RTCP_SOCK].sin_port = pj_htons((pj_uint16_t)(rtp_port+1));
	    break;
	} else {
	    pj_status_t rc;
	    rc = pj_stun_get_mapped_addr( global.pf, 3, sock,
					  &global.stun_srv1, global.stun_port1,
					  &global.stun_srv2, global.stun_port2,
					  mapped_addr);
	    if (rc != 0) {
		PJ_LOG(3,(THIS_FILE, "Error: %s", pj_stun_get_err_msg(rc)));
		goto on_error;
	    }

	    if (pj_ntohs(mapped_addr[2].sin_port) == pj_ntohs(mapped_addr[1].sin_port)+1)
		break;

	    pj_sock_close(sock[RTP_SOCK]); sock[RTP_SOCK] = PJ_INVALID_SOCKET;
	    pj_sock_close(sock[RTCP_SOCK]); sock[RTCP_SOCK] = PJ_INVALID_SOCKET;
	}
    }

    if (sock[RTP_SOCK] == PJ_INVALID_SOCKET) {
	PJ_LOG(2,(THIS_FILE, "Unable to find appropriate RTP/RTCP ports combination"));
	goto on_error;
    }

    global.sip_sock = sock[SIP_SOCK];
    pj_memcpy(&global.sip_sock_name, &mapped_addr[SIP_SOCK], sizeof(pj_sockaddr_in));
    global.rtp_sock = sock[RTP_SOCK];
    pj_memcpy(&global.rtp_sock_name, &mapped_addr[RTP_SOCK], sizeof(pj_sockaddr_in));
    global.rtcp_sock = sock[RTCP_SOCK];
    pj_memcpy(&global.rtcp_sock_name, &mapped_addr[RTCP_SOCK], sizeof(pj_sockaddr_in));

    PJ_LOG(4,(THIS_FILE, "SIP UDP socket reachable at %s:%d",
	      pj_inet_ntoa(global.sip_sock_name.sin_addr), 
	      pj_ntohs(global.sip_sock_name.sin_port)));
    PJ_LOG(4,(THIS_FILE, "RTP socket reachable at %s:%d",
	      pj_inet_ntoa(global.rtp_sock_name.sin_addr), 
	      pj_ntohs(global.rtp_sock_name.sin_port)));
    PJ_LOG(4,(THIS_FILE, "RTCP UDP socket reachable at %s:%d",
	      pj_inet_ntoa(global.rtcp_sock_name.sin_addr), 
	      pj_ntohs(global.rtcp_sock_name.sin_port)));
    return 0;

on_error:
    for (i=0; i<3; ++i) {
	if (sock[i] != PJ_INVALID_SOCKET)
	    pj_sock_close(sock[i]);
    }
    return -1;
}

static void log_function(int level, const char *buffer, int len)
{
    /* Write to both stdout and file. */
    if (level <= global.app_log_level)
	pj_log_to_stdout(level, buffer, len);
    if (global.log_file) {
	fwrite(buffer, len, 1, global.log_file);
	fflush(global.log_file);
    }
}

/* Initialize stack. */
static pj_status_t init_stack()
{
    pj_status_t status;
    pj_sockaddr_in bind_addr;
    pj_sockaddr_in bind_name;
    const char *local_addr;
    static char local_uri[128];

    /* Optionally set logging file. */
    if (global.log_filename) {
	global.log_file = fopen(global.log_filename, "wt");
    }

    /* Initialize endpoint. This will also call initialization to all the
     * modules.
     */
    global.endpt = pjsip_endpt_create(global.pf);
    if (global.endpt == NULL) {
	return -1;
    }

    /* Set dialog callback. */
    pjsip_ua_set_dialog_callback(global.user_agent, &dlg_callback);

    /* Init listener's bound address and port. */
    pj_sockaddr_init2(&bind_addr, "0.0.0.0", global.sip_port);
    pj_sockaddr_init(&bind_name, pj_gethostname(), global.sip_port);

    /* Add UDP transport listener. */
    status = pjsip_endpt_create_udp_listener( global.endpt, global.sip_sock,
					      &global.sip_sock_name);
    if (status != 0)
	return -1;

    local_addr = pj_inet_ntoa(global.sip_sock_name.sin_addr);

#if PJ_HAS_TCP
    /* Add TCP transport listener. */
    status = pjsip_endpt_create_listener( global.endpt, PJSIP_TRANSPORT_TCP, 
					  &bind_addr, &bind_name);
    if (status != 0)
	return -1;
#endif

    /* Determine user_id to be put in Contact */
    if (global.local_uri.slen) {
	pj_pool_t *pool = pj_pool_create(global.pf, "parser", 1024, 0, NULL);
	pjsip_uri *uri;

	uri = pjsip_parse_uri(pool, global.local_uri.ptr, global.local_uri.slen, 0);
	if (uri) {
	    if (pj_stricmp2(pjsip_uri_get_scheme(uri), "sip")==0) {
		pjsip_url *url = (pjsip_url*)pjsip_uri_get_uri(uri);
		if (url->user.slen)
		    strncpy(global.user_id, url->user.ptr, url->user.slen);
	    }
	} 
	pj_pool_release(pool);
    } 
    
    if (global.user_id[0]=='\0') {
	pj_native_strcpy(global.user_id, "user");
    }

    /* build contact */
    global.real_contact.ptr = local_uri;
    global.real_contact.slen = 
	sprintf(local_uri, "<sip:%s@%s:%d>", global.user_id, local_addr, global.sip_port);

    if (global.contact.slen == 0)
	global.contact = global.real_contact;

    /* initialize local_uri with contact if it's not specified in cmdline */
    if (global.local_uri.slen == 0)
	global.local_uri = global.contact;

    /* Init proxy. */
    if (global.proxy.slen || global.outbound_proxy.slen) {
	int count = 0;
	pj_str_t proxy_url[2];

	if (global.outbound_proxy.slen) {
	    proxy_url[count++] = global.outbound_proxy;
	}
	if (global.proxy.slen) {
	    proxy_url[count++] = global.proxy;
	}

	if (pjsip_endpt_set_proxies(global.endpt, count, proxy_url) != 0) {
	    PJ_LOG(2,(THIS_FILE, "Error setting proxy address!"));
	    return -1;
	}
    }

    /* initialize SIP registration if registrar is configured */
    if (global.registrar_uri.slen) {
	global.regc = pjsip_regc_create( global.endpt, NULL, &regc_cb);
	pjsip_regc_init( global.regc, &global.registrar_uri, 
			 &global.local_uri, 
			 &global.local_uri,
			 1, &global.contact, 
			 global.reg_timeout);
	pjsip_regc_set_credentials( global.regc, global.cred_count, global.cred_info );
    }

    return PJ_SUCCESS;
}

/* Worker thread function, only used when threading is enabled. */
static void *PJ_THREAD_FUNC worker_thread(void *unused)
{
    PJ_UNUSED_ARG(unused)

    while (!global.worker_quit_flag) {
	pj_time_val timeout = { 0, 10 };
	pjsip_endpt_handle_events (global.endpt, &timeout);
    }
    return NULL;
}


/* Make call to the specified URI. */
static pjsip_dlg *make_call(pj_str_t *remote_uri)
{
    pjsip_dlg *dlg;
    pj_str_t local = global.contact;
    pj_str_t remote = *remote_uri;
    struct dialog_data *dlg_data;
    pjsip_tx_data *tdata;
    pj_media_sock_info sock_info;

    /* Create new dialog instance. */
    dlg = pjsip_ua_create_dialog(global.user_agent, PJSIP_ROLE_UAC);

    /* Attach our own user data. */
    dlg_data = pj_pool_calloc(dlg->pool, 1, sizeof(struct dialog_data));
    dlg->user_data = dlg_data;

    /* Create media session. */
    pj_memset(&sock_info, 0, sizeof(sock_info));
    sock_info.rtp_sock = global.rtp_sock;
    sock_info.rtcp_sock = global.rtcp_sock;
    pj_memcpy(&sock_info.rtp_addr_name, &global.rtp_sock_name, sizeof(pj_sockaddr_in));

    dlg_data->msession = pj_media_session_create (global.mmgr, &sock_info);
    dlg_data->x_ms_msg_session = -1;

    if (global.offer_x_ms_msg) {
	const pj_media_stream_info *minfo[32];
	unsigned cnt;

	cnt = pj_media_session_enum_streams(dlg_data->msession, 32, minfo);
	if (cnt > 0)
	    dlg_data->x_ms_msg_session = cnt;
    } 

    /* Initialize dialog with local and remote URI. */
    if (pjsip_dlg_init(dlg, &local, &remote, NULL) != PJ_SUCCESS) {
	pjsip_ua_destroy_dialog(dlg);
	return NULL;
    }

    /* Initialize credentials. */
    pjsip_dlg_set_credentials(dlg, global.cred_count, global.cred_info);

    /* Send INVITE! */
    tdata = pjsip_dlg_invite(dlg);
    tdata->msg->body = create_msg_body (dlg, 0);

    if (pjsip_dlg_send_msg(dlg, tdata) != PJ_SUCCESS) {
	pjsip_ua_destroy_dialog(dlg);
	return NULL;
    }

    return dlg;
}

/*
 * Callback to receive incoming IM message.
 */
static int on_incoming_im_msg(pjsip_rx_data *rdata)
{
    pjsip_msg *msg = rdata->msg;
    pjsip_msg_body *body = msg->body;
    int len;
    char to[128], from[128];


    len = pjsip_uri_print( PJSIP_URI_IN_CONTACT_HDR, 
			   rdata->from->uri, from, sizeof(from));
    if (len > 0) from[len] = '\0';
    else pj_native_strcpy(from, "<URL too long..>");

    len = pjsip_uri_print( PJSIP_URI_IN_CONTACT_HDR, 
			   rdata->to->uri, to, sizeof(to));
    if (len > 0) to[len] = '\0';
    else pj_native_strcpy(to, "<URL too long..>");

    PJ_LOG(3,(THIS_FILE, "Incoming instant message:"));
    
    printf("----- BEGIN INSTANT MESSAGE ----->\n");
    printf("From:\t%s\n", from);
    printf("To:\t%s\n", to);
    printf("Body:\n%.*s\n", (body ? body->len : 0), (body ? (char*)body->data : ""));
    printf("<------ END INSTANT MESSAGE ------\n");

    fflush(stdout);

    /* Must answer with final response. */
    return 200;
}

/*
 * Input URL.
 */
static pj_str_t *ui_input_url(pj_str_t *out, char *buf, int len, int *selection)
{
    int i;

    *selection = -1;

    printf("\nBuddy list:\n");
    printf("---------------------------------------\n");
    for (i=0; i<global.buddy_cnt; ++i) {
	printf(" %d\t%s  <%s>\n", i+1, global.buddy[i].ptr,
		(global.buddy_status[i]?"Online":"Offline"));
    }
    printf("-------------------------------------\n");

    printf("Choices\n"
	   "\t0        For current dialog.\n"
	   "\t[1-%02d]   Select from buddy list\n"
	   "\tURL      An URL\n"
	   , global.buddy_cnt);
    printf("Input: ");

    fflush(stdout);
    fgets(buf, len, stdin);
    buf[strlen(buf)-1] = '\0'; /* remove trailing newline. */

    while (isspace(*buf)) ++buf;

    if (!*buf || *buf=='\n' || *buf=='\r')
	return NULL;

    i = atoi(buf);

    if (i == 0) {
	if (isdigit(*buf)) {
	    *selection = 0;
	    *out = pj_str("0");
	    return out;
	} else {
	    if (verify_sip_url(buf) != 0) {
		puts("Invalid URL specified!");
		return NULL;
	    }
	    *out = pj_str(buf);
	    return out;
	}
    } else if (i > global.buddy_cnt || i < 0) {
	printf("Error: invalid selection!\n");
	return NULL;
    } else {
	*out = global.buddy[i-1];
	*selection = i;
	return out;
    }
}


static void generic_request_callback( void *token, pjsip_event *event )
{
    pjsip_transaction *tsx = event->obj.tsx;
    
    PJ_UNUSED_ARG(token)

    if (tsx->status_code/100 == 2) {
	PJ_LOG(3,(THIS_FILE, "Outgoing %.*s %d (%s)",
		  event->obj.tsx->method.name.slen,
		  event->obj.tsx->method.name.ptr,
		  tsx->status_code,
		  pjsip_get_status_text(tsx->status_code)->ptr));
    } else if (tsx->status_code==401 || tsx->status_code==407)  {
	pjsip_tx_data *tdata;
	tdata = pjsip_auth_reinit_req( global.endpt,
				       global.pool, NULL, global.cred_count, global.cred_info,
				       tsx->last_tx, event->src.rdata);
	if (tdata) {
	    int rc;
	    pjsip_cseq_hdr *cseq;
	    cseq = (pjsip_cseq_hdr*)pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CSEQ, NULL);
	    cseq->cseq++;
	    rc = pjsip_endpt_send_request( global.endpt, tdata, -1, NULL, 
					    &generic_request_callback);
	    if (rc == 0)
		return;
	}
	PJ_LOG(2,(THIS_FILE, "Outgoing %.*s failed, status=%d (%s)",
		  event->obj.tsx->method.name.slen,
		  event->obj.tsx->method.name.ptr,
		  event->obj.tsx->status_code,
		  pjsip_get_status_text(event->obj.tsx->status_code)->ptr));
    } else {
	const pj_str_t *reason;
	if (event->src_type == PJSIP_EVENT_RX_MSG)
	    reason = &event->src.rdata->msg->line.status.reason;
	else
	    reason = pjsip_get_status_text(tsx->status_code);
	PJ_LOG(2,(THIS_FILE, "Outgoing %.*s failed, status=%d (%.*s)",
		  event->obj.tsx->method.name.slen,
		  event->obj.tsx->method.name.ptr,
		  event->obj.tsx->status_code,
		  reason->slen, reason->ptr));
    }
}


static void ui_send_im_message()
{
    char line[100];
    char text_buf[100];
    pj_str_t str;
    pj_str_t text_msg;
    int selection, rc;
    pjsip_tx_data *tdata;
  
    if (ui_input_url(&str, line, sizeof(line), &selection) == NULL)
	return;

	
    printf("Enter text to send (empty to cancel): "); fflush(stdout);
    fgets(text_buf, sizeof(text_buf), stdin);
    text_buf[strlen(text_buf)-1] = '\0';
    if (!*text_buf)
	return;

    text_msg = pj_str(text_buf);
    
    if (selection==0) {
	pjsip_method message_method;
	pj_str_t str_MESSAGE = { "MESSAGE", 7 };

	/* Send IM to current dialog. */
	if (global.cur_dlg == NULL || global.cur_dlg->state != PJSIP_DIALOG_STATE_ESTABLISHED) {
	    printf("No current dialog or dialog state is not ESTABLISHED!\n");
	    return;
	}

	pjsip_method_init( &message_method, global.cur_dlg->pool, &str_MESSAGE);
	tdata = pjsip_dlg_create_request( global.cur_dlg, &message_method, -1 );

	if (tdata) {
	    /* Create message body for the text. */
	    pjsip_msg_body *body = pj_pool_calloc(tdata->pool, 1, sizeof(*body));
	    body->content_type.type = pj_str("text");
	    body->content_type.subtype = pj_str("plain");
	    body->data = pj_pool_alloc(tdata->pool, text_msg.slen);
	    pj_memcpy(body->data, text_msg.ptr, text_msg.slen);
	    body->len = text_msg.slen;
	    body->print_body = &pjsip_print_text_body;

	    /* Assign body to message, and send the message! */
	    tdata->msg->body = body;
	    pjsip_dlg_send_msg( global.cur_dlg, tdata );
	}

    } else {
	/* Send IM to buddy list. */
	pjsip_method message;
	static pj_str_t MESSAGE = { "MESSAGE", 7 };
	pjsip_method_init_np(&message, &MESSAGE);
	tdata = pjsip_endpt_create_request(global.endpt, &message, 
					   &str,
					   &global.real_contact,
				           &str, &global.real_contact, NULL, -1, 
					   &text_msg);
	if (!tdata) {
	    puts("Error creating request");
	    return;
	}
	rc = pjsip_endpt_send_request(global.endpt, tdata, -1, NULL, &generic_request_callback);
	if (rc == 0) {
	    printf("Sending IM message %d\n", global.im_counter);
	    ++global.im_counter;
	} else {
	    printf("Error: unable to send IM message!\n");
	}
    }
}

static void ui_send_options()
{
    char line[100];
    pj_str_t str;
    int selection, rc;
    pjsip_tx_data *tdata;
    pjsip_method options;

    if (ui_input_url(&str, line, sizeof(line), &selection) == NULL)
	return;

    pjsip_method_set( &options, PJSIP_OPTIONS_METHOD );

    if (selection == 0) {
	/* Send OPTIONS to current dialog. */
	tdata = pjsip_dlg_create_request(global.cur_dlg, &options, -1);
	if (tdata)
	    pjsip_dlg_send_msg( global.cur_dlg, tdata );
    } else {
	/* Send OPTIONS to arbitrary party. */
	tdata = pjsip_endpt_create_request( global.endpt, &options,
					    &str,
					    &global.local_uri, &str, 
					    &global.real_contact,
					    NULL, -1, NULL);
	if (tdata) {
	    rc = pjsip_endpt_send_request( global.endpt, tdata, -1, NULL, 
					   &generic_request_callback);
	    if (rc != 0)
		PJ_LOG(2,(THIS_FILE, "Error sending OPTIONS!"));
	}
    }
}

static void init_presence()
{
    const pjsip_presence_cb pres_cb = {
	NULL,
	&pres_on_received_request,
	&pres_on_received_refresh,
	&pres_on_received_update,
	&pres_on_terminated
    };

    pjsip_presence_init(&pres_cb);
}

/* Subscribe presence information for all buddies. */
static void subscribe_buddies_presence()
{
    int i;
    for (i=0; i<global.buddy_cnt; ++i) {
	pjsip_presentity *pres;
	if (global.buddy_pres[i])
	    continue;
	pres = pjsip_presence_create( global.endpt, &global.local_uri,
				      &global.buddy[i], PRESENCE_TIMEOUT, (void*)i);
	if (pres) {
	    pjsip_presence_set_credentials( pres, global.cred_count, global.cred_info );
	    pjsip_presence_subscribe( pres );
	}
	global.buddy_pres[i] = pres;
    }
}

/* Unsubscribe presence information for all buddies. */
static void unsubscribe_buddies_presence()
{
    int i;
    for (i=0; i<global.buddy_cnt; ++i) {
	pjsip_presentity *pres = global.buddy_pres[i];
	if (pres) {
	    pjsip_presence_unsubscribe(pres);
	    pjsip_presence_destroy(pres);
	    global.buddy_pres[i] = NULL;
	}
    }
}

/* Unsubscribe presence. */
static void unsubscribe_presence()
{
    int i;

    unsubscribe_buddies_presence();
    for (i=0; i<global.pres_cnt; ++i) {
	pjsip_presentity *pres = global.pres[i];
	pjsip_presence_notify( pres, PJSIP_EVENT_SUB_STATE_TERMINATED, 0);
	pjsip_presence_destroy( pres );
    }
}

/* Advertise online status to subscribers. */
static void update_im_status()
{
    int i;
    for (i=0; i<global.pres_cnt; ++i) {
	pjsip_presentity *pres = global.pres[i];
	pjsip_presence_notify( pres, PJSIP_EVENT_SUB_STATE_ACTIVE, 
			       !global.hide_status);
    }
}

/*
 * Main program.
 */
int main(int argc, char *argv[])
{
    /* set to WORKER_COUNT+1 to avoid zero size warning 
     * when threading is disabled. */
    pj_thread_t *thread[WORKER_COUNT+1];
    pj_caching_pool cp;
    int i;

    global.sip_port = 5060;
    global.auto_answer = -1;
    global.auto_hangup = -1;
    global.app_log_level = 3;

    pj_log_set_level(4);
    pj_log_set_log_func(&log_function);

    /* Init PJLIB */
    if (pj_init() != PJ_SUCCESS)
	return 1;

    /* Init caching pool. */
    pj_caching_pool_init(&cp, &pj_pool_factory_default_policy, 0);
    global.pf = &cp.factory;

    /* Create memory pool for application. */
    global.pool = pj_pool_create(global.pf, "main", 1024, 0, NULL);

    /* Parse command line arguments. */
    if (parse_args(global.pool, argc, argv) != PJ_SUCCESS) {
	pj_caching_pool_destroy(&cp);
	return 1;
    }

    /* Init sockets */
    if (init_sockets() != 0) {
	pj_caching_pool_destroy(&cp);
	return 1;
    }

    /* Initialize stack. */
    if (init_stack() != PJ_SUCCESS) {
	pj_caching_pool_destroy(&cp);
	return 1;
    }

    /* Set callback to receive incoming IM */
    pjsip_messaging_set_incoming_callback( &on_incoming_im_msg );

    /* Set default worker count (can be zero) */
    global.worker_cnt = WORKER_COUNT;

    /* Create user worker thread(s), only when threading is enabled. */
    for (i=0; i<global.worker_cnt; ++i) {
	thread[i] = pj_thread_create( global.pool, "sip%p", 
				      &worker_thread, 
				      NULL, 0, NULL, 0);
	if (thread == NULL) {
	    global.worker_quit_flag = 1;
	    for (--i; i>=0; --i) {
		pj_thread_join(thread[i]);
		pj_thread_destroy(thread[i]);
	    }
	    pj_caching_pool_destroy(&cp);
	    return 1;
	}
    }

    printf("Worker thread count: %d\n", global.worker_cnt);

    /* Perform registration, if required. */
    if (global.regc) {
	update_registration(global.regc, 1);
    }

    /* Initialize media manager. */
    global.mmgr = pj_med_mgr_create(global.pf);

    /* Init presence. */
    init_presence();

    /* Subscribe presence information of all buddies. */
    if (!global.no_presence)
	subscribe_buddies_presence();

    /* Initializatio completes, loop waiting for commands. */
    for (;!global.worker_quit_flag;) {
	pj_str_t str;
	char line[128];

#if WORKER_COUNT==0
	/* If worker thread does not exist, main thread must poll for evetns. 
	 * But this won't work very well since main thread is blocked by 
	 * fgets(). So keep pressing the ENTER key to get the events!
	 */
	pj_time_val timeout = { 0, 100 };
	pjsip_endpt_handle_events(global.endpt, &timeout);
	puts("Keep pressing ENTER key to get the events!");
#endif

	printf("\nCurrent dialog: ");
	print_dialog(global.cur_dlg);
	puts("");

	keystroke_help();

	fgets(line, sizeof(line), stdin);

	switch (*line) {
	case 'm':
	    puts("Make outgoing call");
	    if (ui_input_url(&str, line, sizeof(line), &i) != NULL) {
		pjsip_dlg *dlg = make_call(&str);
		if (global.cur_dlg == NULL) {
		    global.cur_dlg = dlg;
		}
	    }
	    break;
	case 'i':
	    puts("Send Instant Messaging");
	    ui_send_im_message();
	    break;
	case 'o':
	    puts("Send OPTIONS");
	    ui_send_options();
	    break;
	case 'a':
	    if (global.cur_dlg) {
		unsigned code;
		pjsip_tx_data *tdata;
		struct dialog_data *dlg_data = global.cur_dlg->user_data;

		printf("Answer with status code (1xx-6xx): ");
		fflush(stdout);
		fgets(line, sizeof(line), stdin);
		str = pj_str(line);
		str.slen -= 1;

		code = pj_strtoul(&str);
		tdata = pjsip_dlg_answer(global.cur_dlg, code);
		if (tdata) {
		    if (code/100 == 2) {
			tdata->msg->body = dlg_data->body;
		    }
		    pjsip_dlg_send_msg(global.cur_dlg, tdata);

		}
	    } else {
		puts("No current dialog");
	    }
	    break;
	case 'h':
	    if (global.cur_dlg) {
		pjsip_tx_data *tdata;
		tdata = pjsip_dlg_disconnect(global.cur_dlg, PJSIP_SC_DECLINE);
		if (tdata) {
		    pjsip_dlg_send_msg(global.cur_dlg, tdata);
		}
	    } else {
		puts("No current dialog");
	    }
	    break;
	case ']':
	    if (global.cur_dlg) {
		global.cur_dlg = global.cur_dlg->next;
		if (global.cur_dlg == (void*)&global.user_agent->dlg_list) {
		    global.cur_dlg = global.cur_dlg->next;
		}
	    } else {
		puts("No current dialog");
	    }
	    break;
	case '[':
	    if (global.cur_dlg) {
		global.cur_dlg = global.cur_dlg->prev;
		if (global.cur_dlg == (void*)&global.user_agent->dlg_list) {
		    global.cur_dlg = global.cur_dlg->prev;
		}
	    } else {
		puts("No current dialog");
	    }
	    break;
	case 'd':
	    pjsip_endpt_dump(global.endpt, *(line+1)=='1');
	    pjsip_ua_dump(global.user_agent);
	    break;
	case 's':
	    if (*(line+1) == 'u')
		subscribe_buddies_presence();
	    break;
	case 'u':
	    if (*(line+1) == 's')
		unsubscribe_presence();
	    break;
	case 't':
	    global.hide_status = !global.hide_status;
	    update_im_status();
	    break;
	case 'q':
	    goto on_exit;
	case 'l':
	    print_all_dialogs();
	    break;
	}
    }

on_exit:
    /* Unregister, if required. */
    if (global.regc) {
	update_registration(global.regc, 0);
    }

    /* Unsubscribe presence. */
    unsubscribe_presence();

    /* Allow one second to get all events. */
    if (1) {
	pj_time_val end_time;

	pj_gettimeofday(&end_time);
	end_time.sec++;

	PJ_LOG(3,(THIS_FILE, "Shutting down.."));
	for (;;) {
	    pj_time_val timeout = { 0, 20 }, now;
	    pjsip_endpt_handle_events (global.endpt, &timeout);
	    pj_gettimeofday(&now);
	    PJ_TIME_VAL_SUB(now, end_time);
	    if (now.sec >= 1)
		break;
	}
    }

    global.worker_quit_flag = 1;

    pj_med_mgr_destroy(global.mmgr);

    /* Wait all threads to quit. */
    for (i=0; i<global.worker_cnt; ++i) {
	pj_thread_join(thread[i]);
	pj_thread_destroy(thread[i]);
    }

    /* Destroy endpoint. */
    pjsip_endpt_destroy(global.endpt);

    /* Destroy caching pool. */
    pj_caching_pool_destroy(&cp);

    /* Close log file, if any. */
    if (global.log_file)
	fclose(global.log_file);

    return 0;
}

/*
 * Register static modules to the endpoint.
 */
pj_status_t register_static_modules( pj_size_t *count,
				     pjsip_module **modules )
{
    /* Reset count. */
    *count = 0;

    /* Register user agent module. */
    modules[(*count)++] = pjsip_ua_get_module();
    global.user_agent = modules[0]->mod_data;
    modules[(*count)++] = pjsip_messaging_get_module();
    modules[(*count)++] = pjsip_event_sub_get_module();

    return PJ_SUCCESS;
}
