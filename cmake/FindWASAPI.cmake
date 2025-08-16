find_path(WASAPI_INCLUDE_DIR "audioclient.h")
mark_as_advanced(WASAPI_INCLUDE_DIR)

set(_wasapi_libs)
set(_wasapi_required_vars "WASAPI_INCLUDE_DIR")
foreach(_wasapi_lib IN ITEMS ksuser mfplat mfuuid wmcodecdspuuid)
  string(TOUPPER "WASAPI_LIBRARY_${_wasapi_lib}" _wasapi_lib_var)
  list(APPEND _wasapi_required_vars ${_wasapi_lib_var})

  find_library(${_wasapi_lib_var} "${_wasapi_lib}")
  mark_as_advanced(${_wasapi_lib_var})
  if(${_wasapi_lib_var})
    list(APPEND _wasapi_libs "${${_wasapi_lib_var}}")
  endif()
endforeach()
unset(_wasapi_lib)
unset(_wasapi_lib_var)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WASAPI
  REQUIRED_VARS
    ${_wasapi_required_vars}
)

if(WASAPI_FOUND)
  set(WASAPI_INCLUDE_DIRS ${WASAPI_INCLUDE_DIR})
  set(WASAPI_LIBRARIES ${_wasapi_libs})
  set(WASAPI_DEFINITIONS "__WINDOWS_WASAPI__")

  if(NOT TARGET WASAPI::WASAPI)
    add_library(wasapi INTERFACE)
    set_target_properties(wasapi PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${WASAPI_INCLUDE_DIRS}"
      INTERFACE_LINK_LIBRARIES "${WASAPI_LIBRARIES}"
      INTERFACE_COMPILE_OPTIONS "${WASAPI_DEFINITIONS}"
    )
    add_library(WASAPI::WASAPI ALIAS wasapi)
  endif()
endif()

unset(_wasapi_libs)
unset(_wasapi_required_vars)
