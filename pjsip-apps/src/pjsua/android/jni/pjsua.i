%module (directors="1") pjsua

%{
#include "pjsua_app_callback.h"
#include "../../pjsua_app.h"

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

int pjsuaStart();
void pjsuaDestroy();
int pjsuaRestart();

/* turn on director wrapping PjsuaAppCallback */
%feature("director") PjsuaAppCallback;

%include "pjsua_app_callback.h"

void setCallbackObject(PjsuaAppCallback* callback);

