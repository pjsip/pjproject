/* $Id: callbacks.c.template 4566 2013-07-17 20:20:50Z nanang $ */

#include "callbacks.h"

/*
 * Wrapper of pjsua_logging_config callback
 */
static PjsuaLoggingConfigCallback *pjsua_logging_config_cb = NULL;

void pjsua_logging_config_callback_proxy(int level, const char *data, int len)
{
  PJ_UNUSED_ARG(len);
  pjsua_logging_config_cb->on_log(level, data);
}

void setPjsuaLoggingConfigCallback(PjsuaLoggingConfigCallback *callback)
{
  pjsua_logging_config_cb = callback;
}


/*
 * Wrapper of pj_timer_heap_callback
 */
void pj_timer_heap_callback_proxy(pj_timer_heap_t *timer_heap, struct pj_timer_entry *entry)
{
  PjTimerHeapCallback *cb = (PjTimerHeapCallback*)entry->user_data;
  pj_assert(cb);
  cb->on_timer(timer_heap, entry);
}

void setPjTimerHeapCallback(pj_timer_entry *entry, PjTimerHeapCallback *callback)
{
  entry->user_data = callback;
}


/*
 * Wrapper of pjsua_callback
 */
static PjsuaCallback* cb = NULL;

void setPjsuaCallback(PjsuaCallback* callback) {
  cb = callback;
}

