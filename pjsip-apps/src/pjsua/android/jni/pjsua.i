%module (directors="1") pjsua

%{
#include "pjsua_app_callback.h"
#include "../../pjsua_app.h"

#if defined(PJMEDIA_HAS_VIDEO) && PJMEDIA_HAS_VIDEO!=0
#  include <android/native_window_jni.h>
#else
#  define ANativeWindow_fromSurface(a,b) NULL
#endif
%}

/* Turn on director wrapping PjsuaAppCallback */
%feature("director") PjsuaAppCallback;

/* Convert Surface object to ANativeWindow for setIncomingVideoRenderer() */
%typemap(in) jobject surface {
    $1 = $input? (jobject)ANativeWindow_fromSurface(jenv, $input) : NULL;
}

%include "pjsua_app_callback.h"
