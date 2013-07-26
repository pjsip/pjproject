/* $Id: header.i 4566 2013-07-17 20:20:50Z nanang $ */

/* TODO:
 * - fix memory leaks in pj_str_t - java String typemaps
 * - typemap for pj_str_t as output param for callback/director,
 *   e.g: "st_text" param in pjsua_callback::on_call_replace_request()
 * - workaround for nested struct/union, i.e: moving out inner classes to global scope
 */

%module (directors="1") pjsua

%include "enums.swg"
%include "../my_typemaps.i"

%header %{
    #include <pjsua-lib/pjsua.h>
%}

/* Strip "pjsua_" prefix from pjsua functions, for better compatibility with 
 * pjsip-jni & csipsimple.
 */
%rename("%(strip:[pjsua_])s", %$isfunction) "";

/* Suppress pjsua_schedule_timer2(), app can still use pjsua_schedule_timer() */
%ignore pjsua_schedule_timer2;

/* Suppress aux function pjsua_resolve_stun_servers(), usually app won't need this
 * as app can just simply configure STUN server to use STUN.
 */
%ignore pjsua_resolve_stun_servers;

/* Map 'void *' simply as long, app can use this "long" as index of its real user data */
%apply long long { void * };

/* Handle void *[ANY], e.g: pjsip_tx_data::mod_data, pjsip_transaction::mod_data */
//%ignore pjsip_tx_data::mod_data;
//%ignore pjsip_transaction::mod_data;
%apply long long[ANY]   { void *[ANY] };

/* Map pj_bool_t */
%apply bool { pj_bool_t };

/* Map pjsua_call_dump() output buffer */
%apply (char *STRING, size_t LENGTH) { (char *buffer, unsigned maxlen) };

/* Map "int*" & "unsigned*" as input & output */
%apply unsigned	*INOUT  { unsigned * };
%apply int      *INOUT  { int * };

/* Map the following args as input & output */
%apply int      *INOUT	{ pj_stun_nat_type * };
%apply int      *INOUT	{ pjsip_status_code * };
%apply pj_str_t *INOUT  { pj_str_t *p_contact };

/* Apply array of integer on array of enum */
%apply int[ANY]         { pjmedia_format_id dec_fmt_id[ANY] };

/* Handle members typed array of pj_str_t */
MY_JAVA_MEMBER_ARRAY_OF_STR(pjsua_config, nameserver, nameserver_count)
MY_JAVA_MEMBER_ARRAY_OF_STR(pjsua_config, outbound_proxy, outbound_proxy_cnt)
MY_JAVA_MEMBER_ARRAY_OF_STR(pjsua_config, stun_srv, stun_srv_cnt)
MY_JAVA_MEMBER_ARRAY_OF_STR(pjsua_acc_config, proxy, proxy_cnt)
MY_JAVA_MEMBER_ARRAY_OF_STR(pjsua_config, stun_srv, stun_srv_cnt)
MY_JAVA_MEMBER_ARRAY_OF_STR(pjsua_config, stun_srv, stun_srv_cnt)
MY_JAVA_MEMBER_ARRAY_OF_STR(pjsip_generic_array_hdr, values, count)

/* Handle pointer-to-pointer-to-object as input & output */
MY_JAVA_CLASS_INOUT(pjmedia_port, p_port)
MY_JAVA_CLASS_INOUT(pjsip_tx_data, p_tdata)

/* Handle array of pj_ssl_cipher in pjsip_tls_setting. */
MY_JAVA_MEMBER_ARRAY_OF_ENUM(pjsip_tls_setting, pj_ssl_cipher, ciphers, ciphers_num)

/* Handle array of pointer in struct/class member */
MY_JAVA_MEMBER_ARRAY_OF_POINTER(pjsip_regc_cbparam, pjsip_contact_hdr, contact, contact_cnt)
MY_JAVA_MEMBER_ARRAY_OF_POINTER(pjmedia_sdp_session, pjmedia_sdp_media, media, media_count)
MY_JAVA_MEMBER_ARRAY_OF_POINTER(pjmedia_sdp_media, pjmedia_sdp_bandw, bandw, bandw_count)
MY_JAVA_MEMBER_ARRAY_OF_POINTER(pjmedia_sdp_media, pjmedia_sdp_attr, attr, attr_count)
MY_JAVA_MEMBER_ARRAY_OF_POINTER(pjsua_acc_config, pjsip_cred_info, cred_info, cred_count)

%include "../callbacks.i"

/* Global constants */
#define PJ_SUCCESS  0

