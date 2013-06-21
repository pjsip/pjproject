%module (directors="1") pjsua_cb

#pragma SWIG nowarn=312		/* nested struct/class/union */

%header %{
    #include <pjsua-lib/pjsua.h>
    #include "callbacks.h"
%}

# Get pjsua definitions
%import "output/pjsua.i"

%feature("director") PjsuaCallback;
%include "callbacks.h"
