set(_coreaudio_libs)
foreach(_coreaudio_lib IN ITEMS CoreAudio AudioToolbox Foundation AppKit)
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
