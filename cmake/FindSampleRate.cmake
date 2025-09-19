find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_search_module(PC_SampleRate QUIET samplerate libsamplerate)
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

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SampleRate
  REQUIRED_VARS
    SampleRate_LIBRARY
    SampleRate_INCLUDE_DIR
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
  endif()
endif()
