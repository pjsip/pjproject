# Metal video render backend (macOS and iOS)
#
# Defines the imported target `Metal::Metal` when every framework is found.
# The framework list follows the one the autotools build links (aconfigure.ac).

set(_metal_frameworks Metal MetalKit Foundation)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  list(APPEND _metal_frameworks AppKit)
else()
  list(APPEND _metal_frameworks UIKit)
endif()

set(_metal_libs)
foreach(_metal_lib IN LISTS _metal_frameworks)
  string(TOUPPER "Metal_LIBRARY_${_metal_lib}" _metal_lib_var)
  list(APPEND _metal_required_vars ${_metal_lib_var})

  find_library(${_metal_lib_var} "${_metal_lib}")
  mark_as_advanced(${_metal_lib_var})
  if(${_metal_lib_var})
    list(APPEND _metal_libs "${${_metal_lib_var}}")
  endif()
endforeach()
unset(_metal_lib)
unset(_metal_lib_var)
unset(_metal_frameworks)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Metal
  REQUIRED_VARS
    ${_metal_required_vars}
)

if(Metal_FOUND)
  set(Metal_LIBRARIES ${_metal_libs})

  if(NOT TARGET Metal::Metal)
    add_library(metal INTERFACE)
    set_target_properties(metal PROPERTIES
      INTERFACE_LINK_LIBRARIES "${Metal_LIBRARIES}"
    )
    add_library(Metal::Metal ALIAS metal)
  endif()
endif()

unset(_metal_libs)
unset(_metal_required_vars)
