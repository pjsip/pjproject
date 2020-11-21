/*
  See https://github.com/RobertBeckebans/RBDOOM-3-BFG/blob/master/neo/libs/mingw-hacks/sal.h
*/
/* From PortAudio, which is under MIT license:
 * https://www.assembla.com/code/portaudio/subversion/nodes/portaudio/trunk/src/hostapi/wasapi/mingw-include/sal.h
 */
#pragma once

#if __GNUC__ >=3
#pragma GCC system_header
#endif

#define __in
#define __out
#define __deref_in
#define __deref_inout_opt
#define __field_ecount_opt(x)
#define __in_bcount_opt(size)

