#include "callbacks.h"

static PjsuaCallback* cb = NULL;

void setPjsuaCallback(PjsuaCallback* callback) {
	cb = callback;
}

static void on_call_media_state_wrapper(pjsua_call_id call_id) {
	cb->on_call_media_state(call_id);
}

static pjsua_callback my_cb_proxy = {
	NULL,
	NULL,
	NULL,
	&on_call_media_state_wrapper
};

pjsua_callback* PJSUA_CALLBACK_PROXY = &my_cb_proxy;
