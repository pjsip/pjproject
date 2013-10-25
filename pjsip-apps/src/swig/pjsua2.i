%module(directors="1") pjsua2

//
// Suppress few warnings
//
#pragma SWIG nowarn=312		// 312: nested struct (in types.h, sip_auth.h)

//
// Header section
//
%{
#include "pjsua2.hpp"
using namespace std;
using namespace pj;
%}

//
// STL stuff.
//
%include "std_string.i"
%include "std_vector.i"

namespace std
{
	%template(StringVector) std::vector<std::string>;
	%template(IntVector) std::vector<int>;
	%template(AuthCredInfoVector) std::vector<AuthCredInfo>;
	%template(SipMultipartPartVector) std::vector<SipMultipartPart>;
	%template(SipHeaderVector) std::vector<SipHeader>;
}

//
// Classes that can be extended in the target language
//
%feature("director") LogWriter; 
%feature("director") EpCallback;

//
// And include supporting constants etc. Here we don't want to export
// the whole file, so we start with ignoring everything and then enable
// selected symbols  
//
#define PJ_BEGIN_DECL
#define PJ_END_DECL
#define PJ_DECL(type)		type
#define PJ_INLINE(type)		type
#define PJ_DECL_DATA(type)	extern type

//
// Start with ignoring everything
//
%ignore "";

// Process the files. Keep them sorted.

//
//
%rename("%s") pj_file_access;				// Unignore this
%rename("%s", regexmatch$name="PJ_O_.*") "";		// Unignore this
%include "pj/file_io.h"

//
//
%rename("%s") pj_log_decoration;			// Unignore this
%rename("%s", regexmatch$name="PJ_LOG_HAS_.*") "";	// Unignore this
%include "pj/log.h"

//
//
%rename("%s") pj_qos_type;				// Unignore this
%rename("%s", regexmatch$name="PJ_QOS_TYPE_.*") "";	// Unignore this
%rename("%s") pj_qos_flag;				// Unignore this
%rename("%s", regexmatch$name="PJ_QOS_PARAM_HAS_.*") "";// Unignore this
%rename("%s") pj_qos_wmm_prio;				// Unignore this
%rename("%s", regexmatch$name="PJ_QOS_WMM_PRIO_.*") "";	// Unignore this
// TODO:
//  Wish there is a better way to unignore a struct than
//  listing the fields one by one like below. Using regex
//  doesn't seem to work?
%rename("%s") pj_qos_params;				// Unignore this
%rename("%s") pj_qos_params::flags;			// Unignore this
%rename("%s") pj_qos_params::dscp_val;			// Unignore this
%rename("%s") pj_qos_params::so_prio;			// Unignore this
%rename("%s") pj_qos_params::wmm_prio;			// Unignore this
%include "pj/sock_qos.h"

//
//
%rename("%s") pj_ssl_cipher;				// Unignore this
%rename("%s", regexmatch$name="PJ_O_.*") "";		// Unignore this
%include "pj/ssl_sock.h"

//
//
%rename("%s") pj_status_t;				// Unignore this
%rename("%s") PJ_SUCCESS;				// Unignore this
%include "pj/types.h"

//
//
%rename("%s") pj_stun_nat_type;				// Unignore this
%rename("%s", regexmatch$name="PJ_STUN_NAT_TYPE_.*") "";// Unignore this
%include "pjnath/nat_detect.h"

//
//
%rename("%s") pj_turn_tp_type;				// Unignore this
%rename("%s", regexmatch$name="PJ_TURN_TP_.*") "";	// Unignore this
%include "pjnath/turn_session.h"

//
//
%rename("%s") pjmedia_vid_stream_rc_method;		// Unignore this
%rename("%s", regexmatch$name="PJMEDIA_VID_STREAM_RC_.*") "";// Unignore this
%include "pjmedia/vid_stream.h"

//
//
%rename("%s") pjmedia_srtp_use;				// Unignore this
%rename("%s", regexmatch$name="PJMEDIA_SRTP_.*") "";	// Unignore this
%include "pjmedia/transport_srtp.h"

//
//
%rename("%s") pjmedia_vid_dev_index;			// Unignore this
%rename("%s") PJMEDIA_VID_DEFAULT_CAPTURE_DEV;		// Unignore this
%rename("%s") PJMEDIA_VID_DEFAULT_RENDER_DEV;		// Unignore this
%rename("%s") PJMEDIA_VID_INVALID_DEV;			// Unignore this
%include "pjmedia-videodev/videodev.h"

//
//
%rename("%s") pjsip_cred_data_type;			// Unignore this
%rename("%s", regexmatch$name="PJSIP_CRED_DATA_.*") "";	// Unignore this
%include "pjsip/sip_auth.h"

//
//
%rename("%s") pjsip_status_code;			// Unignore this
%rename("%s", regexmatch$name="PJSIP_SC_.*") "";	// Unignore this
%include "pjsip/sip_msg.h"

//
//
%rename("%s") pjsip_transport_type_e;			// Unignore this
%rename("%s") pjsip_transport_flags_e;			// Unignore this
%rename("%s") pjsip_transport_state;			// Unignore this
%rename("%s", regexmatch$name="PJSIP_TRANSPORT_.*") "";	// Unignore this
%rename("%s", regexmatch$name="PJSIP_TP_STATE_.*") "";	// Unignore this
%include "pjsip/sip_transport.h"

//
//
%rename("%s") pjsip_ssl_method;				// Unignore this
%rename("%s", regexmatch$name="PJSIP_TLS.*_METHOD") "";	// Unignore this
%rename("%s", regexmatch$name="PJSIP_SSL.*_METHOD") "";	// Unignore this
%include "pjsip/sip_transport_tls.h"

//
//
%rename("%s") PJSUA_INVALID_ID;				// Unignore this
%rename("%s") pjsua_state;				// Unignore this
%rename("%s", regexmatch$name="PJSUA_STATE_.*") "";	// Unignore this
%rename("%s") pjsua_stun_use;				// Unignore this
%rename("%s", regexmatch$name="PJSUA_STUN_USE_.*") "";	// Unignore this
%rename("%s") pjsua_call_hold_type;			// Unignore this
%rename("%s", regexmatch$name="PJSUA_CALL_HOLD_TYPE_.*") "";// Unignore this
%rename("%s") pjsua_acc_id;				// Unignore this
%rename("%s") pjsua_destroy_flag;			// Unignore this
%rename("%s", regexmatch$name="PJSUA_DESTROY_.*") "";	// Unignore this
%include "pjsua-lib/pjsua.h"

//
// Back to unignore everything
//
%rename("%s") "";

//
// Ignore stuffs in pjsua2 itself
//
%ignore fromPj;
%ignore toPj;

//
// Now include the API itself.
//
%include "pjsua2/types.hpp"
%include "pjsua2/endpoint.hpp"
#include "pjsua2/account.hpp"

