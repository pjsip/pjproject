/* $Id: callbacks.i 4566 2013-07-17 20:20:50Z nanang $ */

%header %{
    #include "callbacks.h"
%}

/*
 * Wrap pjsua_logging_config callback
 */
%feature("director") PjsuaLoggingConfigCallback;
%ignore pjsua_logging_config::cb;
%extend pjsua_logging_config {
    void setCb(PjsuaLoggingConfigCallback* callback) {
	setPjsuaLoggingConfigCallback(callback);
	$self->cb = callback? &pjsua_logging_config_callback_proxy : NULL;
    }
}


/*
 * Wrap pj_timer_heap_callback in pj_timer_entry
 */
%feature("director") PjTimerHeapCallback;
%ignore pj_timer_entry::cb;
/* Suppress user_data, app can put user data in PjTimerHeapCallback inherited class */
%ignore pj_timer_entry::user_data;
%extend pj_timer_entry {
    void setCb(PjTimerHeapCallback* callback) {
	setPjTimerHeapCallback($self, callback);
	$self->cb = callback? &pj_timer_heap_callback_proxy : NULL;
    }
}


/*
 * Wrap pjsua_callback
 */
%feature("director") PjsuaCallback;
%ignore pjsua_callback;
%ignore pjsua_config::cb;
%extend pjsua_config {
    void setCb(PjsuaCallback *callback) {
	setPjsuaCallback(callback);
	if (callback)
	    $self->cb = *pjsua_callback_proxy;
	else
	    pj_bzero(&$self->cb, sizeof($self->cb));
    }
}


%include <callbacks.h>

/* Ignore these callbacks */
%ignore pjsip_msg_body::print_body;
%ignore pjsip_msg_body::clone_data;
%ignore pjsip_tx_data::cb;
%ignore pjsip_transaction::state_handler;

/* Ignore some struct members with aux credential callbacks, at least temporarily */
%ignore pjsip_cred_info::ext;
%ignore pjsip_cred_info_ext;
%ignore pjsip_cred_info_ext_aka;

%ignore pj_stun_auth_cred::data::dyn_cred;
%ignore pj_stun_auth_cred_data::dyn_cred;
%ignore pj_stun_auth_cred_data_dyn_cred;
