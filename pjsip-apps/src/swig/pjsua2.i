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

// Change for ilogixx:
//
// Convert Char* to byte[]
//
%include "arrays_csharp.i"

%typemap(imtype) unsigned char *frameBuffer "System.IntPtr"
%typemap(cstype) unsigned char *frameBuffer "System.IntPtr"
%typemap(csin)   unsigned char *frameBuffer "$csinput"
%typemap(out)    unsigned char *frameBuffer %{ $result = $1; %}
%typemap(csout)  unsigned char *frameBuffer { return $imcall; }
%typemap(csvarout, excode=SWIGEXCODE2) unsigned char* frameBuffer %{ 
  get { 
	    global::System.IntPtr ret = pjsua2PINVOKE.AudioInputDevice_OnBufferReceivedParam_frameBuffer_get(swigCPtr);       
        return ret;
  } 
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

 // Change for ilogixx:
//Allow application exceptions to be enriched with pj specific information
#ifdef SWIGCSHARP
	%typemap(throws, canthrow=1) pj::Error {
		SWIG_CSharpSetPendingException(SWIG_CSharpApplicationException, (std::string("C++ pj::Error:\nStatus: ") + std::to_string($1.status).c_str() + "\nTitle: " + $1.title.c_str() + "\nReason: " + $1.reason.c_str() + "\n" + $1.info(true).c_str()).c_str());
	    return $null;
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

#ifdef SWIGJAVA
%include "enumtypeunsafe.swg"
%javaconst(1);

%pragma(java) jniclasscode=%{
  static {
    try {
	System.loadLibrary("openh264");
    } catch (UnsatisfiedLinkError e) {
	System.err.println("Failed to load native library openh264\n" + e);
	System.out.println("This could be safely ignored if you " +
			   "don't use OpenH264 video codec.");
    }
    try {
	System.loadLibrary("pjsua2");
    } catch (UnsatisfiedLinkError e) {
	System.err.println("Failed to load native library pjsua2\n" + e);
    }
  }
%}

#endif

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

 // Change for ilogixx:
%feature("director") AudioPlayer;
%feature("director") AudioInputDevice;
%feature("director") AudioOutputDevice;

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

 // Change for ilogixx:
%import "../../pjlib/include/pj/config_site.h"
%import "../../pjsip/include/pjsua2/config.hpp"

//
// Now include the API itself.
//
 // Change for ilogixx:
%include "../../pjsip/include/pjsua2/types.hpp"

%ignore pj::ContainerNode::op;
%ignore pj::ContainerNode::data;
%ignore container_node_op;
%ignore container_node_internal_data;
 // Change for ilogixx:
%include "../../pjsip/include/pjsua2/persistent.hpp"

%include "../../pjsip/include/pjsua2/siptypes.hpp"

%template(SipHeaderVector)		std::vector<pj::SipHeader>;
%template(AuthCredInfoVector)		std::vector<pj::AuthCredInfo>;
%template(SrtpCryptoVector)		std::vector<pj::SrtpCrypto>;
%template(SipMultipartPartVector)	std::vector<pj::SipMultipartPart>;
%template(BuddyVector)			std::vector<pj::Buddy*>;
%template(BuddyVector2)			std::vector<pj::Buddy>;
%template(AudioMediaVector)		std::vector<pj::AudioMedia*>;
%template(AudioMediaVector2)		std::vector<pj::AudioMedia>;
%template(VideoMediaVector)		std::vector<pj::VideoMedia>;
%template(ToneDescVector)		std::vector<pj::ToneDesc>;
%template(ToneDigitVector)		std::vector<pj::ToneDigit>;
%template(ToneDigitMapVector)	        std::vector<pj::ToneDigitMapDigit>;
%template(AudioDevInfoVector)		std::vector<pj::AudioDevInfo*>;
%template(AudioDevInfoVector2)		std::vector<pj::AudioDevInfo>;
%template(CodecInfoVector)		std::vector<pj::CodecInfo*>;
%template(CodecInfoVector2)		std::vector<pj::CodecInfo>;
%template(VideoDevInfoVector)		std::vector<pj::VideoDevInfo*>;
%template(VideoDevInfoVector2)		std::vector<pj::VideoDevInfo>;
%template(CodecFmtpVector)		std::vector<pj::CodecFmtp>;	
%template(MediaFormatAudioVector)       std::vector<pj::MediaFormatAudio>;
%template(MediaFormatVideoVector)       std::vector<pj::MediaFormatVideo>;
%template(CallMediaInfoVector)          std::vector<pj::CallMediaInfo>;
%template(RtcpFbCapVector)              std::vector<pj::RtcpFbCap>;
%template(SslCertNameVector)            std::vector<pj::SslCertName>;

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

// Change for ilogixx:
%include "../../pjsip/include/pjsua2/media.hpp"
%include "../../pjsip/include/pjsua2/presence.hpp"
%include "../../pjsip/include/pjsua2/account.hpp"
%include "../../pjsip/include/pjsua2/call.hpp"

%ignore pj::JsonDocument::allocElement;
%ignore pj::JsonDocument::getPool;

// Change for ilogixx:
%include "../../pjsip/include/pjsua2/json.hpp"

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

// Change for ilogixx:
%include "../../pjsip/include/pjsua2/endpoint.hpp"
%include "../../pjsip/include/pjsua2/conferenceBridge.hpp"
%include "../../pjsip/include/pjsua2/audioRecorder.hpp"
%include "../../pjsip/include/pjsua2/audioPlayer.hpp"
%include "../../pjsip/include/pjsua2/audioInputDevice.hpp"
%include "../../pjsip/include/pjsua2/audioOutputDevice.hpp"