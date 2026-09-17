# AVFoundation video capture backend (macOS and iOS)
#
# Defines the imported target `AVFoundation::AVFoundation` when every framework is found.

set(_avfoundation_libs)
foreach(_avfoundation_lib IN ITEMS AVFoundation CoreMedia CoreVideo CoreGraphics QuartzCore)
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

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(AVFoundation
  REQUIRED_VARS
    ${_avfoundation_required_vars}
)

if(AVFoundation_FOUND)
  set(AVFoundation_LIBRARIES ${_avfoundation_libs})

  if(NOT TARGET AVFoundation::AVFoundation)
    add_library(avfoundation INTERFACE)
    set_target_properties(avfoundation PROPERTIES
      INTERFACE_LINK_LIBRARIES "${AVFoundation_LIBRARIES}"
    )
    add_library(AVFoundation::AVFoundation ALIAS avfoundation)
  endif()
endif()

unset(_avfoundation_libs)
unset(_avfoundation_required_vars)
