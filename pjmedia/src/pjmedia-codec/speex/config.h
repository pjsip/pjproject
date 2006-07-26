
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
