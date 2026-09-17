# AVFoundation video capture backend (macOS and iOS)
#
# Defines the imported target `AVFoundation::AVFoundation` when every framework is found.
# The framework list follows the one the autotools build links (aconfigure.ac).

set(_avfoundation_frameworks AVFoundation CoreGraphics QuartzCore CoreVideo CoreMedia Foundation)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  list(APPEND _avfoundation_frameworks AppKit)
else()
  list(APPEND _avfoundation_frameworks UIKit)
endif()

set(_avfoundation_libs)
foreach(_avfoundation_lib IN LISTS _avfoundation_frameworks)
  string(TOUPPER "AVFoundation_LIBRARY_${_avfoundation_lib}" _avfoundation_lib_var)
  list(APPEND _avfoundation_required_vars ${_avfoundation_lib_var})

  find_library(${_avfoundation_lib_var} "${_avfoundation_lib}")
  mark_as_advanced(${_avfoundation_lib_var})
  if(${_avfoundation_lib_var})
    list(APPEND _avfoundation_libs "${${_avfoundation_lib_var}}")
  endif()
endforeach()
unset(_avfoundation_lib)
unset(_avfoundation_lib_var)
unset(_avfoundation_frameworks)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(AVFoundation
  REQUIRED_VARS
    ${_avfoundation_required_vars}
)

if(AVFoundation_FOUND)
  set(AVFoundation_LIBRARIES ${_avfoundation_libs})

  # IMPORTED, so the target never has to belong to an export set when a
  # library that links it is installed.
  if(NOT TARGET AVFoundation::AVFoundation)
    add_library(AVFoundation::AVFoundation INTERFACE IMPORTED GLOBAL)
    set_target_properties(AVFoundation::AVFoundation PROPERTIES
      INTERFACE_LINK_LIBRARIES "${AVFoundation_LIBRARIES}"
    )
  endif()
endif()

unset(_avfoundation_libs)
unset(_avfoundation_required_vars)
