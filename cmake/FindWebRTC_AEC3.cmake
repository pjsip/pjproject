# WebRTC AEC3 as bundled in third_party/webrtc_aec3, built separately
# (autoconf --with-external-webrtc-aec3). Headers are included as
# "modules/...", "api/...", "rtc_base/..." and "absl/..." from one root.

find_path(WebRTC_AEC3_INCLUDE_DIR
  NAMES
    "modules/audio_processing/aec3/echo_canceller3.h"
  PATH_SUFFIXES
    pjproject/third_party/webrtc_aec3
)
mark_as_advanced(WebRTC_AEC3_INCLUDE_DIR)

find_library(WebRTC_AEC3_LIBRARY
  NAMES
    webrtc_aec3
    webrtc-aec3
  PATH_SUFFIXES
    pjproject/third_party
)
mark_as_advanced(WebRTC_AEC3_LIBRARY)

# Definitions the headers require
set(_aec3_definitions WEBRTC_APM_DEBUG_DUMP=0)
if(WIN32)
  list(APPEND _aec3_definitions WEBRTC_WIN=1)
else()
  list(APPEND _aec3_definitions WEBRTC_POSIX=1)
endif()

if(WebRTC_AEC3_INCLUDE_DIR AND WebRTC_AEC3_LIBRARY)
  set(_aec3_check_definitions)
  foreach(_def IN LISTS _aec3_definitions)
    list(APPEND _aec3_check_definitions "-D${_def}")
  endforeach()

  set(_aec3_check_libs "${WebRTC_AEC3_LIBRARY}")
  if(WebRTC_AEC3_LIBRARY MATCHES "\\${CMAKE_STATIC_LIBRARY_SUFFIX}$")
    # A static libwebrtc_aec3 needs these
    if(WIN32 AND NOT CMAKE_SYSTEM_NAME MATCHES "Windows(Store|Phone)")
      list(APPEND _aec3_check_libs winmm)
    elseif(UNIX AND NOT APPLE)
      list(APPEND _aec3_check_libs m)
      if(TARGET Threads::Threads)
        list(APPEND _aec3_check_libs Threads::Threads)
      endif()
    endif()
    if(ANDROID)
      list(APPEND _aec3_check_libs log)
    endif()
  endif()

  # AEC3 needs C++17
  set(_aec3_saved_cxx_standard "${CMAKE_CXX_STANDARD}")
  if(NOT CMAKE_CXX_STANDARD OR CMAKE_CXX_STANDARD LESS 17)
    set(CMAKE_CXX_STANDARD 17)
  endif()

  include(Pj/CheckLibraryUsable)
  pj_check_library_usable(WebRTC_AEC3
    LANGUAGE CXX
    CODE "
#include \"modules/audio_processing/aec3/echo_canceller3.h\"
int main() {
  webrtc::EchoCanceller3 ec(webrtc::EchoCanceller3Config(), 16000, 1, 1);
  return 0;
}
"
    INCLUDE_DIRS ${WebRTC_AEC3_INCLUDE_DIR}
    LIBRARIES ${_aec3_check_libs}
    DEFINITIONS ${_aec3_check_definitions}
  )

  if(_aec3_saved_cxx_standard)
    set(CMAKE_CXX_STANDARD "${_aec3_saved_cxx_standard}")
  else()
    unset(CMAKE_CXX_STANDARD)
  endif()
  unset(_aec3_saved_cxx_standard)
  unset(_aec3_check_libs)
  unset(_aec3_check_definitions)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WebRTC_AEC3
  REQUIRED_VARS
    WebRTC_AEC3_LIBRARY
    WebRTC_AEC3_INCLUDE_DIR
    WebRTC_AEC3_USABLE
)

if(WebRTC_AEC3_FOUND)
  set(WebRTC_AEC3_LIBRARIES ${WebRTC_AEC3_LIBRARY})
  set(WebRTC_AEC3_INCLUDE_DIRS ${WebRTC_AEC3_INCLUDE_DIR})

  if(NOT TARGET WebRTC_AEC3::WebRTC_AEC3)
    add_library(WebRTC_AEC3::WebRTC_AEC3 UNKNOWN IMPORTED)
    set_target_properties(WebRTC_AEC3::WebRTC_AEC3 PROPERTIES
      IMPORTED_LOCATION "${WebRTC_AEC3_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${WebRTC_AEC3_INCLUDE_DIRS}"
      INTERFACE_COMPILE_DEFINITIONS "${_aec3_definitions}"
      INTERFACE_COMPILE_FEATURES cxx_std_17
    )
    if(ANDROID)
      # logging uses __android_log_print
      set_target_properties(WebRTC_AEC3::WebRTC_AEC3 PROPERTIES
        INTERFACE_LINK_LIBRARIES log
      )
    endif()
  endif()
endif()
unset(_aec3_definitions)
