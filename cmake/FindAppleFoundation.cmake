# Frameworks every Apple target needs: Foundation and the platform UI framework
#
# Defines the imported target `AppleFoundation::AppleFoundation` when every framework is found.
# The framework list follows the one the autotools build links (aconfigure.ac).

set(_applefoundation_frameworks Foundation CoreFoundation)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  list(APPEND _applefoundation_frameworks AppKit)
else()
  list(APPEND _applefoundation_frameworks UIKit)
endif()

set(_applefoundation_libs)
foreach(_applefoundation_lib IN LISTS _applefoundation_frameworks)
  string(TOUPPER "AppleFoundation_LIBRARY_${_applefoundation_lib}" _applefoundation_lib_var)
  list(APPEND _applefoundation_required_vars ${_applefoundation_lib_var})

  find_library(${_applefoundation_lib_var} "${_applefoundation_lib}")
  mark_as_advanced(${_applefoundation_lib_var})
  if(${_applefoundation_lib_var})
    list(APPEND _applefoundation_libs "${${_applefoundation_lib_var}}")
  endif()
endforeach()
unset(_applefoundation_lib)
unset(_applefoundation_lib_var)
unset(_applefoundation_frameworks)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(AppleFoundation
  REQUIRED_VARS
    ${_applefoundation_required_vars}
)

if(AppleFoundation_FOUND)
  set(AppleFoundation_LIBRARIES ${_applefoundation_libs})

  # IMPORTED, so the target never has to belong to an export set when a
  # library that links it is installed.
  if(NOT TARGET AppleFoundation::AppleFoundation)
    add_library(AppleFoundation::AppleFoundation INTERFACE IMPORTED GLOBAL)
    set_target_properties(AppleFoundation::AppleFoundation PROPERTIES
      INTERFACE_LINK_LIBRARIES "${AppleFoundation_LIBRARIES}"
    )
  endif()
endif()

unset(_applefoundation_libs)
unset(_applefoundation_required_vars)