enum {PJ_TERM_COLOR_R = 2, PJ_TERM_COLOR_G = 4, PJ_TERM_COLOR_B = 1, PJ_TERM_COLOR_BRIGHT = 8};
enum {PJ_SCAN_AUTOSKIP_WS = 1, PJ_SCAN_AUTOSKIP_WS_HEADER = 3, PJ_SCAN_AUTOSKIP_NEWLINE = 4};
enum {PJSIP_PARSE_URI_AS_NAMEADDR = 1, PJSIP_PARSE_URI_IN_FROM_TO_HDR = 2};
enum {PJSIP_PARSE_REMOVE_QUOTE = 1};
enum {PJ_DNS_CLASS_IN = 1};
enum {PJSIP_UDP_TRANSPORT_KEEP_SOCKET = 1, PJSIP_UDP_TRANSPORT_DESTROY_SOCKET = 2};
enum {PJMEDIA_AUD_DEFAULT_CAPTURE_DEV = -1, PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV = -2, PJMEDIA_AUD_INVALID_DEV = -3};
enum {PJMEDIA_TONEGEN_LOOP = 1, PJMEDIA_TONEGEN_NO_LOCK = 2};
enum {PJMEDIA_VID_DEFAULT_CAPTURE_DEV = -1, PJMEDIA_VID_DEFAULT_RENDER_DEV = -2, PJMEDIA_VID_INVALID_DEV = -3};
enum {PJSIP_EVSUB_NO_EVENT_ID = 1};
%nodefaultctor pj_pool_t; %nodefaultdtor pj_pool_t;
struct pj_pool_t {};
extern void pj_pool_release(pj_pool_t *pool);
typedef int pjsua_call_id;
typedef int pjsua_acc_id;
typedef int pjsua_buddy_id;
typedef int pjsua_player_id;
typedef int pjsua_recorder_id;
typedef int pjsua_conf_port_id;
%nodefaultctor pjsua_srv_pres; %nodefaultdtor pjsua_srv_pres;
struct pjsua_srv_pres {};
typedef enum pjsip_hdr_e {PJSIP_H_ACCEPT, PJSIP_H_ACCEPT_ENCODING_UNIMP, PJSIP_H_ACCEPT_LANGUAGE_UNIMP, PJSIP_H_ALERT_INFO_UNIMP, PJSIP_H_ALLOW, PJSIP_H_AUTHENTICATION_INFO_UNIMP, PJSIP_H_AUTHORIZATION, PJSIP_H_CALL_ID, PJSIP_H_CALL_INFO_UNIMP, PJSIP_H_CONTACT, PJSIP_H_CONTENT_DISPOSITION_UNIMP, PJSIP_H_CONTENT_ENCODING_UNIMP, PJSIP_H_CONTENT_LANGUAGE_UNIMP, PJSIP_H_CONTENT_LENGTH, PJSIP_H_CONTENT_TYPE, PJSIP_H_CSEQ, PJSIP_H_DATE_UNIMP, PJSIP_H_ERROR_INFO_UNIMP, PJSIP_H_EXPIRES, PJSIP_H_FROM, PJSIP_H_IN_REPLY_TO_UNIMP, PJSIP_H_MAX_FORWARDS, PJSIP_H_MIME_VERSION_UNIMP, PJSIP_H_MIN_EXPIRES, PJSIP_H_ORGANIZATION_UNIMP, PJSIP_H_PRIORITY_UNIMP, PJSIP_H_PROXY_AUTHENTICATE, PJSIP_H_PROXY_AUTHORIZATION, PJSIP_H_PROXY_REQUIRE_UNIMP, PJSIP_H_RECORD_ROUTE, PJSIP_H_REPLY_TO_UNIMP, PJSIP_H_REQUIRE, PJSIP_H_RETRY_AFTER, PJSIP_H_ROUTE, PJSIP_H_SERVER_UNIMP, PJSIP_H_SUBJECT_UNIMP, PJSIP_H_SUPPORTED, PJSIP_H_TIMESTAMP_UNIMP, PJSIP_H_TO, PJSIP_H_UNSUPPORTED, PJSIP_H_USER_AGENT_UNIMP, PJSIP_H_VIA, PJSIP_H_WARNING_UNIMP, PJSIP_H_WWW_AUTHENTICATE, PJSIP_H_OTHER} pjsip_hdr_e;
typedef long pj_ssize_t;
struct pj_str_t
{
  char *ptr;
  pj_ssize_t slen;
};
%nodefaultctor pjsip_hdr_vptr; %nodefaultdtor pjsip_hdr_vptr;
struct pjsip_hdr_vptr {};
struct pjsip_hdr
{
  struct pjsip_hdr *prev;
  struct pjsip_hdr *next;
  pjsip_hdr_e type;
  pj_str_t name;
  pj_str_t sname;
  pjsip_hdr_vptr *vptr;
};
struct pjsip_param
{
  struct pjsip_param *prev;
  struct pjsip_param *next;
  pj_str_t name;
  pj_str_t value;
};
struct pjsip_media_type
{
  pj_str_t type;
  pj_str_t subtype;
  pjsip_param param;
};
typedef size_t pj_size_t;
struct pjsip_msg_body
{
  pjsip_media_type content_type;
  void *data;
  unsigned len;
  int (*print_body)(struct pjsip_msg_body *msg_body, char *buf, pj_size_t size);
  void *(*clone_data)(pj_pool_t *pool, const void *data, unsigned len);
};
struct pjsip_multipart_part
{
  struct pjsip_multipart_part *prev;
  struct pjsip_multipart_part *next;
  pjsip_hdr hdr;
  pjsip_msg_body *body;
};
struct pjsua_msg_data
{
  pjsip_hdr hdr_list;
  pj_str_t content_type;
  pj_str_t msg_body;
  pjsip_media_type multipart_ctype;
  pjsip_multipart_part multipart_parts;
};
typedef enum pjsua_state {PJSUA_STATE_NULL, PJSUA_STATE_CREATED, PJSUA_STATE_INIT, PJSUA_STATE_STARTING, PJSUA_STATE_RUNNING, PJSUA_STATE_CLOSING} pjsua_state;
typedef int pj_bool_t;
struct pjsua_logging_config
{
  pj_bool_t msg_logging;
  unsigned level;
  unsigned console_level;
  unsigned decor;
  pj_str_t log_filename;
  unsigned log_file_flags;
  void (*cb)(int level, const char *data, int len);
};
extern void pjsua_logging_config_default(pjsua_logging_config *cfg);
extern void pjsua_logging_config_dup(pj_pool_t *pool, pjsua_logging_config *dst, const pjsua_logging_config *src);
%nodefaultctor pjsip_evsub; %nodefaultdtor pjsip_evsub;
struct pjsip_evsub {};
%nodefaultctor pjsip_transport; %nodefaultdtor pjsip_transport;
struct pjsip_transport {};
%nodefaultctor pjsip_rx_data_op_key; %nodefaultdtor pjsip_rx_data_op_key;
struct pjsip_rx_data_op_key {};
struct pj_time_val
{
  long sec;
  long msec;
};
typedef unsigned int pj_uint32_t;
typedef unsigned short pj_uint16_t;
struct pj_addr_hdr
{
  pj_uint16_t sa_family;
};
struct pj_in_addr
{
  pj_uint32_t s_addr;
};
struct pj_sockaddr_in
{
  pj_uint16_t sin_family;
  pj_uint16_t sin_port;
  pj_in_addr sin_addr;
  char sin_zero[8];
};
typedef unsigned char pj_uint8_t;
union pj_in6_addr
{
  pj_uint8_t s6_addr[16];
  pj_uint32_t u6_addr32[4];
};
struct pj_sockaddr_in6
{
  pj_uint16_t sin6_family;
  pj_uint16_t sin6_port;
  pj_uint32_t sin6_flowinfo;
  pj_in6_addr sin6_addr;
  pj_uint32_t sin6_scope_id;
};
union pj_sockaddr
{
  pj_addr_hdr addr;
  pj_sockaddr_in ipv4;
  pj_sockaddr_in6 ipv6;
};
typedef enum pjsip_msg_type_e {PJSIP_REQUEST_MSG, PJSIP_RESPONSE_MSG} pjsip_msg_type_e;
typedef enum pjsip_method_e {PJSIP_INVITE_METHOD, PJSIP_CANCEL_METHOD, PJSIP_ACK_METHOD, PJSIP_BYE_METHOD, PJSIP_REGISTER_METHOD, PJSIP_OPTIONS_METHOD, PJSIP_OTHER_METHOD} pjsip_method_e;
struct pjsip_method
{
  pjsip_method_e id;
  pj_str_t name;
};
%nodefaultctor pjsip_uri; %nodefaultdtor pjsip_uri;
struct pjsip_uri {};
struct pjsip_request_line
{
  pjsip_method method;
  pjsip_uri *uri;
};
struct pjsip_status_line
{
  int code;
  pj_str_t reason;
};
%inline %{
union pjsip_msg_line
{
    pjsip_request_line req;
    pjsip_status_line status;
};
%}
%apply NESTED_INNER { pjsip_msg_line line };
struct pjsip_msg
{
  pjsip_msg_type_e type;
  union pjsip_msg_line line;
  pjsip_hdr hdr;
  pjsip_msg_body *body;
};
struct pjsip_cid_hdr
{
  struct pjsip_cid_hdr *prev;
  struct pjsip_cid_hdr *next;
  pjsip_hdr_e type;
  pj_str_t name;
  pj_str_t sname;
  pjsip_hdr_vptr *vptr;
  pj_str_t id;
};
struct pjsip_fromto_hdr
{
  struct pjsip_fromto_hdr *prev;
  struct pjsip_fromto_hdr *next;
  pjsip_hdr_e type;
  pj_str_t name;
  pj_str_t sname;
  pjsip_hdr_vptr *vptr;
  pjsip_uri *uri;
  pj_str_t tag;
  pjsip_param other_param;
};
typedef pjsip_fromto_hdr pjsip_from_hdr;
typedef pjsip_fromto_hdr pjsip_to_hdr;
struct pjsip_host_port
{
  pj_str_t host;
  int port;
};
struct pjsip_via_hdr
{
  struct pjsip_via_hdr *prev;
  struct pjsip_via_hdr *next;
  pjsip_hdr_e type;
  pj_str_t name;
  pj_str_t sname;
  pjsip_hdr_vptr *vptr;
  pj_str_t transport;
  pjsip_host_port sent_by;
  int ttl_param;
  int rport_param;
  pj_str_t maddr_param;
  pj_str_t recvd_param;
  pj_str_t branch_param;
  pjsip_param other_param;
  pj_str_t comment;
};
typedef int pj_int32_t;
struct pjsip_cseq_hdr
{
  struct pjsip_cseq_hdr *prev;
  struct pjsip_cseq_hdr *next;
  pjsip_hdr_e type;
  pj_str_t name;
  pj_str_t sname;
  pjsip_hdr_vptr *vptr;
  pj_int32_t cseq;
  pjsip_method method;
};
struct pjsip_generic_int_hdr
{
  struct pjsip_generic_int_hdr *prev;
  struct pjsip_generic_int_hdr *next;
  pjsip_hdr_e type;
  pj_str_t name;
  pj_str_t sname;
  pjsip_hdr_vptr *vptr;
  pj_int32_t ivalue;
};
typedef pjsip_generic_int_hdr pjsip_max_fwd_hdr;
%nodefaultctor pjsip_uri_vptr; %nodefaultdtor pjsip_uri_vptr;
struct pjsip_uri_vptr {};
struct pjsip_name_addr
{
  pjsip_uri_vptr *vptr;
  pj_str_t display;
  pjsip_uri *uri;
};
struct pjsip_routing_hdr
{
  struct pjsip_routing_hdr *prev;
  struct pjsip_routing_hdr *next;
  pjsip_hdr_e type;
  pj_str_t name;
  pj_str_t sname;
  pjsip_hdr_vptr *vptr;
  pjsip_name_addr name_addr;
  pjsip_param other_param;
};
typedef pjsip_routing_hdr pjsip_route_hdr;
typedef pjsip_routing_hdr pjsip_rr_hdr;
struct pjsip_ctype_hdr
{
  struct pjsip_ctype_hdr *prev;
  struct pjsip_ctype_hdr *next;
  pjsip_hdr_e type;
  pj_str_t name;
  pj_str_t sname;
  pjsip_hdr_vptr *vptr;
  pjsip_media_type media;
};
struct pjsip_clen_hdr
{
  struct pjsip_clen_hdr *prev;
  struct pjsip_clen_hdr *next;
  pjsip_hdr_e type;
  pj_str_t name;
  pj_str_t sname;
  pjsip_hdr_vptr *vptr;
  int len;
};
struct pjsip_generic_array_hdr
{
  struct pjsip_generic_array_hdr *prev;
  struct pjsip_generic_array_hdr *next;
  pjsip_hdr_e type;
  pj_str_t name;
  pj_str_t sname;
  pjsip_hdr_vptr *vptr;
  unsigned count;
  pj_str_t values[32];
};
typedef pjsip_generic_array_hdr pjsip_require_hdr;
typedef pjsip_generic_array_hdr pjsip_supported_hdr;
struct pjsip_parser_err_report
{
  struct pjsip_parser_err_report *prev;
  struct pjsip_parser_err_report *next;
  int except_code;
  int line;
  int col;
  pj_str_t hname;
};
%inline %{
struct pjsip_rx_data_tp_info
{
    pj_pool_t *pool;
    pjsip_transport *transport;
    void *tp_data;
    pjsip_rx_data_op_key op_key;
};
%}
%apply NESTED_INNER { pjsip_rx_data_tp_info tp_info };
%inline %{
struct pjsip_rx_data_pkt_info
{
    pj_time_val timestamp;
    char packet[4000];
    pj_uint32_t zero;
    pj_ssize_t len;
    pj_sockaddr src_addr;
    int src_addr_len;
    char src_name[46];
    int src_port;
};
%}
%apply NESTED_INNER { pjsip_rx_data_pkt_info pkt_info };
%inline %{
struct pjsip_rx_data_msg_info
{
    char *msg_buf;
    int len;
    pjsip_msg *msg;
    char *info;
    pjsip_cid_hdr *cid;
    pjsip_from_hdr *from;
    pjsip_to_hdr *to;
    pjsip_via_hdr *via;
    pjsip_cseq_hdr *cseq;
    pjsip_max_fwd_hdr *max_fwd;
    pjsip_route_hdr *route;
    pjsip_rr_hdr *record_route;
    pjsip_ctype_hdr *ctype;
    pjsip_clen_hdr *clen;
    pjsip_require_hdr *require;
    pjsip_supported_hdr *supported;
    pjsip_parser_err_report parse_err;
};
%}
%apply NESTED_INNER { pjsip_rx_data_msg_info msg_info };
%inline %{
struct pjsip_rx_data_endpt_info
{
    void *mod_data[32];
};
%}
%apply NESTED_INNER { pjsip_rx_data_endpt_info endpt_info };
struct pjsip_rx_data
{
  struct pjsip_rx_data_tp_info tp_info;
  struct pjsip_rx_data_pkt_info pkt_info;
  struct pjsip_rx_data_msg_info msg_info;
  struct pjsip_rx_data_endpt_info endpt_info;
};
struct pjsua_mwi_info
{
  pjsip_evsub *evsub;
  pjsip_rx_data *rdata;
};
%nodefaultctor pjsip_regc; %nodefaultdtor pjsip_regc;
struct pjsip_regc {};
typedef int pj_status_t;
struct pjsip_contact_hdr
{
  struct pjsip_contact_hdr *prev;
  struct pjsip_contact_hdr *next;
  pjsip_hdr_e type;
  pj_str_t name;
  pj_str_t sname;
  pjsip_hdr_vptr *vptr;
  int star;
  pjsip_uri *uri;
  int q1000;
  pj_int32_t expires;
  pjsip_param other_param;
};
struct pjsip_regc_cbparam
{
  pjsip_regc *regc;
  void *token;
  pj_status_t status;
  int code;
  pj_str_t reason;
  pjsip_rx_data *rdata;
  int expiration;
  int contact_cnt;
  pjsip_contact_hdr *contact[10];
};
struct pjsua_reg_info
{
  pjsip_regc_cbparam *cbparam;
};
typedef enum pjsua_med_tp_st {PJSUA_MED_TP_NULL, PJSUA_MED_TP_CREATING, PJSUA_MED_TP_IDLE, PJSUA_MED_TP_INIT, PJSUA_MED_TP_RUNNING, PJSUA_MED_TP_DISABLED} pjsua_med_tp_st;
struct pjsua_med_tp_state_info
{
  unsigned med_idx;
  pjsua_med_tp_st state;
  pj_status_t status;
  int sip_err_code;
  void *ext_info;
};
typedef pj_status_t (*pjsua_med_tp_state_cb)(pjsua_call_id call_id, const pjsua_med_tp_state_info *info);
typedef enum pjsua_create_media_transport_flag {PJSUA_MED_TP_CLOSE_MEMBER = 1} pjsua_create_media_transport_flag;
struct pjsua_call_setting
{
  unsigned flag;
  unsigned req_keyframe_method;
  unsigned aud_cnt;
  unsigned vid_cnt;
};
typedef enum pjsip_event_id_e {PJSIP_EVENT_UNKNOWN, PJSIP_EVENT_TIMER, PJSIP_EVENT_TX_MSG, PJSIP_EVENT_RX_MSG, PJSIP_EVENT_TRANSPORT_ERROR, PJSIP_EVENT_TSX_STATE, PJSIP_EVENT_USER} pjsip_event_id_e;
%nodefaultctor pj_timer_heap_t; %nodefaultdtor pj_timer_heap_t;
struct pj_timer_heap_t {};
typedef void pj_timer_heap_callback(pj_timer_heap_t *timer_heap, struct pj_timer_entry *entry);
typedef int pj_timer_id_t;
%nodefaultctor pj_grp_lock_t; %nodefaultdtor pj_grp_lock_t;
struct pj_grp_lock_t {};
struct pj_timer_entry
{
  void *user_data;
  int id;
  pj_timer_heap_callback *cb;
  pj_timer_id_t _timer_id;
  pj_time_val _timer_value;
  pj_grp_lock_t *_grp_lock;
};
%nodefaultctor pjsip_tpmgr; %nodefaultdtor pjsip_tpmgr;
struct pjsip_tpmgr {};
%nodefaultctor pjsip_tx_data_op_key; %nodefaultdtor pjsip_tx_data_op_key;
struct pjsip_tx_data_op_key {};
%nodefaultctor pj_lock_t; %nodefaultdtor pj_lock_t;
struct pj_lock_t {};
struct pjsip_buffer
{
  char *start;
  char *cur;
  char *end;
};
%nodefaultctor pj_atomic_t; %nodefaultdtor pj_atomic_t;
struct pj_atomic_t {};
typedef enum pjsip_transport_type_e {PJSIP_TRANSPORT_UNSPECIFIED, PJSIP_TRANSPORT_UDP, PJSIP_TRANSPORT_TCP, PJSIP_TRANSPORT_TLS, PJSIP_TRANSPORT_SCTP, PJSIP_TRANSPORT_LOOP, PJSIP_TRANSPORT_LOOP_DGRAM, PJSIP_TRANSPORT_START_OTHER, PJSIP_TRANSPORT_IPV6 = 128, PJSIP_TRANSPORT_UDP6 = PJSIP_TRANSPORT_UDP + PJSIP_TRANSPORT_IPV6, PJSIP_TRANSPORT_TCP6 = PJSIP_TRANSPORT_TCP + PJSIP_TRANSPORT_IPV6, PJSIP_TRANSPORT_TLS6 = PJSIP_TRANSPORT_TLS + PJSIP_TRANSPORT_IPV6} pjsip_transport_type_e;
%inline %{
struct pjsip_server_addresses_entry
{
    pjsip_transport_type_e type;
    unsigned priority;
    unsigned weight;
    pj_sockaddr addr;
    int addr_len;
};
%}
%apply NESTED_INNER { pjsip_server_addresses_entry entry };
struct pjsip_server_addresses
{
  unsigned count;
  struct pjsip_server_addresses_entry entry[8];
};
typedef enum pjsip_tpselector_type {PJSIP_TPSELECTOR_NONE, PJSIP_TPSELECTOR_TRANSPORT, PJSIP_TPSELECTOR_LISTENER} pjsip_tpselector_type;
%nodefaultctor pjsip_tpfactory; %nodefaultdtor pjsip_tpfactory;
struct pjsip_tpfactory {};
%inline %{
union pjsip_tpselector_u
{
    pjsip_transport *transport;
    pjsip_tpfactory *listener;
    void *ptr;
};
%}
%apply NESTED_INNER { pjsip_tpselector_u u };
struct pjsip_tpselector
{
  pjsip_tpselector_type type;
  union pjsip_tpselector_u u;
};
%inline %{
struct pjsip_tx_data_dest_info
{
    pj_str_t name;
    pjsip_server_addresses addr;
    unsigned cur_addr;
};
%}
%apply NESTED_INNER { pjsip_tx_data_dest_info dest_info };
%inline %{
struct pjsip_tx_data_tp_info
{
    pjsip_transport *transport;
    pj_sockaddr dst_addr;
    int dst_addr_len;
    char dst_name[46];
    int dst_port;
};
%}
%apply NESTED_INNER { pjsip_tx_data_tp_info tp_info };
struct pjsip_tx_data
{
  struct pjsip_tx_data *prev;
  struct pjsip_tx_data *next;
  pj_pool_t *pool;
  char obj_name[32];
  char *info;
  pj_time_val rx_timestamp;
  pjsip_tpmgr *mgr;
  pjsip_tx_data_op_key op_key;
  pj_lock_t *lock;
  pjsip_msg *msg;
  pjsip_route_hdr *saved_strict_route;
  pjsip_buffer buf;
  pj_atomic_t *ref_cnt;
  int is_pending;
  void *token;
  void (*cb)(void *, pjsip_tx_data *, pj_ssize_t);
  struct pjsip_tx_data_dest_info dest_info;
  struct pjsip_tx_data_tp_info tp_info;
  pjsip_tpselector tp_sel;
  pj_bool_t auth_retry;
  void *mod_data[32];
  pjsip_host_port via_addr;
  const void *via_tp;
};
%nodefaultctor pjsip_module; %nodefaultdtor pjsip_module;
struct pjsip_module {};
%nodefaultctor pjsip_endpoint; %nodefaultdtor pjsip_endpoint;
struct pjsip_endpoint {};
%nodefaultctor pj_mutex_t; %nodefaultdtor pj_mutex_t;
struct pj_mutex_t {};
typedef enum pjsip_role_e {PJSIP_ROLE_UAC, PJSIP_ROLE_UAS, PJSIP_UAC_ROLE = PJSIP_ROLE_UAC, PJSIP_UAS_ROLE = PJSIP_ROLE_UAS} pjsip_role_e;
typedef enum pjsip_tsx_state_e {PJSIP_TSX_STATE_NULL, PJSIP_TSX_STATE_CALLING, PJSIP_TSX_STATE_TRYING, PJSIP_TSX_STATE_PROCEEDING, PJSIP_TSX_STATE_COMPLETED, PJSIP_TSX_STATE_CONFIRMED, PJSIP_TSX_STATE_TERMINATED, PJSIP_TSX_STATE_DESTROYED, PJSIP_TSX_STATE_MAX} pjsip_tsx_state_e;
struct pjsip_host_info
{
  unsigned flag;
  pjsip_transport_type_e type;
  pjsip_host_port addr;
};
struct pjsip_response_addr
{
  pjsip_transport *transport;
  pj_sockaddr addr;
  int addr_len;
  pjsip_host_info dst_host;
};
typedef void pjsip_tp_state_listener_key;
struct pjsip_transaction
{
  pj_pool_t *pool;
  pjsip_module *tsx_user;
  pjsip_endpoint *endpt;
  pj_bool_t terminating;
  pj_grp_lock_t *grp_lock;
  pj_mutex_t *mutex_b;
  char obj_name[32];
  pjsip_role_e role;
  pjsip_method method;
  pj_int32_t cseq;
  pj_str_t transaction_key;
  pj_uint32_t hashed_key;
  pj_str_t branch;
  int status_code;
  pj_str_t status_text;
  pjsip_tsx_state_e state;
  int handle_200resp;
  int tracing;
  pj_status_t (*state_handler)(struct pjsip_transaction *, pjsip_event *);
  pjsip_transport *transport;
  pj_bool_t is_reliable;
  pj_sockaddr addr;
  int addr_len;
  pjsip_response_addr res_addr;
  unsigned transport_flag;
  pj_status_t transport_err;
  pjsip_tpselector tp_sel;
  pjsip_tx_data *pending_tx;
  pjsip_tp_state_listener_key *tp_st_key;
  pjsip_tx_data *last_tx;
  int retransmit_count;
  pj_timer_entry retransmit_timer;
  pj_timer_entry timeout_timer;
  void *mod_data[32];
};
%inline %{
struct pjsip_event_body_timer
{
      pj_timer_entry *entry;
};
%}
%apply NESTED_INNER { pjsip_event_body_timer timer };
%inline %{
union pjsip_event_body_tsx_state_src
{
        pjsip_rx_data *rdata;
        pjsip_tx_data *tdata;
        pj_timer_entry *timer;
        pj_status_t status;
        void *data;
};
%}
%apply NESTED_INNER { pjsip_event_body_tsx_state_src src };
%inline %{
struct pjsip_event_body_tsx_state
{
      union pjsip_event_body_tsx_state_src src;
      pjsip_transaction *tsx;
      int prev_state;
      pjsip_event_id_e type;
};
%}
%apply NESTED_INNER { pjsip_event_body_tsx_state tsx_state };
%inline %{
struct pjsip_event_body_tx_msg
{
      pjsip_tx_data *tdata;
};
%}
%apply NESTED_INNER { pjsip_event_body_tx_msg tx_msg };
%inline %{
struct pjsip_event_body_tx_error
{
      pjsip_tx_data *tdata;
      pjsip_transaction *tsx;
};
%}
%apply NESTED_INNER { pjsip_event_body_tx_error tx_error };
%inline %{
struct pjsip_event_body_rx_msg
{
      pjsip_rx_data *rdata;
};
%}
%apply NESTED_INNER { pjsip_event_body_rx_msg rx_msg };
%inline %{
struct pjsip_event_body_user
{
      void *user1;
      void *user2;
      void *user3;
      void *user4;
};
%}
%apply NESTED_INNER { pjsip_event_body_user user };
%inline %{
union pjsip_event_body
{
    struct pjsip_event_body_timer timer;
    struct pjsip_event_body_tsx_state tsx_state;
    struct pjsip_event_body_tx_msg tx_msg;
    struct pjsip_event_body_tx_error tx_error;
    struct pjsip_event_body_rx_msg rx_msg;
    struct pjsip_event_body_user user;
};
%}
%apply NESTED_INNER { pjsip_event_body body };
struct pjsip_event
{
  struct pjsip_event *prev;
  struct pjsip_event *next;
  pjsip_event_id_e type;
  union pjsip_event_body body;
};
struct pjmedia_sdp_conn
{
  pj_str_t net_type;
  pj_str_t addr_type;
  pj_str_t addr;
};
struct pjmedia_sdp_bandw
{
  pj_str_t modifier;
  pj_uint32_t value;
};
struct pjmedia_sdp_attr
{
  pj_str_t name;
  pj_str_t value;
};
%inline %{
struct pjmedia_sdp_media_desc
{
    pj_str_t media;
    pj_uint16_t port;
    unsigned port_count;
    pj_str_t transport;
    unsigned fmt_count;
    pj_str_t fmt[32];
};
%}
%apply NESTED_INNER { pjmedia_sdp_media_desc desc };
struct pjmedia_sdp_media
{
  struct pjmedia_sdp_media_desc desc;
  pjmedia_sdp_conn *conn;
  unsigned bandw_count;
  pjmedia_sdp_bandw *bandw[4];
  unsigned attr_count;
  pjmedia_sdp_attr *attr[(32 * 2) + 4];
};
%inline %{
struct pjmedia_sdp_session_origin
{
    pj_str_t user;
    pj_uint32_t id;
    pj_uint32_t version;
    pj_str_t net_type;
    pj_str_t addr_type;
    pj_str_t addr;
};
%}
%apply NESTED_INNER { pjmedia_sdp_session_origin origin };
%inline %{
struct pjmedia_sdp_session_time
{
    pj_uint32_t start;
    pj_uint32_t stop;
};
%}
%apply NESTED_INNER { pjmedia_sdp_session_time time };
struct pjmedia_sdp_session
{
  struct pjmedia_sdp_session_origin origin;
  pj_str_t name;
  pjmedia_sdp_conn *conn;
  unsigned bandw_count;
  pjmedia_sdp_bandw *bandw[4];
  struct pjmedia_sdp_session_time time;
  unsigned attr_count;
  pjmedia_sdp_attr *attr[(32 * 2) + 4];
  unsigned media_count;
  pjmedia_sdp_media *media[16];
};
%nodefaultctor pjmedia_stream; %nodefaultdtor pjmedia_stream;
struct pjmedia_stream {};
%nodefaultctor pjmedia_port; %nodefaultdtor pjmedia_port;
struct pjmedia_port {};
typedef enum pjsip_status_code {PJSIP_SC_TRYING = 100, PJSIP_SC_RINGING = 180, PJSIP_SC_CALL_BEING_FORWARDED = 181, PJSIP_SC_QUEUED = 182, PJSIP_SC_PROGRESS = 183, PJSIP_SC_OK = 200, PJSIP_SC_ACCEPTED = 202, PJSIP_SC_MULTIPLE_CHOICES = 300, PJSIP_SC_MOVED_PERMANENTLY = 301, PJSIP_SC_MOVED_TEMPORARILY = 302, PJSIP_SC_USE_PROXY = 305, PJSIP_SC_ALTERNATIVE_SERVICE = 380, PJSIP_SC_BAD_REQUEST = 400, PJSIP_SC_UNAUTHORIZED = 401, PJSIP_SC_PAYMENT_REQUIRED = 402, PJSIP_SC_FORBIDDEN = 403, PJSIP_SC_NOT_FOUND = 404, PJSIP_SC_METHOD_NOT_ALLOWED = 405, PJSIP_SC_NOT_ACCEPTABLE = 406, PJSIP_SC_PROXY_AUTHENTICATION_REQUIRED = 407, PJSIP_SC_REQUEST_TIMEOUT = 408, PJSIP_SC_GONE = 410, PJSIP_SC_REQUEST_ENTITY_TOO_LARGE = 413, PJSIP_SC_REQUEST_URI_TOO_LONG = 414, PJSIP_SC_UNSUPPORTED_MEDIA_TYPE = 415, PJSIP_SC_UNSUPPORTED_URI_SCHEME = 416, PJSIP_SC_BAD_EXTENSION = 420, PJSIP_SC_EXTENSION_REQUIRED = 421, PJSIP_SC_SESSION_TIMER_TOO_SMALL = 422, PJSIP_SC_INTERVAL_TOO_BRIEF = 423, PJSIP_SC_TEMPORARILY_UNAVAILABLE = 480, PJSIP_SC_CALL_TSX_DOES_NOT_EXIST = 481, PJSIP_SC_LOOP_DETECTED = 482, PJSIP_SC_TOO_MANY_HOPS = 483, PJSIP_SC_ADDRESS_INCOMPLETE = 484, PJSIP_AC_AMBIGUOUS = 485, PJSIP_SC_BUSY_HERE = 486, PJSIP_SC_REQUEST_TERMINATED = 487, PJSIP_SC_NOT_ACCEPTABLE_HERE = 488, PJSIP_SC_BAD_EVENT = 489, PJSIP_SC_REQUEST_UPDATED = 490, PJSIP_SC_REQUEST_PENDING = 491, PJSIP_SC_UNDECIPHERABLE = 493, PJSIP_SC_INTERNAL_SERVER_ERROR = 500, PJSIP_SC_NOT_IMPLEMENTED = 501, PJSIP_SC_BAD_GATEWAY = 502, PJSIP_SC_SERVICE_UNAVAILABLE = 503, PJSIP_SC_SERVER_TIMEOUT = 504, PJSIP_SC_VERSION_NOT_SUPPORTED = 505, PJSIP_SC_MESSAGE_TOO_LARGE = 513, PJSIP_SC_PRECONDITION_FAILURE = 580, PJSIP_SC_BUSY_EVERYWHERE = 600, PJSIP_SC_DECLINE = 603, PJSIP_SC_DOES_NOT_EXIST_ANYWHERE = 604, PJSIP_SC_NOT_ACCEPTABLE_ANYWHERE = 606, PJSIP_SC_TSX_TIMEOUT = PJSIP_SC_REQUEST_TIMEOUT, PJSIP_SC_TSX_TRANSPORT_ERROR = PJSIP_SC_SERVICE_UNAVAILABLE, PJSIP_SC__force_32bit = 0x7FFFFFFF} pjsip_status_code;
enum pjsip_evsub_state {PJSIP_EVSUB_STATE_NULL, PJSIP_EVSUB_STATE_SENT, PJSIP_EVSUB_STATE_ACCEPTED, PJSIP_EVSUB_STATE_PENDING, PJSIP_EVSUB_STATE_ACTIVE, PJSIP_EVSUB_STATE_TERMINATED, PJSIP_EVSUB_STATE_UNKNOWN};
typedef enum pj_stun_nat_type {PJ_STUN_NAT_TYPE_UNKNOWN, PJ_STUN_NAT_TYPE_ERR_UNKNOWN, PJ_STUN_NAT_TYPE_OPEN, PJ_STUN_NAT_TYPE_BLOCKED, PJ_STUN_NAT_TYPE_SYMMETRIC_UDP, PJ_STUN_NAT_TYPE_FULL_CONE, PJ_STUN_NAT_TYPE_SYMMETRIC, PJ_STUN_NAT_TYPE_RESTRICTED, PJ_STUN_NAT_TYPE_PORT_RESTRICTED} pj_stun_nat_type;
struct pj_stun_nat_detect_result
{
  pj_status_t status;
  const char *status_text;
  pj_stun_nat_type nat_type;
  const char *nat_type_name;
};
typedef enum pjsip_redirect_op {PJSIP_REDIRECT_REJECT, PJSIP_REDIRECT_ACCEPT, PJSIP_REDIRECT_ACCEPT_REPLACE, PJSIP_REDIRECT_PENDING, PJSIP_REDIRECT_STOP} pjsip_redirect_op;
typedef enum pjsip_transport_state {PJSIP_TP_STATE_CONNECTED, PJSIP_TP_STATE_DISCONNECTED} pjsip_transport_state;
struct pjsip_transport_state_info
{
  pj_status_t status;
  void *ext_info;
  void *user_data;
};
typedef void (*pjsip_tp_state_callback)(pjsip_transport *tp, pjsip_transport_state state, const pjsip_transport_state_info *info);
typedef enum pj_ice_strans_op {PJ_ICE_STRANS_OP_INIT, PJ_ICE_STRANS_OP_NEGOTIATION, PJ_ICE_STRANS_OP_KEEP_ALIVE} pj_ice_strans_op;
typedef enum pjmedia_event_type {PJMEDIA_EVENT_NONE, PJMEDIA_EVENT_FMT_CHANGED = ((('H' << 24) | ('C' << 16)) | ('M' << 8)) | 'F', PJMEDIA_EVENT_WND_CLOSING = ((('L' << 24) | ('C' << 16)) | ('N' << 8)) | 'W', PJMEDIA_EVENT_WND_CLOSED = ((('O' << 24) | ('C' << 16)) | ('N' << 8)) | 'W', PJMEDIA_EVENT_WND_RESIZED = ((('Z' << 24) | ('R' << 16)) | ('N' << 8)) | 'W', PJMEDIA_EVENT_MOUSE_BTN_DOWN = ((('N' << 24) | ('D' << 16)) | ('S' << 8)) | 'M', PJMEDIA_EVENT_KEYFRAME_FOUND = ((('F' << 24) | ('R' << 16)) | ('F' << 8)) | 'I', PJMEDIA_EVENT_KEYFRAME_MISSING = ((('M' << 24) | ('R' << 16)) | ('F' << 8)) | 'I', PJMEDIA_EVENT_ORIENT_CHANGED = ((('T' << 24) | ('N' << 16)) | ('R' << 8)) | 'O'} pjmedia_event_type;
typedef unsigned long long pj_uint64_t;
%inline %{
struct pj_timestamp_u32
{
    pj_uint32_t lo;
    pj_uint32_t hi;
};
%}
%apply NESTED_INNER { pj_timestamp_u32 u32 };
union pj_timestamp
{
  struct pj_timestamp_u32 u32;
  pj_uint64_t u64;
};
typedef enum pjmedia_dir {PJMEDIA_DIR_NONE = 0, PJMEDIA_DIR_ENCODING = 1, PJMEDIA_DIR_CAPTURE = PJMEDIA_DIR_ENCODING, PJMEDIA_DIR_DECODING = 2, PJMEDIA_DIR_PLAYBACK = PJMEDIA_DIR_DECODING, PJMEDIA_DIR_RENDER = PJMEDIA_DIR_DECODING, PJMEDIA_DIR_ENCODING_DECODING = 3, PJMEDIA_DIR_CAPTURE_PLAYBACK = PJMEDIA_DIR_ENCODING_DECODING, PJMEDIA_DIR_CAPTURE_RENDER = PJMEDIA_DIR_ENCODING_DECODING} pjmedia_dir;
typedef enum pjmedia_type {PJMEDIA_TYPE_NONE, PJMEDIA_TYPE_AUDIO, PJMEDIA_TYPE_VIDEO, PJMEDIA_TYPE_APPLICATION, PJMEDIA_TYPE_UNKNOWN} pjmedia_type;
typedef enum pjmedia_format_detail_type {PJMEDIA_FORMAT_DETAIL_NONE, PJMEDIA_FORMAT_DETAIL_AUDIO, PJMEDIA_FORMAT_DETAIL_VIDEO, PJMEDIA_FORMAT_DETAIL_MAX} pjmedia_format_detail_type;
struct pjmedia_audio_format_detail
{
  unsigned clock_rate;
  unsigned channel_count;
  unsigned frame_time_usec;
  unsigned bits_per_sample;
  pj_uint32_t avg_bps;
  pj_uint32_t max_bps;
};
struct pjmedia_rect_size
{
  unsigned w;
  unsigned h;
};
struct pjmedia_ratio
{
  int num;
  int denum;
};
struct pjmedia_video_format_detail
{
  pjmedia_rect_size size;
  pjmedia_ratio fps;
  pj_uint32_t avg_bps;
  pj_uint32_t max_bps;
};
%inline %{
union pjmedia_format_det
{
    pjmedia_audio_format_detail aud;
    pjmedia_video_format_detail vid;
    char user[1];
};
%}
%apply NESTED_INNER { pjmedia_format_det det };
struct pjmedia_format
{
  pj_uint32_t id;
  pjmedia_type type;
  pjmedia_format_detail_type detail_type;
  union pjmedia_format_det det;
};
struct pjmedia_event_fmt_changed_data
{
  pjmedia_dir dir;
  pjmedia_format new_fmt;
};
struct pjmedia_event_wnd_resized_data
{
  pjmedia_rect_size new_size;
};
struct pjmedia_event_wnd_closing_data
{
  pj_bool_t cancel;
};
struct pjmedia_event_dummy_data
{
  int dummy;
};
typedef pjmedia_event_dummy_data pjmedia_event_wnd_closed_data;
typedef pjmedia_event_dummy_data pjmedia_event_mouse_btn_down_data;
typedef pjmedia_event_dummy_data pjmedia_event_keyframe_found_data;
typedef pjmedia_event_dummy_data pjmedia_event_keyframe_missing_data;
typedef char pjmedia_event_user_data[sizeof(pjmedia_event_fmt_changed_data)];
%inline %{
union pjmedia_event_data
{
    pjmedia_event_fmt_changed_data fmt_changed;
    pjmedia_event_wnd_resized_data wnd_resized;
    pjmedia_event_wnd_closing_data wnd_closing;
    pjmedia_event_wnd_closed_data wnd_closed;
    pjmedia_event_mouse_btn_down_data mouse_btn_down;
    pjmedia_event_keyframe_found_data keyframe_found;
    pjmedia_event_keyframe_missing_data keyframe_missing;
    pjmedia_event_user_data user;
    void *ptr;
};
%}
%apply NESTED_INNER { pjmedia_event_data data };
struct pjmedia_event
{
  pjmedia_event_type type;
  pj_timestamp timestamp;
  const void *src;
  const void *epub;
  union pjmedia_event_data data;
};
%nodefaultctor pjmedia_transport; %nodefaultdtor pjmedia_transport;
struct pjmedia_transport {};
typedef enum pjsua_sip_timer_use {PJSUA_SIP_TIMER_INACTIVE, PJSUA_SIP_TIMER_OPTIONAL, PJSUA_SIP_TIMER_REQUIRED, PJSUA_SIP_TIMER_ALWAYS} pjsua_sip_timer_use;
typedef enum pjsua_100rel_use {PJSUA_100REL_NOT_USED, PJSUA_100REL_MANDATORY, PJSUA_100REL_OPTIONAL} pjsua_100rel_use;
struct pjsip_timer_setting
{
  unsigned min_se;
  unsigned sess_expires;
};
struct pjsip_digest_challenge
{
  pj_str_t realm;
  pjsip_param other_param;
  pj_str_t domain;
  pj_str_t nonce;
  pj_str_t opaque;
  int stale;
  pj_str_t algorithm;
  pj_str_t qop;
};
struct pjsip_digest_credential
{
  pj_str_t realm;
  pjsip_param other_param;
  pj_str_t username;
  pj_str_t nonce;
  pj_str_t uri;
  pj_str_t response;
  pj_str_t algorithm;
  pj_str_t cnonce;
  pj_str_t opaque;
  pj_str_t qop;
  pj_str_t nc;
};
typedef pj_status_t (*pjsip_cred_cb)(pj_pool_t *pool, const pjsip_digest_challenge *chal, const pjsip_cred_info *cred, const pj_str_t *method, pjsip_digest_credential *auth);
%inline %{
struct pjsip_cred_info_ext_aka
{
      pj_str_t k;
      pj_str_t op;
      pj_str_t amf;
      pjsip_cred_cb cb;
};
%}
%apply NESTED_INNER { pjsip_cred_info_ext_aka aka };
%inline %{
union pjsip_cred_info_ext
{
    struct pjsip_cred_info_ext_aka aka;
};
%}
%apply NESTED_INNER { pjsip_cred_info_ext ext };
struct pjsip_cred_info
{
  pj_str_t realm;
  pj_str_t scheme;
  pj_str_t username;
  int data_type;
  pj_str_t data;
  union pjsip_cred_info_ext ext;
};
typedef enum pjmedia_srtp_use {PJMEDIA_SRTP_DISABLED, PJMEDIA_SRTP_OPTIONAL, PJMEDIA_SRTP_MANDATORY} pjmedia_srtp_use;
struct pjsua_config
{
  unsigned max_calls;
  unsigned thread_cnt;
  unsigned nameserver_count;
  pj_str_t nameserver[4];
  pj_bool_t force_lr;
  unsigned outbound_proxy_cnt;
  pj_str_t outbound_proxy[4];
  pj_str_t stun_domain;
  pj_str_t stun_host;
  unsigned stun_srv_cnt;
  pj_str_t stun_srv[8];
  pj_bool_t stun_ignore_failure;
  pj_bool_t stun_map_use_stun2;
  int nat_type_in_sdp;
  pjsua_100rel_use require_100rel;
  pjsua_sip_timer_use use_timer;
  pj_bool_t enable_unsolicited_mwi;
  pjsip_timer_setting timer_setting;
  unsigned cred_count;
  pjsip_cred_info cred_info[8];
  pjsua_callback cb;
  pj_str_t user_agent;
  pjmedia_srtp_use use_srtp;
  int srtp_secure_signaling;
  pj_bool_t srtp_optional_dup_offer;
  pj_bool_t hangup_forked_call;
};
typedef enum pjsua_destroy_flag {PJSUA_DESTROY_NO_RX_MSG = 1, PJSUA_DESTROY_NO_TX_MSG = 2, PJSUA_DESTROY_NO_NETWORK = PJSUA_DESTROY_NO_RX_MSG | PJSUA_DESTROY_NO_TX_MSG} pjsua_destroy_flag;
extern void pjsua_config_default(pjsua_config *cfg);
extern void pjsua_config_dup(pj_pool_t *pool, pjsua_config *dst, const pjsua_config *src);
extern void pjsua_msg_data_init(pjsua_msg_data *msg_data);
extern pjsua_msg_data *pjsua_msg_data_clone(pj_pool_t *pool, const pjsua_msg_data *rhs);
extern pj_status_t pjsua_create(void);
struct pj_ice_sess_options
{
  pj_bool_t aggressive;
  unsigned nominated_check_delay;
  int controlled_agent_want_nom_timeout;
};
typedef enum pj_turn_tp_type {PJ_TURN_TP_UDP = 17, PJ_TURN_TP_TCP = 6, PJ_TURN_TP_TLS = 255} pj_turn_tp_type;
typedef enum pj_stun_auth_cred_type {PJ_STUN_AUTH_CRED_STATIC, PJ_STUN_AUTH_CRED_DYNAMIC} pj_stun_auth_cred_type;
typedef enum pj_stun_passwd_type {PJ_STUN_PASSWD_PLAIN = 0, PJ_STUN_PASSWD_HASHED = 1} pj_stun_passwd_type;
%nodefaultctor pj_stun_msg; %nodefaultdtor pj_stun_msg;
struct pj_stun_msg {};
%inline %{
struct pj_stun_auth_cred_data_static_cred
{
      pj_str_t realm;
      pj_str_t username;
      pj_stun_passwd_type data_type;
      pj_str_t data;
      pj_str_t nonce;
};
%}
%apply NESTED_INNER { pj_stun_auth_cred_data_static_cred static_cred };
%inline %{
struct pj_stun_auth_cred_data_dyn_cred
{
      void *user_data;
      pj_status_t (*get_auth)(void *user_data, pj_pool_t *pool, pj_str_t *realm, pj_str_t *nonce);
      pj_status_t (*get_cred)(const pj_stun_msg *msg, void *user_data, pj_pool_t *pool, pj_str_t *realm, pj_str_t *username, pj_str_t *nonce, pj_stun_passwd_type *data_type, pj_str_t *data);
      pj_status_t (*get_password)(const pj_stun_msg *msg, void *user_data, const pj_str_t *realm, const pj_str_t *username, pj_pool_t *pool, pj_stun_passwd_type *data_type, pj_str_t *data);
      pj_bool_t (*verify_nonce)(const pj_stun_msg *msg, void *user_data, const pj_str_t *realm, const pj_str_t *username, const pj_str_t *nonce);
};
%}
%apply NESTED_INNER { pj_stun_auth_cred_data_dyn_cred dyn_cred };
%inline %{
union pj_stun_auth_cred_data
{
    struct pj_stun_auth_cred_data_static_cred static_cred;
    struct pj_stun_auth_cred_data_dyn_cred dyn_cred;
};
%}
%apply NESTED_INNER { pj_stun_auth_cred_data data };
struct pj_stun_auth_cred
{
  pj_stun_auth_cred_type type;
  union pj_stun_auth_cred_data data;
};
struct pjsua_media_config
{
  unsigned clock_rate;
  unsigned snd_clock_rate;
  unsigned channel_count;
  unsigned audio_frame_ptime;
  unsigned max_media_ports;
  pj_bool_t has_ioqueue;
  unsigned thread_cnt;
  unsigned quality;
  unsigned ptime;
  pj_bool_t no_vad;
  unsigned ilbc_mode;
  unsigned tx_drop_pct;
  unsigned rx_drop_pct;
  unsigned ec_options;
  unsigned ec_tail_len;
  unsigned snd_rec_latency;
  unsigned snd_play_latency;
  int jb_init;
  int jb_min_pre;
  int jb_max_pre;
  int jb_max;
  pj_bool_t enable_ice;
  int ice_max_host_cands;
  pj_ice_sess_options ice_opt;
  pj_bool_t ice_no_rtcp;
  pj_bool_t ice_always_update;
  pj_bool_t enable_turn;
  pj_str_t turn_server;
  pj_turn_tp_type turn_conn_type;
  pj_stun_auth_cred turn_auth_cred;
  int snd_auto_close_time;
  pj_bool_t vid_preview_enable_native;
  pj_bool_t no_smart_media_update;
  pj_bool_t no_rtcp_sdes_bye;
};
extern pj_status_t pjsua_init(const pjsua_config *ua_cfg, const pjsua_logging_config *log_cfg, const pjsua_media_config *media_cfg);
extern pj_status_t pjsua_start(void);
extern pj_status_t pjsua_destroy(void);
extern pjsua_state pjsua_get_state(void);
extern pj_status_t pjsua_destroy2(unsigned flags);
extern int pjsua_handle_events(unsigned msec_timeout);
extern pj_pool_t *pjsua_pool_create(const char *name, pj_size_t init_size, pj_size_t increment);
extern pj_status_t pjsua_reconfigure_logging(const pjsua_logging_config *c);
extern pjsip_endpoint *pjsua_get_pjsip_endpt(void);
%nodefaultctor pjmedia_endpt; %nodefaultdtor pjmedia_endpt;
struct pjmedia_endpt {};
extern pjmedia_endpt *pjsua_get_pjmedia_endpt(void);
%nodefaultctor pj_pool_factory; %nodefaultdtor pj_pool_factory;
struct pj_pool_factory {};
extern pj_pool_factory *pjsua_get_pool_factory(void);
extern pj_status_t pjsua_detect_nat_type(void);
extern pj_status_t pjsua_get_nat_type(pj_stun_nat_type *type);
struct pj_stun_resolve_result
{
  void *token;
  pj_status_t status;
  pj_str_t name;
  pj_sockaddr addr;
};
typedef void (*pj_stun_resolve_cb)(const pj_stun_resolve_result *result);
extern pj_status_t pjsua_resolve_stun_servers(unsigned count, pj_str_t srv[], pj_bool_t wait, void *token, pj_stun_resolve_cb cb);
extern pj_status_t pjsua_cancel_stun_resolution(void *token, pj_bool_t notify_cb);
extern pj_status_t pjsua_verify_sip_url(const char *url);
extern pj_status_t pjsua_verify_url(const char *url);
extern pj_status_t pjsua_schedule_timer(pj_timer_entry *entry, const pj_time_val *delay);
extern pj_status_t pjsua_schedule_timer2(void (*cb)(void *user_data), void *user_data, unsigned msec_delay);
extern void pjsua_cancel_timer(pj_timer_entry *entry);
extern void pjsua_perror(const char *sender, const char *title, pj_status_t status);
extern void pjsua_dump(pj_bool_t detail);
typedef int pjsua_transport_id;
typedef enum pj_ssl_cipher {PJ_TLS_NULL_WITH_NULL_NULL = 0x00000000, PJ_TLS_RSA_WITH_NULL_MD5 = 0x00000001, PJ_TLS_RSA_WITH_NULL_SHA = 0x00000002, PJ_TLS_RSA_WITH_NULL_SHA256 = 0x0000003B, PJ_TLS_RSA_WITH_RC4_128_MD5 = 0x00000004, PJ_TLS_RSA_WITH_RC4_128_SHA = 0x00000005, PJ_TLS_RSA_WITH_3DES_EDE_CBC_SHA = 0x0000000A, PJ_TLS_RSA_WITH_AES_128_CBC_SHA = 0x0000002F, PJ_TLS_RSA_WITH_AES_256_CBC_SHA = 0x00000035, PJ_TLS_RSA_WITH_AES_128_CBC_SHA256 = 0x0000003C, PJ_TLS_RSA_WITH_AES_256_CBC_SHA256 = 0x0000003D, PJ_TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA = 0x0000000D, PJ_TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA = 0x00000010, PJ_TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA = 0x00000013, PJ_TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA = 0x00000016, PJ_TLS_DH_DSS_WITH_AES_128_CBC_SHA = 0x00000030, PJ_TLS_DH_RSA_WITH_AES_128_CBC_SHA = 0x00000031, PJ_TLS_DHE_DSS_WITH_AES_128_CBC_SHA = 0x00000032, PJ_TLS_DHE_RSA_WITH_AES_128_CBC_SHA = 0x00000033, PJ_TLS_DH_DSS_WITH_AES_256_CBC_SHA = 0x00000036, PJ_TLS_DH_RSA_WITH_AES_256_CBC_SHA = 0x00000037, PJ_TLS_DHE_DSS_WITH_AES_256_CBC_SHA = 0x00000038, PJ_TLS_DHE_RSA_WITH_AES_256_CBC_SHA = 0x00000039, PJ_TLS_DH_DSS_WITH_AES_128_CBC_SHA256 = 0x0000003E, PJ_TLS_DH_RSA_WITH_AES_128_CBC_SHA256 = 0x0000003F, PJ_TLS_DHE_DSS_WITH_AES_128_CBC_SHA256 = 0x00000040, PJ_TLS_DHE_RSA_WITH_AES_128_CBC_SHA256 = 0x00000067, PJ_TLS_DH_DSS_WITH_AES_256_CBC_SHA256 = 0x00000068, PJ_TLS_DH_RSA_WITH_AES_256_CBC_SHA256 = 0x00000069, PJ_TLS_DHE_DSS_WITH_AES_256_CBC_SHA256 = 0x0000006A, PJ_TLS_DHE_RSA_WITH_AES_256_CBC_SHA256 = 0x0000006B, PJ_TLS_DH_anon_WITH_RC4_128_MD5 = 0x00000018, PJ_TLS_DH_anon_WITH_3DES_EDE_CBC_SHA = 0x0000001B, PJ_TLS_DH_anon_WITH_AES_128_CBC_SHA = 0x00000034, PJ_TLS_DH_anon_WITH_AES_256_CBC_SHA = 0x0000003A, PJ_TLS_DH_anon_WITH_AES_128_CBC_SHA256 = 0x0000006C, PJ_TLS_DH_anon_WITH_AES_256_CBC_SHA256 = 0x0000006D, PJ_TLS_RSA_EXPORT_WITH_RC4_40_MD5 = 0x00000003, PJ_TLS_RSA_EXPORT_WITH_RC2_CBC_40_MD5 = 0x00000006, PJ_TLS_RSA_WITH_IDEA_CBC_SHA = 0x00000007, PJ_TLS_RSA_EXPORT_WITH_DES40_CBC_SHA = 0x00000008, PJ_TLS_RSA_WITH_DES_CBC_SHA = 0x00000009, PJ_TLS_DH_DSS_EXPORT_WITH_DES40_CBC_SHA = 0x0000000B, PJ_TLS_DH_DSS_WITH_DES_CBC_SHA = 0x0000000C, PJ_TLS_DH_RSA_EXPORT_WITH_DES40_CBC_SHA = 0x0000000E, PJ_TLS_DH_RSA_WITH_DES_CBC_SHA = 0x0000000F, PJ_TLS_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA = 0x00000011, PJ_TLS_DHE_DSS_WITH_DES_CBC_SHA = 0x00000012, PJ_TLS_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA = 0x00000014, PJ_TLS_DHE_RSA_WITH_DES_CBC_SHA = 0x00000015, PJ_TLS_DH_anon_EXPORT_WITH_RC4_40_MD5 = 0x00000017, PJ_TLS_DH_anon_EXPORT_WITH_DES40_CBC_SHA = 0x00000019, PJ_TLS_DH_anon_WITH_DES_CBC_SHA = 0x0000001A, PJ_SSL_FORTEZZA_KEA_WITH_NULL_SHA = 0x0000001C, PJ_SSL_FORTEZZA_KEA_WITH_FORTEZZA_CBC_SHA = 0x0000001D, PJ_SSL_FORTEZZA_KEA_WITH_RC4_128_SHA = 0x0000001E, PJ_SSL_CK_RC4_128_WITH_MD5 = 0x00010080, PJ_SSL_CK_RC4_128_EXPORT40_WITH_MD5 = 0x00020080, PJ_SSL_CK_RC2_128_CBC_WITH_MD5 = 0x00030080, PJ_SSL_CK_RC2_128_CBC_EXPORT40_WITH_MD5 = 0x00040080, PJ_SSL_CK_IDEA_128_CBC_WITH_MD5 = 0x00050080, PJ_SSL_CK_DES_64_CBC_WITH_MD5 = 0x00060040, PJ_SSL_CK_DES_192_EDE3_CBC_WITH_MD5 = 0x000700C0} pj_ssl_cipher;
typedef enum pj_qos_type {PJ_QOS_TYPE_BEST_EFFORT, PJ_QOS_TYPE_BACKGROUND, PJ_QOS_TYPE_VIDEO, PJ_QOS_TYPE_VOICE, PJ_QOS_TYPE_CONTROL} pj_qos_type;
typedef enum pj_qos_wmm_prio {PJ_QOS_WMM_PRIO_BULK_EFFORT, PJ_QOS_WMM_PRIO_BULK, PJ_QOS_WMM_PRIO_VIDEO, PJ_QOS_WMM_PRIO_VOICE} pj_qos_wmm_prio;
struct pj_qos_params
{
  pj_uint8_t flags;
  pj_uint8_t dscp_val;
  pj_uint8_t so_prio;
  pj_qos_wmm_prio wmm_prio;
};
struct pjsip_tls_setting
{
  pj_str_t ca_list_file;
  pj_str_t cert_file;
  pj_str_t privkey_file;
  pj_str_t password;
  int method;
  unsigned ciphers_num;
  pj_ssl_cipher *ciphers;
  pj_bool_t verify_server;
  pj_bool_t verify_client;
  pj_bool_t require_client_cert;
  pj_time_val timeout;
  pj_bool_t reuse_addr;
  pj_qos_type qos_type;
  pj_qos_params qos_params;
  pj_bool_t qos_ignore_error;
};
struct pjsua_transport_config
{
  unsigned port;
  unsigned port_range;
  pj_str_t public_addr;
  pj_str_t bound_addr;
  pjsip_tls_setting tls_setting;
  pj_qos_type qos_type;
  pj_qos_params qos_params;
};
extern void pjsua_transport_config_default(pjsua_transport_config *cfg);
extern void pjsua_transport_config_dup(pj_pool_t *pool, pjsua_transport_config *dst, const pjsua_transport_config *src);
struct pjsua_transport_info
{
  pjsua_transport_id id;
  pjsip_transport_type_e type;
  pj_str_t type_name;
  pj_str_t info;
  unsigned flag;
  unsigned addr_len;
  pj_sockaddr local_addr;
  pjsip_host_port local_name;
  unsigned usage_count;
};
extern pj_status_t pjsua_transport_create(pjsip_transport_type_e type, const pjsua_transport_config *cfg, pjsua_transport_id *p_id);
extern pj_status_t pjsua_transport_register(pjsip_transport *tp, pjsua_transport_id *p_id);
extern pj_status_t pjsua_enum_transports(pjsua_transport_id id[], unsigned *count);
extern pj_status_t pjsua_transport_get_info(pjsua_transport_id id, pjsua_transport_info *info);
extern pj_status_t pjsua_transport_set_enable(pjsua_transport_id id, pj_bool_t enabled);
extern pj_status_t pjsua_transport_close(pjsua_transport_id id, pj_bool_t force);
typedef enum pjsua_call_hold_type {PJSUA_CALL_HOLD_TYPE_RFC3264, PJSUA_CALL_HOLD_TYPE_RFC2543} pjsua_call_hold_type;
typedef enum pjsua_stun_use {PJSUA_STUN_USE_DEFAULT, PJSUA_STUN_USE_DISABLED} pjsua_stun_use;
typedef enum pjsua_ice_config_use {PJSUA_ICE_CONFIG_USE_DEFAULT, PJSUA_ICE_CONFIG_USE_CUSTOM} pjsua_ice_config_use;
typedef enum pjsua_turn_config_use {PJSUA_TURN_CONFIG_USE_DEFAULT, PJSUA_TURN_CONFIG_USE_CUSTOM} pjsua_turn_config_use;
struct pjsua_ice_config
{
  pj_bool_t enable_ice;
  int ice_max_host_cands;
  pj_ice_sess_options ice_opt;
  pj_bool_t ice_no_rtcp;
  pj_bool_t ice_always_update;
};
struct pjsua_turn_config
{
  pj_bool_t enable_turn;
  pj_str_t turn_server;
  pj_turn_tp_type turn_conn_type;
  pj_stun_auth_cred turn_auth_cred;
};
typedef enum pjsua_ipv6_use {PJSUA_IPV6_DISABLED, PJSUA_IPV6_ENABLED} pjsua_ipv6_use;
struct pjsip_publishc_opt
{
  pj_bool_t queue_request;
};
struct pjsip_auth_clt_pref
{
  pj_bool_t initial_auth;
  pj_str_t algorithm;
};
typedef pj_int32_t pjmedia_vid_dev_index;
typedef enum pjmedia_vid_stream_rc_method {PJMEDIA_VID_STREAM_RC_NONE = 0, PJMEDIA_VID_STREAM_RC_SIMPLE_BLOCKING = 1} pjmedia_vid_stream_rc_method;
struct pjmedia_vid_stream_rc_config
{
  pjmedia_vid_stream_rc_method method;
  unsigned bandwidth;
};
struct pjsua_acc_config
{
  void *user_data;
  int priority;
  pj_str_t id;
  pj_str_t reg_uri;
  pjsip_hdr reg_hdr_list;
  pjsip_hdr sub_hdr_list;
  pj_bool_t mwi_enabled;
  unsigned mwi_expires;
  pj_bool_t publish_enabled;
  pjsip_publishc_opt publish_opt;
  unsigned unpublish_max_wait_time_msec;
  pjsip_auth_clt_pref auth_pref;
  pj_str_t pidf_tuple_id;
  pj_str_t force_contact;
  pj_str_t contact_params;
  pj_str_t contact_uri_params;
  pjsua_100rel_use require_100rel;
  pjsua_sip_timer_use use_timer;
  pjsip_timer_setting timer_setting;
  unsigned proxy_cnt;
  pj_str_t proxy[8];
  unsigned lock_codec;
  unsigned reg_timeout;
  unsigned reg_delay_before_refresh;
  unsigned unreg_timeout;
  unsigned cred_count;
  pjsip_cred_info cred_info[8];
  pjsua_transport_id transport_id;
  pj_bool_t allow_contact_rewrite;
  int contact_rewrite_method;
  pj_bool_t allow_via_rewrite;
  unsigned use_rfc5626;
  pj_str_t rfc5626_instance_id;
  pj_str_t rfc5626_reg_id;
  unsigned ka_interval;
  pj_str_t ka_data;
  pj_bool_t vid_in_auto_show;
  pj_bool_t vid_out_auto_transmit;
  unsigned vid_wnd_flags;
  pjmedia_vid_dev_index vid_cap_dev;
  pjmedia_vid_dev_index vid_rend_dev;
  pjmedia_vid_stream_rc_config vid_stream_rc_cfg;
  pjsua_transport_config rtp_cfg;
  pjsua_ipv6_use ipv6_media_use;
  pjsua_stun_use sip_stun_use;
  pjsua_stun_use media_stun_use;
  pjsua_ice_config_use ice_cfg_use;
  pjsua_ice_config ice_cfg;
  pjsua_turn_config_use turn_cfg_use;
  pjsua_turn_config turn_cfg;
  pjmedia_srtp_use use_srtp;
  int srtp_secure_signaling;
  pj_bool_t srtp_optional_dup_offer;
  unsigned reg_retry_interval;
  unsigned reg_first_retry_interval;
  pj_bool_t drop_calls_on_reg_fail;
  unsigned reg_use_proxy;
  pjsua_call_hold_type call_hold_type;
  pj_bool_t register_on_acc_add;
};
extern void pjsua_ice_config_from_media_config(pj_pool_t *pool, pjsua_ice_config *dst, const pjsua_media_config *src);
extern void pjsua_ice_config_dup(pj_pool_t *pool, pjsua_ice_config *dst, const pjsua_ice_config *src);
extern void pjsua_turn_config_from_media_config(pj_pool_t *pool, pjsua_turn_config *dst, const pjsua_media_config *src);
extern void pjsua_turn_config_dup(pj_pool_t *pool, pjsua_turn_config *dst, const pjsua_turn_config *src);
extern void pjsua_acc_config_default(pjsua_acc_config *cfg);
extern void pjsua_acc_config_dup(pj_pool_t *pool, pjsua_acc_config *dst, const pjsua_acc_config *src);
typedef enum pjrpid_element_type {PJRPID_ELEMENT_TYPE_PERSON} pjrpid_element_type;
typedef enum pjrpid_activity {PJRPID_ACTIVITY_UNKNOWN, PJRPID_ACTIVITY_AWAY, PJRPID_ACTIVITY_BUSY} pjrpid_activity;
struct pjrpid_element
{
  pjrpid_element_type type;
  pj_str_t id;
  pjrpid_activity activity;
  pj_str_t note;
};
struct pjsua_acc_info
{
  pjsua_acc_id id;
  pj_bool_t is_default;
  pj_str_t acc_uri;
  pj_bool_t has_registration;
  int expires;
  pjsip_status_code status;
  pj_status_t reg_last_err;
  pj_str_t status_text;
  pj_bool_t online_status;
  pj_str_t online_status_text;
  pjrpid_element rpid;
  char buf_[80];
};
extern unsigned pjsua_acc_get_count(void);
extern pj_bool_t pjsua_acc_is_valid(pjsua_acc_id acc_id);
extern pj_status_t pjsua_acc_set_default(pjsua_acc_id acc_id);
extern pjsua_acc_id pjsua_acc_get_default(void);
extern pj_status_t pjsua_acc_add(const pjsua_acc_config *acc_cfg, pj_bool_t is_default, pjsua_acc_id *p_acc_id);
extern pj_status_t pjsua_acc_add_local(pjsua_transport_id tid, pj_bool_t is_default, pjsua_acc_id *p_acc_id);
extern pj_status_t pjsua_acc_set_user_data(pjsua_acc_id acc_id, void *user_data);
extern void *pjsua_acc_get_user_data(pjsua_acc_id acc_id);
extern pj_status_t pjsua_acc_del(pjsua_acc_id acc_id);
extern pj_status_t pjsua_acc_get_config(pjsua_acc_id acc_id, pjsua_acc_config *acc_cfg);
extern pj_status_t pjsua_acc_modify(pjsua_acc_id acc_id, const pjsua_acc_config *acc_cfg);
extern pj_status_t pjsua_acc_set_online_status(pjsua_acc_id acc_id, pj_bool_t is_online);
extern pj_status_t pjsua_acc_set_online_status2(pjsua_acc_id acc_id, pj_bool_t is_online, const pjrpid_element *pr);
extern pj_status_t pjsua_acc_set_registration(pjsua_acc_id acc_id, pj_bool_t renew);
extern pj_status_t pjsua_acc_get_info(pjsua_acc_id acc_id, pjsua_acc_info *info);
extern pj_status_t pjsua_enum_accs(pjsua_acc_id ids[], unsigned *count);
extern pj_status_t pjsua_acc_enum_info(pjsua_acc_info info[], unsigned *count);
extern pjsua_acc_id pjsua_acc_find_for_outgoing(const pj_str_t *url);
extern pjsua_acc_id pjsua_acc_find_for_incoming(pjsip_rx_data *rdata);
extern pj_status_t pjsua_acc_create_request(pjsua_acc_id acc_id, const pjsip_method *method, const pj_str_t *target, pjsip_tx_data **p_tdata);
extern pj_status_t pjsua_acc_create_uac_contact(pj_pool_t *pool, pj_str_t *p_contact, pjsua_acc_id acc_id, const pj_str_t *uri);
extern pj_status_t pjsua_acc_create_uas_contact(pj_pool_t *pool, pj_str_t *p_contact, pjsua_acc_id acc_id, pjsip_rx_data *rdata);
extern pj_status_t pjsua_acc_set_transport(pjsua_acc_id acc_id, pjsua_transport_id tp_id);
typedef enum pjsua_call_media_status {PJSUA_CALL_MEDIA_NONE, PJSUA_CALL_MEDIA_ACTIVE, PJSUA_CALL_MEDIA_LOCAL_HOLD, PJSUA_CALL_MEDIA_REMOTE_HOLD, PJSUA_CALL_MEDIA_ERROR} pjsua_call_media_status;
typedef int pjsua_vid_win_id;
%inline %{
struct pjsua_call_media_info_stream_aud
{
      pjsua_conf_port_id conf_slot;
};
%}
%apply NESTED_INNER { pjsua_call_media_info_stream_aud aud };
%inline %{
struct pjsua_call_media_info_stream_vid
{
      pjsua_vid_win_id win_in;
      pjmedia_vid_dev_index cap_dev;
};
%}
%apply NESTED_INNER { pjsua_call_media_info_stream_vid vid };
%inline %{
union pjsua_call_media_info_stream
{
    struct pjsua_call_media_info_stream_aud aud;
    struct pjsua_call_media_info_stream_vid vid;
};
%}
%apply NESTED_INNER { pjsua_call_media_info_stream stream };
struct pjsua_call_media_info
{
  unsigned index;
  pjmedia_type type;
  pjmedia_dir dir;
  pjsua_call_media_status status;
  union pjsua_call_media_info_stream stream;
};
typedef enum pjsip_inv_state {PJSIP_INV_STATE_NULL, PJSIP_INV_STATE_CALLING, PJSIP_INV_STATE_INCOMING, PJSIP_INV_STATE_EARLY, PJSIP_INV_STATE_CONNECTING, PJSIP_INV_STATE_CONFIRMED, PJSIP_INV_STATE_DISCONNECTED} pjsip_inv_state;
%inline %{
struct pjsua_call_info_buf_
{
    char local_info[128];
    char local_contact[128];
    char remote_info[128];
    char remote_contact[128];
    char call_id[128];
    char last_status_text[128];
};
%}
%apply NESTED_INNER { pjsua_call_info_buf_ buf_ };
struct pjsua_call_info
{
  pjsua_call_id id;
  pjsip_role_e role;
  pjsua_acc_id acc_id;
  pj_str_t local_info;
  pj_str_t local_contact;
  pj_str_t remote_info;
  pj_str_t remote_contact;
  pj_str_t call_id;
  pjsua_call_setting setting;
  pjsip_inv_state state;
  pj_str_t state_text;
  pjsip_status_code last_status;
  pj_str_t last_status_text;
  pjsua_call_media_status media_status;
  pjmedia_dir media_dir;
  pjsua_conf_port_id conf_slot;
  unsigned media_cnt;
  pjsua_call_media_info media[16];
  unsigned prov_media_cnt;
  pjsua_call_media_info prov_media[16];
  pj_time_val connect_duration;
  pj_time_val total_duration;
  pj_bool_t rem_offerer;
  unsigned rem_aud_cnt;
  unsigned rem_vid_cnt;
  struct pjsua_call_info_buf_ buf_;
};
typedef enum pjsua_call_flag {PJSUA_CALL_UNHOLD = 1, PJSUA_CALL_UPDATE_CONTACT = 2, PJSUA_CALL_INCLUDE_DISABLED_MEDIA = 4} pjsua_call_flag;
typedef enum pjmedia_tp_proto {PJMEDIA_TP_PROTO_NONE = 0, PJMEDIA_TP_PROTO_RTP_AVP, PJMEDIA_TP_PROTO_RTP_SAVP, PJMEDIA_TP_PROTO_UNKNOWN} pjmedia_tp_proto;
struct pjmedia_codec_info
{
  pjmedia_type type;
  unsigned pt;
  pj_str_t encoding_name;
  unsigned clock_rate;
  unsigned channel_cnt;
};
typedef enum pjmedia_format_id {PJMEDIA_FORMAT_L16 = 0, PJMEDIA_FORMAT_PCM = PJMEDIA_FORMAT_L16, PJMEDIA_FORMAT_PCMA = ((('W' << 24) | ('A' << 16)) | ('L' << 8)) | 'A', PJMEDIA_FORMAT_ALAW = PJMEDIA_FORMAT_PCMA, PJMEDIA_FORMAT_PCMU = ((('W' << 24) | ('A' << 16)) | ('L' << 8)) | 'u', PJMEDIA_FORMAT_ULAW = PJMEDIA_FORMAT_PCMU, PJMEDIA_FORMAT_AMR = ((('R' << 24) | ('M' << 16)) | ('A' << 8)) | ' ', PJMEDIA_FORMAT_G729 = ((('9' << 24) | ('2' << 16)) | ('7' << 8)) | 'G', PJMEDIA_FORMAT_ILBC = ((('C' << 24) | ('B' << 16)) | ('L' << 8)) | 'I', PJMEDIA_FORMAT_RGB24 = ((('3' << 24) | ('B' << 16)) | ('G' << 8)) | 'R', PJMEDIA_FORMAT_RGBA = ((('A' << 24) | ('B' << 16)) | ('G' << 8)) | 'R', PJMEDIA_FORMAT_BGRA = ((('A' << 24) | ('R' << 16)) | ('G' << 8)) | 'B', PJMEDIA_FORMAT_RGB32 = PJMEDIA_FORMAT_RGBA, PJMEDIA_FORMAT_DIB = (((' ' << 24) | ('B' << 16)) | ('I' << 8)) | 'D', PJMEDIA_FORMAT_GBRP = ((('P' << 24) | ('R' << 16)) | ('B' << 8)) | 'G', PJMEDIA_FORMAT_AYUV = ((('V' << 24) | ('U' << 16)) | ('Y' << 8)) | 'A', PJMEDIA_FORMAT_YUY2 = ((('2' << 24) | ('Y' << 16)) | ('U' << 8)) | 'Y', PJMEDIA_FORMAT_UYVY = ((('Y' << 24) | ('V' << 16)) | ('Y' << 8)) | 'U', PJMEDIA_FORMAT_YVYU = ((('U' << 24) | ('Y' << 16)) | ('V' << 8)) | 'Y', PJMEDIA_FORMAT_I420 = ((('0' << 24) | ('2' << 16)) | ('4' << 8)) | 'I', PJMEDIA_FORMAT_IYUV = PJMEDIA_FORMAT_I420, PJMEDIA_FORMAT_YV12 = ((('2' << 24) | ('1' << 16)) | ('V' << 8)) | 'Y', PJMEDIA_FORMAT_I422 = ((('2' << 24) | ('2' << 16)) | ('4' << 8)) | 'I', PJMEDIA_FORMAT_I420JPEG = ((('0' << 24) | ('2' << 16)) | ('4' << 8)) | 'J', PJMEDIA_FORMAT_I422JPEG = ((('2' << 24) | ('2' << 16)) | ('4' << 8)) | 'J', PJMEDIA_FORMAT_H261 = ((('1' << 24) | ('6' << 16)) | ('2' << 8)) | 'H', PJMEDIA_FORMAT_H263 = ((('3' << 24) | ('6' << 16)) | ('2' << 8)) | 'H', PJMEDIA_FORMAT_H263P = ((('3' << 24) | ('6' << 16)) | ('2' << 8)) | 'P', PJMEDIA_FORMAT_H264 = ((('4' << 24) | ('6' << 16)) | ('2' << 8)) | 'H', PJMEDIA_FORMAT_MJPEG = ((('G' << 24) | ('P' << 16)) | ('J' << 8)) | 'M', PJMEDIA_FORMAT_MPEG1VIDEO = ((('V' << 24) | ('1' << 16)) | ('P' << 8)) | 'M', PJMEDIA_FORMAT_MPEG2VIDEO = ((('V' << 24) | ('2' << 16)) | ('P' << 8)) | 'M', PJMEDIA_FORMAT_MPEG4 = ((('4' << 24) | ('G' << 16)) | ('P' << 8)) | 'M'} pjmedia_format_id;
%inline %{
struct pjmedia_codec_fmtp_param
{
    pj_str_t name;
    pj_str_t val;
};
%}
%apply NESTED_INNER { pjmedia_codec_fmtp_param param };
struct pjmedia_codec_fmtp
{
  pj_uint8_t cnt;
  struct pjmedia_codec_fmtp_param param[16];
};
%inline %{
struct pjmedia_codec_param_info
{
    unsigned clock_rate;
    unsigned channel_cnt;
    pj_uint32_t avg_bps;
    pj_uint32_t max_bps;
    unsigned max_rx_frame_size;
    pj_uint16_t frm_ptime;
    pj_uint16_t enc_ptime;
    pj_uint8_t pcm_bits_per_sample;
    pj_uint8_t pt;
    pjmedia_format_id fmt_id;
};
%}
%apply NESTED_INNER { pjmedia_codec_param_info info };
%inline %{
struct pjmedia_codec_param_setting
{
    pj_uint8_t frm_per_pkt;
    unsigned vad : 1;
    unsigned cng : 1;
    unsigned penh : 1;
    unsigned plc : 1;
    unsigned reserved : 1;
    pjmedia_codec_fmtp enc_fmtp;
    pjmedia_codec_fmtp dec_fmtp;
};
%}
%apply NESTED_INNER { pjmedia_codec_param_setting setting };
struct pjmedia_codec_param
{
  struct pjmedia_codec_param_info info;
  struct pjmedia_codec_param_setting setting;
};
struct pjmedia_stream_info
{
  pjmedia_type type;
  pjmedia_tp_proto proto;
  pjmedia_dir dir;
  pj_sockaddr rem_addr;
  pj_sockaddr rem_rtcp;
  pjmedia_codec_info fmt;
  pjmedia_codec_param *param;
  unsigned tx_pt;
  unsigned rx_pt;
  unsigned tx_maxptime;
  int tx_event_pt;
  int rx_event_pt;
  pj_uint32_t ssrc;
  pj_uint32_t rtp_ts;
  pj_uint16_t rtp_seq;
  pj_uint8_t rtp_seq_ts_set;
  int jb_init;
  int jb_min_pre;
  int jb_max_pre;
  int jb_max;
  pj_bool_t rtcp_sdes_bye_disabled;
};
struct pjmedia_vid_codec_info
{
  pjmedia_format_id fmt_id;
  unsigned pt;
  pj_str_t encoding_name;
  pj_str_t encoding_desc;
  unsigned clock_rate;
  pjmedia_dir dir;
  unsigned dec_fmt_id_cnt;
  pjmedia_format_id dec_fmt_id[8];
  unsigned packings;
  unsigned fps_cnt;
  pjmedia_ratio fps[16];
};
typedef enum pjmedia_vid_packing {PJMEDIA_VID_PACKING_UNKNOWN, PJMEDIA_VID_PACKING_PACKETS = 1, PJMEDIA_VID_PACKING_WHOLE = 2} pjmedia_vid_packing;
struct pjmedia_vid_codec_param
{
  pjmedia_dir dir;
  pjmedia_vid_packing packing;
  pjmedia_format enc_fmt;
  pjmedia_codec_fmtp enc_fmtp;
  unsigned enc_mtu;
  pjmedia_format dec_fmt;
  pjmedia_codec_fmtp dec_fmtp;
  pj_bool_t ignore_fmtp;
};
struct pjmedia_vid_stream_info
{
  pjmedia_type type;
  pjmedia_tp_proto proto;
  pjmedia_dir dir;
  pj_sockaddr rem_addr;
  pj_sockaddr rem_rtcp;
  unsigned tx_pt;
  unsigned rx_pt;
  pj_uint32_t ssrc;
  pj_uint32_t rtp_ts;
  pj_uint16_t rtp_seq;
  pj_uint8_t rtp_seq_ts_set;
  int jb_init;
  int jb_min_pre;
  int jb_max_pre;
  int jb_max;
  pjmedia_vid_codec_info codec_info;
  pjmedia_vid_codec_param *codec_param;
  pj_bool_t rtcp_sdes_bye_disabled;
  pjmedia_vid_stream_rc_config rc_cfg;
};
%inline %{
union pjsua_stream_info_info
{
    pjmedia_stream_info aud;
    pjmedia_vid_stream_info vid;
};
%}
%apply NESTED_INNER { pjsua_stream_info_info info };
struct pjsua_stream_info
{
  pjmedia_type type;
  union pjsua_stream_info_info info;
};
typedef long long pj_int64_t;
typedef pj_int64_t pj_highprec_t;
struct pj_math_stat
{
  int n;
  int max;
  int min;
  int last;
  int mean;
  int mean_res_;
  pj_highprec_t m2_;
};
%inline %{
struct pjmedia_rtcp_stream_stat_loss_type
{
    unsigned burst : 1;
    unsigned random : 1;
};
%}
%apply NESTED_INNER { pjmedia_rtcp_stream_stat_loss_type loss_type };
struct pjmedia_rtcp_stream_stat
{
  pj_time_val update;
  unsigned update_cnt;
  pj_uint32_t pkt;
  pj_uint32_t bytes;
  unsigned discard;
  unsigned loss;
  unsigned reorder;
  unsigned dup;
  pj_math_stat loss_period;
  struct pjmedia_rtcp_stream_stat_loss_type loss_type;
  pj_math_stat jitter;
};
struct pjmedia_rtcp_sdes
{
  pj_str_t cname;
  pj_str_t name;
  pj_str_t email;
  pj_str_t phone;
  pj_str_t loc;
  pj_str_t tool;
  pj_str_t note;
};
struct pjmedia_rtcp_stat
{
  pj_time_val start;
  pjmedia_rtcp_stream_stat tx;
  pjmedia_rtcp_stream_stat rx;
  pj_math_stat rtt;
  pj_uint32_t rtp_tx_last_ts;
  pj_uint16_t rtp_tx_last_seq;
  pjmedia_rtcp_sdes peer_sdes;
  char peer_sdes_buf_[64];
};
struct pjmedia_jb_state
{
  unsigned frame_size;
  unsigned min_prefetch;
  unsigned max_prefetch;
  unsigned burst;
  unsigned prefetch;
  unsigned size;
  unsigned avg_delay;
  unsigned min_delay;
  unsigned max_delay;
  unsigned dev_delay;
  unsigned avg_burst;
  unsigned lost;
  unsigned discard;
  unsigned empty;
};
struct pjsua_stream_stat
{
  pjmedia_rtcp_stat rtcp;
  pjmedia_jb_state jbuf;
};
extern void pjsua_call_setting_default(pjsua_call_setting *opt);
extern unsigned pjsua_call_get_max_count(void);
extern unsigned pjsua_call_get_count(void);
extern pj_status_t pjsua_enum_calls(pjsua_call_id ids[], unsigned *count);
extern pj_status_t pjsua_call_make_call(pjsua_acc_id acc_id, const pj_str_t *dst_uri, const pjsua_call_setting *opt, void *user_data, const pjsua_msg_data *msg_data, pjsua_call_id *p_call_id);
extern pj_bool_t pjsua_call_is_active(pjsua_call_id call_id);
extern pj_bool_t pjsua_call_has_media(pjsua_call_id call_id);
extern pjsua_conf_port_id pjsua_call_get_conf_port(pjsua_call_id call_id);
extern pj_status_t pjsua_call_get_info(pjsua_call_id call_id, pjsua_call_info *info);
typedef enum pjsip_dialog_cap_status {PJSIP_DIALOG_CAP_UNSUPPORTED = 0, PJSIP_DIALOG_CAP_SUPPORTED = 1, PJSIP_DIALOG_CAP_UNKNOWN = 2} pjsip_dialog_cap_status;
extern pjsip_dialog_cap_status pjsua_call_remote_has_cap(pjsua_call_id call_id, int htype, const pj_str_t *hname, const pj_str_t *token);
extern pj_status_t pjsua_call_set_user_data(pjsua_call_id call_id, void *user_data);
extern void *pjsua_call_get_user_data(pjsua_call_id call_id);
extern pj_status_t pjsua_call_get_rem_nat_type(pjsua_call_id call_id, pj_stun_nat_type *p_type);
extern pj_status_t pjsua_call_answer(pjsua_call_id call_id, unsigned code, const pj_str_t *reason, const pjsua_msg_data *msg_data);
extern pj_status_t pjsua_call_answer2(pjsua_call_id call_id, const pjsua_call_setting *opt, unsigned code, const pj_str_t *reason, const pjsua_msg_data *msg_data);
extern pj_status_t pjsua_call_hangup(pjsua_call_id call_id, unsigned code, const pj_str_t *reason, const pjsua_msg_data *msg_data);
extern pj_status_t pjsua_call_process_redirect(pjsua_call_id call_id, pjsip_redirect_op cmd);
extern pj_status_t pjsua_call_set_hold(pjsua_call_id call_id, const pjsua_msg_data *msg_data);
extern pj_status_t pjsua_call_set_hold2(pjsua_call_id call_id, unsigned options, const pjsua_msg_data *msg_data);
extern pj_status_t pjsua_call_reinvite(pjsua_call_id call_id, unsigned options, const pjsua_msg_data *msg_data);
extern pj_status_t pjsua_call_reinvite2(pjsua_call_id call_id, const pjsua_call_setting *opt, const pjsua_msg_data *msg_data);
extern pj_status_t pjsua_call_update(pjsua_call_id call_id, unsigned options, const pjsua_msg_data *msg_data);
extern pj_status_t pjsua_call_update2(pjsua_call_id call_id, const pjsua_call_setting *opt, const pjsua_msg_data *msg_data);
extern pj_status_t pjsua_call_xfer(pjsua_call_id call_id, const pj_str_t *dest, const pjsua_msg_data *msg_data);
extern pj_status_t pjsua_call_xfer_replaces(pjsua_call_id call_id, pjsua_call_id dest_call_id, unsigned options, const pjsua_msg_data *msg_data);
extern pj_status_t pjsua_call_dial_dtmf(pjsua_call_id call_id, const pj_str_t *digits);
extern pj_status_t pjsua_call_send_im(pjsua_call_id call_id, const pj_str_t *mime_type, const pj_str_t *content, const pjsua_msg_data *msg_data, void *user_data);
extern pj_status_t pjsua_call_send_typing_ind(pjsua_call_id call_id, pj_bool_t is_typing, const pjsua_msg_data *msg_data);
extern pj_status_t pjsua_call_send_request(pjsua_call_id call_id, const pj_str_t *method, const pjsua_msg_data *msg_data);
extern void pjsua_call_hangup_all(void);
extern pj_status_t pjsua_call_dump(pjsua_call_id call_id, pj_bool_t with_media, char *buffer, unsigned maxlen, const char *indent);
extern pj_status_t pjsua_call_get_stream_info(pjsua_call_id call_id, unsigned med_idx, pjsua_stream_info *psi);
extern pj_status_t pjsua_call_get_stream_stat(pjsua_call_id call_id, unsigned med_idx, pjsua_stream_stat *stat);
typedef long pj_sock_t;
struct pjmedia_sock_info
{
  pj_sock_t rtp_sock;
  pj_sockaddr rtp_addr_name;
  pj_sock_t rtcp_sock;
  pj_sockaddr rtcp_addr_name;
};
typedef enum pjmedia_transport_type {PJMEDIA_TRANSPORT_TYPE_UDP, PJMEDIA_TRANSPORT_TYPE_ICE, PJMEDIA_TRANSPORT_TYPE_SRTP, PJMEDIA_TRANSPORT_TYPE_USER} pjmedia_transport_type;
struct pjmedia_transport_specific_info
{
  pjmedia_transport_type type;
  int cbsize;
  char buffer[36 * (sizeof(long))];
};
struct pjmedia_transport_info
{
  pjmedia_sock_info sock_info;
  pj_sockaddr src_rtp_name;
  pj_sockaddr src_rtcp_name;
  unsigned specific_info_cnt;
  pjmedia_transport_specific_info spc_info[4];
};
extern pj_status_t pjsua_call_get_med_transport_info(pjsua_call_id call_id, unsigned med_idx, pjmedia_transport_info *t);
struct pjsua_buddy_config
{
  pj_str_t uri;
  pj_bool_t subscribe;
  void *user_data;
};
typedef enum pjsua_buddy_status {PJSUA_BUDDY_STATUS_UNKNOWN, PJSUA_BUDDY_STATUS_ONLINE, PJSUA_BUDDY_STATUS_OFFLINE} pjsua_buddy_status;
struct pj_xml_attr
{
  pj_xml_attr *prev;
  pj_xml_attr *next;
  pj_str_t name;
  pj_str_t value;
};
struct pj_xml_node_head
{
  pj_xml_node *prev;
  pj_xml_node *next;
};
struct pj_xml_node
{
  pj_xml_node *prev;
  pj_xml_node *next;
  pj_str_t name;
  pj_xml_attr attr_head;
  pj_xml_node_head node_head;
  pj_str_t content;
};
%inline %{
struct pjsip_pres_status_info
{
    pj_bool_t basic_open;
    pjrpid_element rpid;
    pj_str_t id;
    pj_str_t contact;
    pj_xml_node *tuple_node;
};
%}
%apply NESTED_INNER { pjsip_pres_status_info info };
struct pjsip_pres_status
{
  unsigned info_cnt;
  struct pjsip_pres_status_info info[8];
  pj_bool_t _is_valid;
};
struct pjsua_buddy_info
{
  pjsua_buddy_id id;
  pj_str_t uri;
  pj_str_t contact;
  pjsua_buddy_status status;
  pj_str_t status_text;
  pj_bool_t monitor_pres;
  pjsip_evsub_state sub_state;
  const char *sub_state_name;
  unsigned sub_term_code;
  pj_str_t sub_term_reason;
  pjrpid_element rpid;
  pjsip_pres_status pres_status;
  char buf_[512];
};
extern void pjsua_buddy_config_default(pjsua_buddy_config *cfg);
extern unsigned pjsua_get_buddy_count(void);
extern pj_bool_t pjsua_buddy_is_valid(pjsua_buddy_id buddy_id);
extern pj_status_t pjsua_enum_buddies(pjsua_buddy_id ids[], unsigned *count);
extern pjsua_buddy_id pjsua_buddy_find(const pj_str_t *uri);
extern pj_status_t pjsua_buddy_get_info(pjsua_buddy_id buddy_id, pjsua_buddy_info *info);
extern pj_status_t pjsua_buddy_set_user_data(pjsua_buddy_id buddy_id, void *user_data);
extern void *pjsua_buddy_get_user_data(pjsua_buddy_id buddy_id);
extern pj_status_t pjsua_buddy_add(const pjsua_buddy_config *buddy_cfg, pjsua_buddy_id *p_buddy_id);
extern pj_status_t pjsua_buddy_del(pjsua_buddy_id buddy_id);
extern pj_status_t pjsua_buddy_subscribe_pres(pjsua_buddy_id buddy_id, pj_bool_t subscribe);
extern pj_status_t pjsua_buddy_update_pres(pjsua_buddy_id buddy_id);
extern pj_status_t pjsua_pres_notify(pjsua_acc_id acc_id, pjsua_srv_pres *srv_pres, pjsip_evsub_state state, const pj_str_t *state_str, const pj_str_t *reason, pj_bool_t with_body, const pjsua_msg_data *msg_data);
extern void pjsua_pres_dump(pj_bool_t verbose);
extern pj_status_t pjsua_im_send(pjsua_acc_id acc_id, const pj_str_t *to, const pj_str_t *mime_type, const pj_str_t *content, const pjsua_msg_data *msg_data, void *user_data);
extern pj_status_t pjsua_im_typing(pjsua_acc_id acc_id, const pj_str_t *to, pj_bool_t is_typing, const pjsua_msg_data *msg_data);
extern void pjsua_media_config_default(pjsua_media_config *cfg);
struct pjsua_codec_info
{
  pj_str_t codec_id;
  pj_uint8_t priority;
  pj_str_t desc;
  char buf_[64];
};
struct pjsua_conf_port_info
{
  pjsua_conf_port_id slot_id;
  pj_str_t name;
  unsigned clock_rate;
  unsigned channel_count;
  unsigned samples_per_frame;
  unsigned bits_per_sample;
  unsigned listener_cnt;
  pjsua_conf_port_id listeners[4 + (2 * 4)];
};
%nodefaultctor pjsua_media_transport; %nodefaultdtor pjsua_media_transport;
struct pjsua_media_transport {};
extern unsigned pjsua_conf_get_max_ports(void);
extern unsigned pjsua_conf_get_active_ports(void);
extern pj_status_t pjsua_enum_conf_ports(pjsua_conf_port_id id[], unsigned *count);
extern pj_status_t pjsua_conf_get_port_info(pjsua_conf_port_id port_id, pjsua_conf_port_info *info);
extern pj_status_t pjsua_conf_add_port(pj_pool_t *pool, pjmedia_port *port, pjsua_conf_port_id *p_id);
extern pj_status_t pjsua_conf_remove_port(pjsua_conf_port_id port_id);
extern pj_status_t pjsua_conf_connect(pjsua_conf_port_id source, pjsua_conf_port_id sink);
extern pj_status_t pjsua_conf_disconnect(pjsua_conf_port_id source, pjsua_conf_port_id sink);
extern pj_status_t pjsua_conf_adjust_tx_level(pjsua_conf_port_id slot, float level);
extern pj_status_t pjsua_conf_adjust_rx_level(pjsua_conf_port_id slot, float level);
extern pj_status_t pjsua_conf_get_signal_level(pjsua_conf_port_id slot, unsigned *tx_level, unsigned *rx_level);
extern pj_status_t pjsua_player_create(const pj_str_t *filename, unsigned options, pjsua_player_id *p_id);
extern pj_status_t pjsua_playlist_create(const pj_str_t file_names[], unsigned file_count, const pj_str_t *label, unsigned options, pjsua_player_id *p_id);
extern pjsua_conf_port_id pjsua_player_get_conf_port(pjsua_player_id id);
extern pj_status_t pjsua_player_get_port(pjsua_player_id id, pjmedia_port **p_port);
extern pj_status_t pjsua_player_set_pos(pjsua_player_id id, pj_uint32_t samples);
extern pj_status_t pjsua_player_destroy(pjsua_player_id id);
extern pj_status_t pjsua_recorder_create(const pj_str_t *filename, unsigned enc_type, void *enc_param, pj_ssize_t max_size, unsigned options, pjsua_recorder_id *p_id);
extern pjsua_conf_port_id pjsua_recorder_get_conf_port(pjsua_recorder_id id);
extern pj_status_t pjsua_recorder_get_port(pjsua_recorder_id id, pjmedia_port **p_port);
extern pj_status_t pjsua_recorder_destroy(pjsua_recorder_id id);
struct pjmedia_aud_dev_info
{
  char name[64];
  unsigned input_count;
  unsigned output_count;
  unsigned default_samples_per_sec;
  char driver[32];
  unsigned caps;
  unsigned routes;
  unsigned ext_fmt_cnt;
  pjmedia_format ext_fmt[8];
};
extern pj_status_t pjsua_enum_aud_devs(pjmedia_aud_dev_info info[], unsigned *count);
struct pjmedia_snd_dev_info
{
  char name[64];
  unsigned input_count;
  unsigned output_count;
  unsigned default_samples_per_sec;
};
extern pj_status_t pjsua_enum_snd_devs(pjmedia_snd_dev_info info[], unsigned *count);
extern pj_status_t pjsua_get_snd_dev(int *capture_dev, int *playback_dev);
extern pj_status_t pjsua_set_snd_dev(int capture_dev, int playback_dev);
extern pj_status_t pjsua_set_null_snd_dev(void);
extern pjmedia_port *pjsua_set_no_snd_dev(void);
extern pj_status_t pjsua_set_ec(unsigned tail_ms, unsigned options);
extern pj_status_t pjsua_get_ec_tail(unsigned *p_tail_ms);
extern pj_bool_t pjsua_snd_is_active(void);
typedef enum pjmedia_aud_dev_cap {PJMEDIA_AUD_DEV_CAP_EXT_FORMAT = 1, PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY = 2, PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY = 4, PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING = 8, PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING = 16, PJMEDIA_AUD_DEV_CAP_INPUT_SIGNAL_METER = 32, PJMEDIA_AUD_DEV_CAP_OUTPUT_SIGNAL_METER = 64, PJMEDIA_AUD_DEV_CAP_INPUT_ROUTE = 128, PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE = 256, PJMEDIA_AUD_DEV_CAP_EC = 512, PJMEDIA_AUD_DEV_CAP_EC_TAIL = 1024, PJMEDIA_AUD_DEV_CAP_VAD = 2048, PJMEDIA_AUD_DEV_CAP_CNG = 4096, PJMEDIA_AUD_DEV_CAP_PLC = 8192, PJMEDIA_AUD_DEV_CAP_MAX = 16384} pjmedia_aud_dev_cap;
extern pj_status_t pjsua_snd_set_setting(pjmedia_aud_dev_cap cap, const void *pval, pj_bool_t keep);
extern pj_status_t pjsua_snd_get_setting(pjmedia_aud_dev_cap cap, void *pval);
extern pj_status_t pjsua_enum_codecs(pjsua_codec_info id[], unsigned *count);
extern pj_status_t pjsua_codec_set_priority(const pj_str_t *codec_id, pj_uint8_t priority);
extern pj_status_t pjsua_codec_get_param(const pj_str_t *codec_id, pjmedia_codec_param *param);
extern pj_status_t pjsua_codec_set_param(const pj_str_t *codec_id, const pjmedia_codec_param *param);

