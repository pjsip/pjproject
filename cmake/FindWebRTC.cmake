# WebRTC AEC/AECM/NS as bundled in third_party/webrtc, built separately
# (autoconf --with-external-webrtc). Headers are included as
# <webrtc/modules/...>, so the include dir is the one holding webrtc/.

find_path(WebRTC_INCLUDE_DIR
  NAMES
    "webrtc/modules/audio_processing/aec/include/echo_cancellation.h"
  PATH_SUFFIXES
    pjproject/third_party/webrtc
)
mark_as_advanced(WebRTC_INCLUDE_DIR)

find_library(WebRTC_LIBRARY
  NAMES
    webrtc
    libwebrtc
  PATH_SUFFIXES
    pjproject/third_party
)
mark_as_advanced(WebRTC_LIBRARY)

if(WebRTC_INCLUDE_DIR AND WebRTC_LIBRARY)
  set(_webrtc_check_libs "${WebRTC_LIBRARY}")
  if(WebRTC_LIBRARY MATCHES "\\${CMAKE_STATIC_LIBRARY_SUFFIX}$" AND UNIX AND NOT APPLE)
    # A static libwebrtc needs libm
    list(APPEND _webrtc_check_libs m)
  endif()

  include(Pj/CheckLibraryUsable)
  pj_check_library_usable(WebRTC
    CODE "
#include <webrtc/modules/audio_processing/aec/aec_core.h>
#include <webrtc/modules/audio_processing/aec/include/echo_cancellation.h>
int main(void) { void *aec = WebRtcAec_Create(); WebRtcAec_Free(aec); return 0; }
"
    INCLUDE_DIRS ${WebRTC_INCLUDE_DIR}
    LIBRARIES ${_webrtc_check_libs}
  )
  unset(_webrtc_check_libs)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WebRTC
  REQUIRED_VARS
    WebRTC_LIBRARY
    WebRTC_INCLUDE_DIR
    WebRTC_USABLE
)

if(WebRTC_FOUND)
  set(WebRTC_LIBRARIES ${WebRTC_LIBRARY})
  set(WebRTC_INCLUDE_DIRS ${WebRTC_INCLUDE_DIR})

  if(NOT TARGET WebRTC::WebRTC)
    add_library(WebRTC::WebRTC UNKNOWN IMPORTED)
    set_target_properties(WebRTC::WebRTC PROPERTIES
      IMPORTED_LOCATION "${WebRTC_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${WebRTC_INCLUDE_DIRS}"
    )
  endif()
endif()
