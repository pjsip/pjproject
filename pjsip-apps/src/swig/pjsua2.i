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

// Constants from PJSIP libraries
%include "symbols.i"


//
// Classes that can be extended in the target language
//
%feature("director") LogWriter;
%feature("director") Endpoint; 
%feature("director") Account;


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

%ignore container_node_op;
%include "pjsua2/persistent.hpp"

%include "pjsua2/siptypes.hpp"

%template(SipHeaderVector)		std::vector<pj::SipHeader>;
%template(AuthCredInfoVector)		std::vector<pj::AuthCredInfo>;
%template(SipMultipartPartVector)	std::vector<pj::SipMultipartPart>;

%include "pjsua2/endpoint.hpp"
%include "pjsua2/account.hpp"

%ignore JsonDocument::allocElement();
%ignore JsonDocument::getPool();
%include "pjsua2/json.hpp"
