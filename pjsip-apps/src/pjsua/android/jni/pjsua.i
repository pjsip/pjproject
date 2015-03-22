%module (directors="1") pjsua

%{
#include "pjsua_app_callback.h"
#include "../../pjsua_app.h"
#include <android/native_window_jni.h>

#ifdef __cplusplus
extern "C" {
#endif
	int pjsuaStart();
	void pjsuaDestroy();
	int pjsuaRestart();
	void setCallbackObject(PjsuaAppCallback* callback);	
#ifdef __cplusplus
}
#endif
%}

/* Turn on director wrapping PjsuaAppCallback */
%feature("director") PjsuaAppCallback;

/* Convert Surface object to ANativeWindow for setIncomingVideoRenderer() */
%typemap(in) jobject surface {
    $1 = (jobject)ANativeWindow_fromSurface(jenv, $input);
}

%include "pjsua_app_callback.h"
