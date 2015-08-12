#ifdef _MSC_VER
#   pragma warning(disable: 4100)   // unreferenced formal parameter
#   pragma warning(disable: 4101)   // unreferenced local variable
#   pragma warning(disable: 4244)   // conversion from 'double ' to 'float '
#   pragma warning(disable: 4305)   // truncation from 'const double ' to 'float '
#   pragma warning(disable: 4018)   // signed/unsigned mismatch
//#   pragma warning(disable: 4701)   // local variable used without initialized
#endif

#if defined(__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__) >= 402
#  pragma GCC diagnostic ignored "-Wpragmas"
#  pragma GCC diagnostic ignored "-Wunused-const-variable"
#endif

#include <string.h>
#include "../../gsm/inc/config.h"
