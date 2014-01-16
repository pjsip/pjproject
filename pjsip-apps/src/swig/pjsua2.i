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

  // Override getMessage()
  %typemap(javacode) pj::Error %{
  public String getMessage() {
    return getTitle();
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
%template(MediaFormatVector)		std::vector<pj::MediaFormat*>;
%template(AudioDevInfoVector)		std::vector<pj::AudioDevInfo*>;
%template(CodecInfoVector)		std::vector<pj::CodecInfo*>;

%include "pjsua2/media.hpp"
%include "pjsua2/endpoint.hpp"
%include "pjsua2/presence.hpp"
%include "pjsua2/account.hpp"
%include "pjsua2/call.hpp"

%template(CallMediaInfoVector)          std::vector<pj::CallMediaInfo>;

%ignore pj::JsonDocument::allocElement;
%ignore pj::JsonDocument::getPool;
%include "pjsua2/json.hpp"
