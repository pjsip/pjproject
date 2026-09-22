# Android MediaCodec, the NDK's hardware codec API
#
# Defines the imported target `MediaNDK::MediaNDK`. The codec wrappers log
# through <android/log.h>, so liblog belongs to the backend rather than being
# incidental.
#
# The library is part of the NDK sysroot, so this only ever succeeds when
# cross-compiling for Android.

set(_mediandk_libs_wanted mediandk log)

set(_mediandk_libs)
set(_mediandk_required_vars)
foreach(_mediandk_lib IN LISTS _mediandk_libs_wanted)
  string(TOUPPER "MediaNDK_LIBRARY_${_mediandk_lib}" _mediandk_lib_var)
  list(APPEND _mediandk_required_vars ${_mediandk_lib_var})

  find_library(${_mediandk_lib_var} "${_mediandk_lib}")
  mark_as_advanced(${_mediandk_lib_var})
  if(${_mediandk_lib_var})
    list(APPEND _mediandk_libs "${${_mediandk_lib_var}}")
  endif()
endforeach()
unset(_mediandk_lib)
unset(_mediandk_lib_var)
unset(_mediandk_libs_wanted)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MediaNDK
  REQUIRED_VARS
    ${_mediandk_required_vars}
)

if(MediaNDK_FOUND)
  set(MediaNDK_LIBRARIES ${_mediandk_libs})

  # IMPORTED, so the target never has to belong to an export set when a
  # library that links it is installed.
  if(NOT TARGET MediaNDK::MediaNDK)
    add_library(MediaNDK::MediaNDK INTERFACE IMPORTED GLOBAL)
    set_target_properties(MediaNDK::MediaNDK PROPERTIES
      INTERFACE_LINK_LIBRARIES "${MediaNDK_LIBRARIES}"
    )

    # The wrappers call AMediaCodec entry points that appeared after the
    # minimum supported API level and check for them at run time. Without
    # these the references bind strongly and the library fails to load on
    # older devices; -Werror=unguarded-availability catches a call that was
    # not guarded. Matches what aconfigure.ac puts in ac_mediacodec_cflags.
    set_property(TARGET MediaNDK::MediaNDK APPEND PROPERTY
      INTERFACE_COMPILE_OPTIONS
        "-D__ANDROID_UNAVAILABLE_SYMBOLS_ARE_WEAK__"
        "-Werror=unguarded-availability"
    )
  endif()
endif()

unset(_mediandk_libs)
unset(_mediandk_required_vars)
