/* This file is included by endian.h */

#include <pj/types.h>
#undef INLINE
#define INLINE	__inline


#ifndef WITH_PJ
#   error This needs to be declared!
#endif

#ifdef _MSC_VER
#   pragma warning(disable: 4244)   // conversion from 'double ' to 'unsigned int ', possible loss of data
#   pragma warning(disable: 4761)   // integral size mismatch in argument; conversion supplied
#endif

#define STATIC

