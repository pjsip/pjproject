include(Pj/PackageRoot)
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pj_pkg_search_module(PC_SampleRate QUIET samplerate libsamplerate)
  set(SampleRate_VERSION ${PC_SampleRate_VERSION})
endif()

find_path(SampleRate_INCLUDE_DIR
  NAMES
    "samplerate.h"
  HINTS
    ${PC_SampleRate_INCLUDEDIR}
    ${PC_SampleRate_INCLUDE_DIRS}
)
mark_as_advanced(SampleRate_INCLUDE_DIR)

find_library(SampleRate_LIBRARY
  NAMES
    samplerate
    libsamplerate
  HINTS
    ${PC_SampleRate_LIBDIR}
    ${PC_SampleRate_LIBRARY_DIRS}
)
mark_as_advanced(SampleRate_LIBRARY)

if(SampleRate_INCLUDE_DIR AND SampleRate_LIBRARY)
  set(_samplerate_check_libs "${SampleRate_LIBRARY}")
  if(SampleRate_LIBRARY MATCHES "\\${CMAKE_STATIC_LIBRARY_SUFFIX}$" AND UNIX AND NOT APPLE)
    # A static libsamplerate needs libm
    list(APPEND _samplerate_check_libs m)
  endif()

  include(Pj/CheckLibraryUsable)
  pj_check_library_usable(SampleRate
    FUNCTION src_new
    LIBRARIES ${_samplerate_check_libs}
  )
  unset(_samplerate_check_libs)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SampleRate
  REQUIRED_VARS
    SampleRate_LIBRARY
    SampleRate_INCLUDE_DIR
    SampleRate_USABLE
  VERSION_VAR
    SampleRate_VERSION
)

if(SampleRate_FOUND)
  set(SampleRate_LIBRARIES "${SampleRate_LIBRARY}")
  set(SampleRate_INCLUDE_DIRS "${SampleRate_INCLUDE_DIR}")
  set(SampleRate_DEFINITIONS "${PC_SampleRate_CFLAGS_OTHER}")

  if(NOT TARGET SampleRate::SampleRate)
    add_library(SampleRate::SampleRate UNKNOWN IMPORTED)
    set_target_properties(SampleRate::SampleRate PROPERTIES
      IMPORTED_LOCATION "${SampleRate_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${SampleRate_INCLUDE_DIRS}"
      INTERFACE_COMPILE_OPTIONS "${SampleRate_DEFINITIONS}"
    )
    include(Pj/DebugLibrary)
    pj_import_debug_library(SampleRate::SampleRate SampleRate_LIBRARY)
  endif()
endif()
