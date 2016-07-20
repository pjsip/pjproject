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

#ifdef SWIGPYTHON
  %feature("director:except") {
    if( $error != NULL ) {
      PyObject *ptype, *pvalue, *ptraceback;
      PyErr_Fetch( &ptype, &pvalue, &ptraceback );
      PyErr_Restore( ptype, pvalue, ptraceback );
      PyErr_Print();
      //Py_Exit(1);
    }
  }
#endif

// Allow C++ exceptions to be handled in Java
#ifdef SWIGJAVA
  %typemap(throws, throws="java.lang.Exception") pj::Error {
  jclass excep = jenv->FindClass("java/lang/Exception");
  if (excep)
    jenv->ThrowNew(excep, $1.info(true).c_str());
  return $null;
}

  // Force the Error Java class to extend java.lang.Exception
  %typemap(javabase) pj::Error "java.lang.Exception";

  %typemap(javacode) pj::Error %{

  // Override getMessage()
  public String getMessage() {
    return getTitle();
  }
  
  // Disable serialization (check ticket #1868)
  private void writeObject(java.io.ObjectOutputStream out) throws java.io.IOException {
    throw new java.io.NotSerializableException("Check ticket #1868!");
  }
  private void readObject(java.io.ObjectInputStream in) throws java.io.IOException {
    throw new java.io.NotSerializableException("Check ticket #1868!");
  }

%}
#endif


// Constants from PJSIP libraries
%include "symbols.i"


//
// Classes that can be extended in the target language
//
%feature("director") LogWriter;
%feature("director") Endpoint; 
%feature("director") Account;
%feature("director") Call;
%feature("director") Buddy;
%feature("director") FindBuddyMatch;
%feature("director") AudioMediaPlayer;

//
// STL stuff.
//
%include "std_string.i"
%include "std_vector.i"

%template(StringVector)			std::vector<std::string>;
%template(IntVector) 			std::vector<int>;

//
// Ignore stuffs in pjsua2
//
%ignore fromPj;
%ignore toPj;

//
// Now include the API itself.
//
%include "pjsua2/types.hpp"

%ignore pj::ContainerNode::op;
%ignore pj::ContainerNode::data;
%ignore container_node_op;
%ignore container_node_internal_data;
%include "pjsua2/persistent.hpp"

%include "pjsua2/siptypes.hpp"

%template(SipHeaderVector)		std::vector<pj::SipHeader>;
%template(AuthCredInfoVector)		std::vector<pj::AuthCredInfo>;
%template(SipMultipartPartVector)	std::vector<pj::SipMultipartPart>;
%template(BuddyVector)			std::vector<pj::Buddy*>;
%template(AudioMediaVector)		std::vector<pj::AudioMedia*>;
%template(ToneDescVector)		std::vector<pj::ToneDesc>;
%template(ToneDigitVector)		std::vector<pj::ToneDigit>;
%template(ToneDigitMapVector)	        std::vector<pj::ToneDigitMapDigit>;
%template(MediaFormatVector)		std::vector<pj::MediaFormat*>;
%template(AudioDevInfoVector)		std::vector<pj::AudioDevInfo*>;
%template(CodecInfoVector)		std::vector<pj::CodecInfo*>;
%template(VideoDevInfoVector)		std::vector<pj::VideoDevInfo*>;
%template(CodecFmtpVector)		std::vector<pj::CodecFmtp>;	

%ignore pj::WindowHandle::display;
%ignore pj::WindowHandle::window;

/* pj::WindowHandle::setWindow() receives Surface object */
#if defined(SWIGJAVA) && defined(__ANDROID__)
%{
#if defined(PJMEDIA_HAS_VIDEO) && PJMEDIA_HAS_VIDEO!=0
#  include <android/native_window_jni.h>
#else
#  define ANativeWindow_fromSurface(a,b) NULL
#endif
%}
%typemap(in) jobject surface {
    $1 = ($input? (jobject)ANativeWindow_fromSurface(jenv, $input): NULL);
}
%extend pj::WindowHandle {
    void setWindow(jobject surface) { $self->window = surface; }
}
#else
%extend pj::WindowHandle {
    void setWindow(long long hwnd) { $self->window = (void*)hwnd; }
}
#endif

%include "pjsua2/media.hpp"
%include "pjsua2/presence.hpp"
%include "pjsua2/account.hpp"
%include "pjsua2/call.hpp"

%template(CallMediaInfoVector)          std::vector<pj::CallMediaInfo>;

%ignore pj::JsonDocument::allocElement;
%ignore pj::JsonDocument::getPool;
%include "pjsua2/json.hpp"

// Try force Java GC before destroying the lib:
// - to avoid late destroy of PJ objects by GC
// - to avoid destruction of PJ objects from a non-registered GC thread
#ifdef SWIGJAVA
%rename(libDestroy_) pj::Endpoint::libDestroy;
%typemap(javacode) pj::Endpoint %{
  public void libDestroy(long prmFlags) throws java.lang.Exception {
	Runtime.getRuntime().gc();
	libDestroy_(prmFlags);
  }

  public void libDestroy() throws java.lang.Exception {
	Runtime.getRuntime().gc();
	libDestroy_();
  }
%}
#endif

%include "pjsua2/endpoint.hpp"

