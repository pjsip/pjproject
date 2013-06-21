%module pjsua

%include "typemaps.i"
%include "enums.swg"
%include "arrays_java.i"

%header %{
    #include <pjsua-lib/pjsua.h>
    extern pjsua_callback* PJSUA_CALLBACK_PROXY;
%}

/* 'void *' shall be handled as byte arrays */
%typemap(jni)		 void * "void *"
%typemap(jtype) 	 void * "byte[]"
%typemap(jstype) 	 void * "byte[]"
%typemap(javain) 	 void * "$javainput"
%typemap(javadirectorin) void * "$jniinput"
%typemap(in) 		 void * { $1 = $input; }
%typemap(out) 		 void * { $result = $1; }
%typemap(javaout)	 void * { return $jnicall; }

/* Apply output args */
%apply unsigned	*INOUT  { unsigned *count };
%apply unsigned *OUTPUT { unsigned *tx_level };
%apply unsigned *OUTPUT { unsigned *rx_level };
%apply unsigned *OUTPUT { unsigned *p_tail_ms };
%apply int 	*OUTPUT { pjsua_acc_id *p_acc_id };
%apply int 	*OUTPUT { pjsua_call_id *p_call_id };
%apply int	*OUTPUT { pjsua_transport_id *p_id };
%apply int	*OUTPUT { pjsua_recorder_id *p_id };
%apply int 	*OUTPUT { pjsua_player_id *p_id };
%apply int 	*OUTPUT { pjsua_buddy_id *p_buddy_id };
%apply int 	*OUTPUT { pjsua_conf_port_id *p_id };
%apply int 	*OUTPUT { int *capture_dev };
%apply int 	*OUTPUT { int *playback_dev };
%apply int 	*OUTPUT { pj_stun_nat_type * };
%apply int[ANY] 	{ pjmedia_format_id dec_fmt_id[8] };

/* Array of pj_str_t */
JAVA_ARRAYSOFCLASSES(pj_str_t)
#ifndef __cplusplus
    /* On C target, pj_str_t::cArrayWrap/Unwrap Java code are missing, this somehow 'fixes' it. */
    JAVA_ARRAYSOFCLASSES(struct pj_str_t)
#endif

/* Array of pj_ssl_cipher in pjsip_tls_setting. Warning: this breaks JAVA_ARRAYS_TYPEMAPS(int)! */
%typemap(out) int[] %{$result = SWIG_JavaArrayOutInt(jenv, (int*)$1, arg1->ciphers_num); %}
%apply int[] { pj_ssl_cipher* };
%ignore pjsip_tls_setting::ciphers;
%ignore pjsip_tls_setting::ciphers_num;
%extend pjsip_tls_setting {
    void setCiphers(pj_ssl_cipher *ciphers, int num) {
        int i;
	$self->ciphers = (pj_ssl_cipher*)calloc(num, sizeof(pj_ssl_cipher));
	for (i=0; i<num; ++i) $self->ciphers[i] = ciphers[i];
	$self->ciphers_num = num;
    }
    pj_ssl_cipher* getCiphers() {
	return $self->ciphers;
    }
};


/* C++ SWIG target doesn't support nested class (C version does though).
 * This is minimal workaround, ignore nested class as if it is not there.
 * TODO: proper workaround will be moving out inner classes to global scope.
 */
#ifdef __cplusplus
    %nestedworkaround pjmedia_codec_fmtp::param;
    %nestedworkaround pjsip_cred_info::ext;
    %nestedworkaround pjsip_event::body;
#endif

/* Typemaps for marshalling pjmedia_port ** */
%typemap(jni) pjmedia_port **p_port "jobject"
%typemap(jtype) pjmedia_port **p_port "pjmedia_port"
%typemap(jstype) pjmedia_port **p_port "pjmedia_port"

/* Typemaps for pjmedia_port ** as a parameter output type */
%typemap(in) pjmedia_port **p_port (pjmedia_port *ppMediaPort = 0) %{ $1 = &ppMediaPort; %}
%typemap(argout) pjmedia_port **p_port {
  // Give Java proxy the C pointer (of newly created object)
  jclass clazz = JCALL1(FindClass, jenv, "org/pjsip/pjsua/pjmedia_port");
  jfieldID fid = JCALL3(GetFieldID, jenv, clazz, "swigCPtr", "J");
  jlong cPtr = 0;
  *(pjmedia_port **)&cPtr = *$1;
  JCALL3(SetLongField, jenv, $input, fid, cPtr);
}
%typemap(javain) pjmedia_port **p_port "$javainput"


/* Strip "pjsua_" prefix from pjsua functions, for better compatibility with 
 * pjsip-jni & csipsimple.
 */
%rename("%(strip:[pjsua_])s", %$isfunction) "";

/* Automatically init pjsua_config::cb to cb proxy via pjsua_config_default() */
/* 1. Hide 'cb' from 'pjsua_config' */
%ignore pjsua_config::cb;
/* 2. Ignore original pjsua_config_default() */
%ignore pjsua_config_default;
/* 3. Optional, put back "pjsua_" prefix, if stripping is not preferred */
//%rename(pjsua_config_default) config_default;
/* 4. Put custom implementation */
%inline %{
    void config_default(pjsua_config *cfg) {
        pjsua_config_default(cfg);
	cfg->cb = *PJSUA_CALLBACK_PROXY;
    }
%}

/* Global constants */
#define PJ_SUCCESS  0
#define PJ_TRUE     1
#define PJ_FALSE    0

