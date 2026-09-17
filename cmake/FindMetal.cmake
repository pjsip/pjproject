# Metal video render backend (macOS and iOS)
#
# Defines the imported target `Metal::Metal` when every framework is found.

set(_metal_libs)
foreach(_metal_lib IN ITEMS Metal MetalKit)
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
