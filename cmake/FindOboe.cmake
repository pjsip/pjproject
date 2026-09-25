# Google Oboe, the low-latency Android audio library
#
# Defines the imported target `Oboe::Oboe`. Oboe is not bundled, so point the
# build at an unpacked oboe-<version>.aar:
#
#   -DOboe_ROOT=/path/to/oboe-1.9.0
#
# The AAR lays its native side out as a Prefab module, the same layout
# `./aconfigure --with-oboe=<prefix>` expects:
#
#   <root>/prefab/modules/oboe/include
#   <root>/prefab/modules/oboe/libs/android.<abi>/liboboe.{a,so}
#
# An Oboe built and installed from source works too. Its own install rules
# write include/ and lib/<abi>/, and a plain include/ + lib/ prefix is
# accepted as well.
#
# Note that the AAR is a Prefab module and carries liboboe.so nowhere else, so
# depending on it from Maven alone does not put the library into an
# application: a consumer either builds against it through Prefab, or links a
# static Oboe built from source.
#
# Oboe drives OpenSL ES on older devices and logs through <android/log.h>, so
# both NDK libraries are part of the backend rather than incidental.

if(NOT ANDROID)
  set(Oboe_FOUND FALSE)
  return()
endif()

set(_oboe_module_dir "prefab/modules/oboe")

find_path(Oboe_INCLUDE_DIR
  NAMES
    "oboe/Oboe.h"
  PATH_SUFFIXES
    "${_oboe_module_dir}/include"
    include
  # Oboe lives outside the NDK sysroot that the toolchain file confines
  # find_path to, so the root path filter has to be lifted here.
  NO_CMAKE_FIND_ROOT_PATH
)
mark_as_advanced(Oboe_INCLUDE_DIR)

find_library(Oboe_LIBRARY
  NAMES
    oboe
  PATH_SUFFIXES
    "${_oboe_module_dir}/libs/android.${CMAKE_ANDROID_ARCH_ABI}"
    "lib/${CMAKE_ANDROID_ARCH_ABI}"
    lib
  NO_CMAKE_FIND_ROOT_PATH
)
mark_as_advanced(Oboe_LIBRARY)

unset(_oboe_module_dir)

# NDK libraries Oboe itself needs; these are in the sysroot, so they are found
# the ordinary way.
find_library(Oboe_LIBRARY_OpenSLES OpenSLES)
find_library(Oboe_LIBRARY_log log)
mark_as_advanced(Oboe_LIBRARY_OpenSLES Oboe_LIBRARY_log)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Oboe
  REQUIRED_VARS
    Oboe_INCLUDE_DIR
    Oboe_LIBRARY
    Oboe_LIBRARY_OpenSLES
    Oboe_LIBRARY_log
)

if(Oboe_FOUND)
  set(Oboe_INCLUDE_DIRS ${Oboe_INCLUDE_DIR})
  set(Oboe_LIBRARIES
    ${Oboe_LIBRARY}
    ${Oboe_LIBRARY_OpenSLES}
    ${Oboe_LIBRARY_log}
  )

  if(NOT TARGET Oboe::Oboe)
    add_library(Oboe::Oboe UNKNOWN IMPORTED)
    set_target_properties(Oboe::Oboe PROPERTIES
      IMPORTED_LOCATION "${Oboe_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${Oboe_INCLUDE_DIRS}"
      INTERFACE_LINK_LIBRARIES
        "${Oboe_LIBRARY_OpenSLES};${Oboe_LIBRARY_log}"
    )
  endif()
endif()
