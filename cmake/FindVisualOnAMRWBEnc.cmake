find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_search_module(PC_VisualOnAMRWBEnc QUIET vo-amrwbenc libvo-amrwbenc)
  set(VisualOnAMRWBEnc_VERSION ${PC_VisualOnAMRWBEnc_VERSION})
endif()

find_path(VisualOnAMRWBEnc_INCLUDE_DIR
  NAMES
    "vo-amrwbenc/enc_if.h"
  HINTS
    ${PC_VisualOnAMRWBEnc_INCLUDEDIR}
    ${PC_VisualOnAMRWBEnc_INCLUDE_DIRS}
  PATH_SUFFIXES
    include
)
mark_as_advanced(VisualOnAMRWBEnc_INCLUDE_DIR)

find_library(VisualOnAMRWBEnc_LIBRARY
  NAMES
    vo-amrwbenc
    libvo-amrwbenc
  HINTS
    ${PC_VisualOnAMRWBEnc_LIBDIR}
    ${PC_VisualOnAMRWBEnc_LIBRARY_DIRS}
  PATH_SUFFIXES
    lib
)
mark_as_advanced(VisualOnAMRWBEnc_LIBRARY)

if(VisualOnAMRWBEnc_INCLUDE_DIR AND VisualOnAMRWBEnc_LIBRARY)
  set(_voamrwbenc_check_libs "${VisualOnAMRWBEnc_LIBRARY}")
  if(VisualOnAMRWBEnc_LIBRARY MATCHES "\\${CMAKE_STATIC_LIBRARY_SUFFIX}$" AND UNIX AND NOT APPLE)
    # A static libvo-amrwbenc needs libm
    list(APPEND _voamrwbenc_check_libs m)
  endif()

  include(Pj/CheckLibraryUsable)
  pj_check_library_usable(VisualOnAMRWBEnc
    FUNCTION E_IF_init
    LIBRARIES ${_voamrwbenc_check_libs}
  )
  unset(_voamrwbenc_check_libs)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VisualOnAMRWBEnc
  REQUIRED_VARS
    VisualOnAMRWBEnc_INCLUDE_DIR
    VisualOnAMRWBEnc_LIBRARY
    VisualOnAMRWBEnc_USABLE
  VERSION_VAR
    VisualOnAMRWBEnc_VERSION
)

if(VisualOnAMRWBEnc_FOUND)
  set(VisualOnAMRWBEnc_LIBRARIES ${VisualOnAMRWBEnc_LIBRARY})
  set(VisualOnAMRWBEnc_INCLUDE_DIRS ${VisualOnAMRWBEnc_INCLUDE_DIR})
  set(VisualOnAMRWBEnc_DEFINITIONS ${PC_VisualOnAMRWBEnc_CFLAGS_OTHER})

  if(NOT TARGET VisualOnAMRWBEnc::VisualOnAMRWBEnc)
    add_library(VisualOnAMRWBEnc::VisualOnAMRWBEnc UNKNOWN IMPORTED)
    set_target_properties(VisualOnAMRWBEnc::VisualOnAMRWBEnc PROPERTIES
      IMPORTED_LOCATION "${VisualOnAMRWBEnc_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${VisualOnAMRWBEnc_INCLUDE_DIRS}"
      INTERFACE_COMPILE_OPTIONS "${VisualOnAMRWBEnc_DEFINITIONS}"
    )
  endif()
endif()
