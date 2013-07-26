/* $Id: callbacks.h.template 4566 2013-07-17 20:20:50Z nanang $ */

#include <pjsua-lib/pjsua.h>

/*
 * Wrapper of pjsua_logging_config callback
 */
class PjsuaLoggingConfigCallback {
public:
  virtual void on_log(int level, const char *data) {}
  virtual ~PjsuaLoggingConfigCallback() {}
};

#ifndef SWIG /* SWIG should ignore these */
extern void pjsua_logging_config_callback_proxy(int level, const char *data, int len);
extern void setPjsuaLoggingConfigCallback(PjsuaLoggingConfigCallback *callback);
#endif


/*
 * Wrapper of pj_timer_heap_callback
 */
class PjTimerHeapCallback {
public:
  virtual void on_timer(pj_timer_heap_t *timer_heap, pj_timer_entry *entry) {}
  virtual ~PjTimerHeapCallback() {}
};

#ifndef SWIG /* SWIG should ignore these */
extern void pj_timer_heap_callback_proxy(pj_timer_heap_t *timer_heap, pj_timer_entry *entry);
extern void setPjTimerHeapCallback(pj_timer_entry *entry, PjTimerHeapCallback *callback);
#endif


/*
 * Wrapper of pjsua_callback
 */
class PjsuaCallback {
public:
  virtual void on_call_state (pjsua_call_id call_id, pjsip_event *e) {}
  virtual void on_incoming_call (pjsua_acc_id acc_id, pjsua_call_id call_id, pjsip_rx_data *rdata) {}
  virtual void on_call_tsx_state (pjsua_call_id call_id, pjsip_transaction *tsx, pjsip_event *e) {}
  virtual void on_call_media_state (pjsua_call_id call_id) {}
  virtual void on_call_sdp_created (pjsua_call_id call_id, pjmedia_sdp_session *sdp, pj_pool_t *pool, const pjmedia_sdp_session *rem_sdp) {}
  virtual void on_stream_created (pjsua_call_id call_id, pjmedia_stream *strm, unsigned stream_idx, pjmedia_port **p_port) {}
  virtual void on_stream_destroyed (pjsua_call_id call_id, pjmedia_stream *strm, unsigned stream_idx) {}
  virtual void on_dtmf_digit (pjsua_call_id call_id, int digit) {}
  virtual void on_call_transfer_request (pjsua_call_id call_id, const pj_str_t *dst, pjsip_status_code *code) {}
  virtual void on_call_transfer_request2 (pjsua_call_id call_id, const pj_str_t *dst, pjsip_status_code *code, pjsua_call_setting *opt) {}
  virtual void on_call_transfer_status (pjsua_call_id call_id, int st_code, const pj_str_t *st_text, pj_bool_t final, pj_bool_t *p_cont) {}
  virtual void on_call_replace_request (pjsua_call_id call_id, pjsip_rx_data *rdata, int *st_code, pj_str_t *st_text) {}
  virtual void on_call_replace_request2 (pjsua_call_id call_id, pjsip_rx_data *rdata, int *st_code, pj_str_t *st_text, pjsua_call_setting *opt) {}
  virtual void on_call_replaced (pjsua_call_id old_call_id, pjsua_call_id new_call_id) {}
  virtual void on_call_rx_offer (pjsua_call_id call_id, const pjmedia_sdp_session *offer, void *reserved, pjsip_status_code *code, pjsua_call_setting *opt) {}
  virtual void on_reg_started (pjsua_acc_id acc_id, pj_bool_t renew) {}
  virtual void on_reg_state (pjsua_acc_id acc_id) {}
  virtual void on_reg_state2 (pjsua_acc_id acc_id, pjsua_reg_info *info) {}
  virtual void on_incoming_subscribe (pjsua_acc_id acc_id, pjsua_srv_pres *srv_pres, pjsua_buddy_id buddy_id, const pj_str_t *from, pjsip_rx_data *rdata, pjsip_status_code *code, pj_str_t *reason, pjsua_msg_data *msg_data) {}
  virtual void on_srv_subscribe_state (pjsua_acc_id acc_id, pjsua_srv_pres *srv_pres, const pj_str_t *remote_uri, pjsip_evsub_state state, pjsip_event *event) {}
  virtual void on_buddy_state (pjsua_buddy_id buddy_id) {}
  virtual void on_buddy_evsub_state (pjsua_buddy_id buddy_id, pjsip_evsub *sub, pjsip_event *event) {}
  virtual void on_pager (pjsua_call_id call_id, const pj_str_t *from, const pj_str_t *to, const pj_str_t *contact, const pj_str_t *mime_type, const pj_str_t *body) {}
  virtual void on_pager2 (pjsua_call_id call_id, const pj_str_t *from, const pj_str_t *to, const pj_str_t *contact, const pj_str_t *mime_type, const pj_str_t *body, pjsip_rx_data *rdata, pjsua_acc_id acc_id) {}
  virtual void on_pager_status (pjsua_call_id call_id, const pj_str_t *to, const pj_str_t *body, void *user_data, pjsip_status_code status, const pj_str_t *reason) {}
  virtual void on_pager_status2 (pjsua_call_id call_id, const pj_str_t *to, const pj_str_t *body, void *user_data, pjsip_status_code status, const pj_str_t *reason, pjsip_tx_data *tdata, pjsip_rx_data *rdata, pjsua_acc_id acc_id) {}
  virtual void on_typing (pjsua_call_id call_id, const pj_str_t *from, const pj_str_t *to, const pj_str_t *contact, pj_bool_t is_typing) {}
  virtual void on_typing2 (pjsua_call_id call_id, const pj_str_t *from, const pj_str_t *to, const pj_str_t *contact, pj_bool_t is_typing, pjsip_rx_data *rdata, pjsua_acc_id acc_id) {}
  virtual void on_nat_detect (const pj_stun_nat_detect_result *res) {}
  virtual pjsip_redirect_op on_call_redirected (pjsua_call_id call_id, const pjsip_uri *target, const pjsip_event *e) { return PJSIP_REDIRECT_STOP; }
  virtual void on_mwi_state (pjsua_acc_id acc_id, pjsip_evsub *evsub) {}
  virtual void on_mwi_info (pjsua_acc_id acc_id, pjsua_mwi_info *mwi_info) {}
  virtual void on_transport_state (pjsip_transport *tp, pjsip_transport_state state, const pjsip_transport_state_info *info) {}
  virtual pj_status_t on_call_media_transport_state (pjsua_call_id call_id, const pjsua_med_tp_state_info *info) { return 0; }
  virtual void on_ice_transport_error (int index, pj_ice_strans_op op, pj_status_t status, void *param) {}
  virtual pj_status_t on_snd_dev_operation (int operation) { return 0; }
  virtual void on_call_media_event (pjsua_call_id call_id, unsigned med_idx, pjmedia_event *event) {}
  virtual pjmedia_transport * on_create_media_transport (pjsua_call_id call_id, unsigned media_idx, pjmedia_transport *base_tp, unsigned flags) { return base_tp; }
  virtual void on_acc_find_for_incoming (const pjsip_rx_data *rdata, pjsua_acc_id *acc_id) {}
  virtual ~PjsuaCallback() {}
};

#ifndef SWIG /* SWIG should ignore these */
extern void setPjsuaCallback(PjsuaCallback* callback);
extern pjsua_callback* pjsua_callback_proxy;
#endif
