# VideoToolbox H.264 codec backend (macOS and iOS)
#
# Defines the imported target `VideoToolbox::VideoToolbox` when every framework is found.

set(_videotoolbox_libs)
foreach(_videotoolbox_lib IN ITEMS VideoToolbox CoreMedia CoreVideo)
  string(TOUPPER "VideoToolbox_LIBRARY_${_videotoolbox_lib}" _videotoolbox_lib_var)
  list(APPEND _videotoolbox_required_vars ${_videotoolbox_lib_var})

  find_library(${_videotoolbox_lib_var} "${_videotoolbox_lib}")
  mark_as_advanced(${_videotoolbox_lib_var})
  if(${_videotoolbox_lib_var})
    list(APPEND _videotoolbox_libs "${${_videotoolbox_lib_var}}")
  endif()
endforeach()
unset(_videotoolbox_lib)
unset(_videotoolbox_lib_var)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VideoToolbox
  REQUIRED_VARS
    ${_videotoolbox_required_vars}
)

if(VideoToolbox_FOUND)
  set(VideoToolbox_LIBRARIES ${_videotoolbox_libs})

  if(NOT TARGET VideoToolbox::VideoToolbox)
    add_library(videotoolbox INTERFACE)
    set_target_properties(videotoolbox PROPERTIES
      INTERFACE_LINK_LIBRARIES "${VideoToolbox_LIBRARIES}"
    )
    add_library(VideoToolbox::VideoToolbox ALIAS videotoolbox)
  endif()
endif()

unset(_videotoolbox_libs)
unset(_videotoolbox_required_vars)