static void on_call_state (pjsua_call_id call_id, pjsip_event *e) { cb->on_call_state(call_id,e); }
static void on_incoming_call (pjsua_acc_id acc_id, pjsua_call_id call_id, pjsip_rx_data *rdata) { cb->on_incoming_call(acc_id,call_id,rdata); }
static void on_call_tsx_state (pjsua_call_id call_id, pjsip_transaction *tsx, pjsip_event *e) { cb->on_call_tsx_state(call_id,tsx,e); }
static void on_call_media_state (pjsua_call_id call_id) { cb->on_call_media_state(call_id); }
static void on_call_sdp_created (pjsua_call_id call_id, pjmedia_sdp_session *sdp, pj_pool_t *pool, const pjmedia_sdp_session *rem_sdp) { cb->on_call_sdp_created(call_id,sdp,pool,rem_sdp); }
static void on_stream_created (pjsua_call_id call_id, pjmedia_stream *strm, unsigned stream_idx, pjmedia_port **p_port) { cb->on_stream_created(call_id,strm,stream_idx,p_port); }
static void on_stream_destroyed (pjsua_call_id call_id, pjmedia_stream *strm, unsigned stream_idx) { cb->on_stream_destroyed(call_id,strm,stream_idx); }
static void on_dtmf_digit (pjsua_call_id call_id, int digit) { cb->on_dtmf_digit(call_id,digit); }
static void on_call_transfer_request (pjsua_call_id call_id, const pj_str_t *dst, pjsip_status_code *code) { cb->on_call_transfer_request(call_id,dst,code); }
static void on_call_transfer_request2 (pjsua_call_id call_id, const pj_str_t *dst, pjsip_status_code *code, pjsua_call_setting *opt) { cb->on_call_transfer_request2(call_id,dst,code,opt); }
static void on_call_transfer_status (pjsua_call_id call_id, int st_code, const pj_str_t *st_text, pj_bool_t final, pj_bool_t *p_cont) { cb->on_call_transfer_status(call_id,st_code,st_text,final,p_cont); }
static void on_call_replace_request (pjsua_call_id call_id, pjsip_rx_data *rdata, int *st_code, pj_str_t *st_text) { cb->on_call_replace_request(call_id,rdata,st_code,st_text); }
static void on_call_replace_request2 (pjsua_call_id call_id, pjsip_rx_data *rdata, int *st_code, pj_str_t *st_text, pjsua_call_setting *opt) { cb->on_call_replace_request2(call_id,rdata,st_code,st_text,opt); }
static void on_call_replaced (pjsua_call_id old_call_id, pjsua_call_id new_call_id) { cb->on_call_replaced(old_call_id,new_call_id); }
static void on_call_rx_offer (pjsua_call_id call_id, const pjmedia_sdp_session *offer, void *reserved, pjsip_status_code *code, pjsua_call_setting *opt) { cb->on_call_rx_offer(call_id,offer,reserved,code,opt); }
static void on_reg_started (pjsua_acc_id acc_id, pj_bool_t renew) { cb->on_reg_started(acc_id,renew); }
static void on_reg_state (pjsua_acc_id acc_id) { cb->on_reg_state(acc_id); }
static void on_reg_state2 (pjsua_acc_id acc_id, pjsua_reg_info *info) { cb->on_reg_state2(acc_id,info); }
static void on_incoming_subscribe (pjsua_acc_id acc_id, pjsua_srv_pres *srv_pres, pjsua_buddy_id buddy_id, const pj_str_t *from, pjsip_rx_data *rdata, pjsip_status_code *code, pj_str_t *reason, pjsua_msg_data *msg_data) { cb->on_incoming_subscribe(acc_id,srv_pres,buddy_id,from,rdata,code,reason,msg_data); }
static void on_srv_subscribe_state (pjsua_acc_id acc_id, pjsua_srv_pres *srv_pres, const pj_str_t *remote_uri, pjsip_evsub_state state, pjsip_event *event) { cb->on_srv_subscribe_state(acc_id,srv_pres,remote_uri,state,event); }
static void on_buddy_state (pjsua_buddy_id buddy_id) { cb->on_buddy_state(buddy_id); }
static void on_buddy_evsub_state (pjsua_buddy_id buddy_id, pjsip_evsub *sub, pjsip_event *event) { cb->on_buddy_evsub_state(buddy_id,sub,event); }
static void on_pager (pjsua_call_id call_id, const pj_str_t *from, const pj_str_t *to, const pj_str_t *contact, const pj_str_t *mime_type, const pj_str_t *body) { cb->on_pager(call_id,from,to,contact,mime_type,body); }
static void on_pager2 (pjsua_call_id call_id, const pj_str_t *from, const pj_str_t *to, const pj_str_t *contact, const pj_str_t *mime_type, const pj_str_t *body, pjsip_rx_data *rdata, pjsua_acc_id acc_id) { cb->on_pager2(call_id,from,to,contact,mime_type,body,rdata,acc_id); }
static void on_pager_status (pjsua_call_id call_id, const pj_str_t *to, const pj_str_t *body, void *user_data, pjsip_status_code status, const pj_str_t *reason) { cb->on_pager_status(call_id,to,body,user_data,status,reason); }
static void on_pager_status2 (pjsua_call_id call_id, const pj_str_t *to, const pj_str_t *body, void *user_data, pjsip_status_code status, const pj_str_t *reason, pjsip_tx_data *tdata, pjsip_rx_data *rdata, pjsua_acc_id acc_id) { cb->on_pager_status2(call_id,to,body,user_data,status,reason,tdata,rdata,acc_id); }
static void on_typing (pjsua_call_id call_id, const pj_str_t *from, const pj_str_t *to, const pj_str_t *contact, pj_bool_t is_typing) { cb->on_typing(call_id,from,to,contact,is_typing); }
static void on_typing2 (pjsua_call_id call_id, const pj_str_t *from, const pj_str_t *to, const pj_str_t *contact, pj_bool_t is_typing, pjsip_rx_data *rdata, pjsua_acc_id acc_id) { cb->on_typing2(call_id,from,to,contact,is_typing,rdata,acc_id); }
static void on_nat_detect (const pj_stun_nat_detect_result *res) { cb->on_nat_detect(res); }
static pjsip_redirect_op on_call_redirected (pjsua_call_id call_id, const pjsip_uri *target, const pjsip_event *e) { return cb->on_call_redirected(call_id,target,e); }
static void on_mwi_state (pjsua_acc_id acc_id, pjsip_evsub *evsub) { cb->on_mwi_state(acc_id,evsub); }
static void on_mwi_info (pjsua_acc_id acc_id, pjsua_mwi_info *mwi_info) { cb->on_mwi_info(acc_id,mwi_info); }
static void on_transport_state (pjsip_transport *tp, pjsip_transport_state state, const pjsip_transport_state_info *info) { cb->on_transport_state(tp,state,info); }
static pj_status_t on_call_media_transport_state (pjsua_call_id call_id, const pjsua_med_tp_state_info *info) { return cb->on_call_media_transport_state(call_id,info); }
static void on_ice_transport_error (int index, pj_ice_strans_op op, pj_status_t status, void *param) { cb->on_ice_transport_error(index,op,status,param); }
static pj_status_t on_snd_dev_operation (int operation) { return cb->on_snd_dev_operation(operation); }
static void on_call_media_event (pjsua_call_id call_id, unsigned med_idx, pjmedia_event *event) { cb->on_call_media_event(call_id,med_idx,event); }
static pjmedia_transport * on_create_media_transport (pjsua_call_id call_id, unsigned media_idx, pjmedia_transport *base_tp, unsigned flags) { return cb->on_create_media_transport(call_id,media_idx,base_tp,flags); }
static void on_acc_find_for_incoming (const pjsip_rx_data *rdata, pjsua_acc_id *acc_id) { cb->on_acc_find_for_incoming(rdata,acc_id); }

static pjsua_callback my_cb_proxy = {
  &on_call_state,
  &on_incoming_call,
  &on_call_tsx_state,
  &on_call_media_state,
  &on_call_sdp_created,
  &on_stream_created,
  &on_stream_destroyed,
  &on_dtmf_digit,
  &on_call_transfer_request,
  &on_call_transfer_request2,
  &on_call_transfer_status,
  &on_call_replace_request,
  &on_call_replace_request2,
  &on_call_replaced,
  &on_call_rx_offer,
  &on_reg_started,
  &on_reg_state,
  &on_reg_state2,
  &on_incoming_subscribe,
  &on_srv_subscribe_state,
  &on_buddy_state,
  &on_buddy_evsub_state,
  &on_pager,
  &on_pager2,
  &on_pager_status,
  &on_pager_status2,
  &on_typing,
  &on_typing2,
  &on_nat_detect,
  &on_call_redirected,
  &on_mwi_state,
  &on_mwi_info,
  &on_transport_state,
  &on_call_media_transport_state,
  &on_ice_transport_error,
  &on_snd_dev_operation,
  &on_call_media_event,
  &on_create_media_transport,
  &on_acc_find_for_incoming
};

pjsua_callback* pjsua_callback_proxy = &my_cb_proxy;
