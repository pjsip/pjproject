
#include <pj/log.h>

/* Check if we need to use the fixed point version */
#if !defined(PJ_HAS_FLOATING_POINT) || PJ_HAS_FLOATING_POINT==0
#   define FIXED_POINT
#endif


#define inline __inline
#define restrict

#include "misc.h"

#ifdef _MSC_VER
#   pragma warning(disable: 4100)   // unreferenced formal parameter
#   pragma warning(disable: 4101)   // unreferenced local variable
#   pragma warning(disable: 4244)   // conversion from 'double ' to 'float '
#   pragma warning(disable: 4305)   // truncation from 'const double ' to 'float '
#   pragma warning(disable: 4701)   // local variable used without initialized
#endif

/*
 * Override miscellaneous Speex functions.
 */
#define OVERRIDE_SPEEX_ERROR
#define speex_error(str) PJ_LOG(4,("speex", "error: %s", str))

#define OVERRIDE_SPEEX_WARNING
#define speex_warning(str) PJ_LOG(5,("speex", "warning: %s", str))

#define OVERRIDE_SPEEX_WARNING_INT
#define speex_warning_int(str,val)  PJ_LOG(5,("speex", "warning: %s: %d", str, val))

