find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_search_module(PC_SpeexDSP QUIET speexdsp libspeexdsp)
endif()

find_path(SpeexDSP_INCLUDE_DIR
  NAMES
    "speex/speexdsp_types.h"
  HINTS
    ${PC_SpeexDSP_INCLUDEDIR}
    ${PC_SpeexDSP_INCLUDE_DIRS}
)
mark_as_advanced(SpeexDSP_INCLUDE_DIR)

find_library(SpeexDSP_LIBRARY
  NAMES
    speexdsp
    libspeexdsp
  HINTS
    ${PC_SpeexDSP_LIBDIR}
    ${PC_SpeexDSP_LIBRARY_DIRS}
)
mark_as_advanced(SpeexDSP_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SpeexDSP
  REQUIRED_VARS
    SpeexDSP_INCLUDE_DIR
    SpeexDSP_LIBRARY
  VERSION_VAR
    SpeexDSP_VERSION
)

if(SpeexDSP_FOUND)
  set(SpeexDSP_INCLUDE_DIRS ${SpeexDSP_INCLUDE_DIR})
  set(SpeexDSP_LIBRARIES ${SpeexDSP_LIBRARY})
  set(SpeexDSP_DEFINITIONS ${PC_SpeexDSP_CFLAGS_OTHER})

  if(NOT TARGET SpeexDSP::SpeexDSP)
    add_library(SpeexDSP::SpeexDSP UNKNOWN IMPORTED)
    set_target_properties(SpeexDSP::SpeexDSP PROPERTIES
      IMPORTED_LOCATION "${SpeexDSP_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${SpeexDSP_INCLUDE_DIRS}"
      INTERFACE_COMPILE_OPTIONS "${SpeexDSP_DEFINITIONS}"
    )
  endif()
endif()
