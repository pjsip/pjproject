# AudioToolbox, used by ilbc.c through PJMEDIA_ILBC_CODEC_USE_COREAUDIO
#
# Defines the imported target `AudioToolbox::AudioToolbox` when every framework is found.
# The framework list follows the one the autotools build links (aconfigure.ac).

set(_audiotoolbox_frameworks AudioToolbox)
set(_audiotoolbox_libs)
foreach(_audiotoolbox_lib IN LISTS _audiotoolbox_frameworks)
  string(TOUPPER "AudioToolbox_LIBRARY_${_audiotoolbox_lib}" _audiotoolbox_lib_var)
  list(APPEND _audiotoolbox_required_vars ${_audiotoolbox_lib_var})

  find_library(${_audiotoolbox_lib_var} "${_audiotoolbox_lib}")
  mark_as_advanced(${_audiotoolbox_lib_var})
  if(${_audiotoolbox_lib_var})
    list(APPEND _audiotoolbox_libs "${${_audiotoolbox_lib_var}}")
  endif()
endforeach()
unset(_audiotoolbox_lib)
unset(_audiotoolbox_lib_var)
unset(_audiotoolbox_frameworks)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(AudioToolbox
  REQUIRED_VARS
    ${_audiotoolbox_required_vars}
)

if(AudioToolbox_FOUND)
  set(AudioToolbox_LIBRARIES ${_audiotoolbox_libs})

  # IMPORTED, so the target never has to belong to an export set when a
  # library that links it is installed.
  if(NOT TARGET AudioToolbox::AudioToolbox)
    add_library(AudioToolbox::AudioToolbox INTERFACE IMPORTED GLOBAL)
    set_target_properties(AudioToolbox::AudioToolbox PROPERTIES
      INTERFACE_LINK_LIBRARIES "${AudioToolbox_LIBRARIES}"
    )
  endif()
endif()

unset(_audiotoolbox_libs)
unset(_audiotoolbox_required_vars)
