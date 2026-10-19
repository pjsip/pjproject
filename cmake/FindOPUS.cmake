find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_OPUS QUIET opus)
  set(OPUS_VERSION ${PC_OPUS_VERSION})
endif()

find_path(OPUS_INCLUDE_DIR
  NAMES
    "opus/opus.h"
  HINTS
    ${PC_OPUS_INCLUDEDIR}
    ${PC_OPUS_INCLUDE_DIRS}
)
mark_as_advanced(OPUS_INCLUDE_DIR)

find_library(OPUS_LIBRARY
  NAMES
    opus
  HINTS
    ${PC_OPUS_LIBDIR}
    ${PC_OPUS_LIBRARY_DIRS}
)
mark_as_advanced(OPUS_LIBRARY)

if(OPUS_INCLUDE_DIR AND OPUS_LIBRARY)
  set(_opus_check_libs "${OPUS_LIBRARY}")
  if(OPUS_LIBRARY MATCHES "\\${CMAKE_STATIC_LIBRARY_SUFFIX}$" AND UNIX AND NOT APPLE)
    # A static libopus needs libm
    list(APPEND _opus_check_libs m)
  endif()

  include(Pj/CheckLibraryUsable)
  pj_check_library_usable(OPUS
    FUNCTION opus_repacketizer_get_size
    LIBRARIES ${_opus_check_libs}
  )
  unset(_opus_check_libs)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OPUS
  REQUIRED_VARS
    OPUS_LIBRARY
    OPUS_INCLUDE_DIR
    OPUS_USABLE
  VERSION_VAR
    OPUS_VERSION
)

if(OPUS_FOUND)
  set(OPUS_LIBRARIES "${OPUS_LIBRARY}")
  set(OPUS_INCLUDE_DIRS "${OPUS_INCLUDE_DIR}")
  set(OPUS_DEFINITIONS "${PC_OPUS_CFLAGS_OTHER}")

  if(NOT TARGET OPUS::OPUS)
    add_library(OPUS::OPUS UNKNOWN IMPORTED)
    set_target_properties(OPUS::OPUS PROPERTIES
      IMPORTED_LOCATION "${OPUS_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${OPUS_INCLUDE_DIRS}"
      INTERFACE_COMPILE_OPTIONS "${OPUS_DEFINITIONS}"
    )
  endif()
endif()

