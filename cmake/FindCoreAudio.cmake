# CoreAudio audio device backend (macOS and iOS)
#
# Defines the imported target `CoreAudio::CoreAudio` when every framework is found.
# The framework list follows the one the autotools build links (aconfigure.ac).

set(_coreaudio_frameworks CoreAudio AudioToolbox Foundation)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  list(APPEND _coreaudio_frameworks CoreServices AudioUnit AppKit)
else()
  list(APPEND _coreaudio_frameworks CoreFoundation CFNetwork AVFoundation UIKit)
endif()

set(_coreaudio_libs)
foreach(_coreaudio_lib IN LISTS _coreaudio_frameworks)
  string(TOUPPER "CoreAudio_LIBRARY_${_coreaudio_lib}" _coreaudio_lib_var)
  list(APPEND _coreaudio_required_vars ${_coreaudio_lib_var})

  find_library(${_coreaudio_lib_var} "${_coreaudio_lib}")
  mark_as_advanced(${_coreaudio_lib_var})
  if(${_coreaudio_lib_var})
    list(APPEND _coreaudio_libs "${${_coreaudio_lib_var}}")
  endif()
endforeach()
unset(_coreaudio_lib)
unset(_coreaudio_lib_var)
unset(_coreaudio_frameworks)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CoreAudio
  REQUIRED_VARS
    ${_coreaudio_required_vars}
)

if(CoreAudio_FOUND)
  set(CoreAudio_LIBRARIES ${_coreaudio_libs})

  if(NOT TARGET CoreAudio::CoreAudio)
    add_library(coreaudio INTERFACE)
    set_target_properties(coreaudio PROPERTIES
      INTERFACE_LINK_LIBRARIES "${CoreAudio_LIBRARIES}"
    )
    add_library(CoreAudio::CoreAudio ALIAS coreaudio)
  endif()
endif()

unset(_coreaudio_libs)
unset(_coreaudio_required_vars)
