/* $Id$ */

%header %{
    #include "callbacks.h"
%}

%feature("director") PjsuaCallback;
%ignore pjsua_callback;
%ignore pjsua_config::cb;
%extend pjsua_config {
    void setCb(PjsuaCallback *pjsuaCb) {
	$self->cb = *PJSUA_CALLBACK_PROXY;
	setPjsuaCallback(pjsuaCb);
    }
}

%include <callbacks.h>
