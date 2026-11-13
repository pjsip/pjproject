include(Pj/PackageRoot)
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pj_pkg_search_module(PC_Speex QUIET speex libspeex)
endif()

find_path(Speex_INCLUDE_DIR
  NAMES
    "speex/speex.h"
  HINTS
    ${PC_Speex_INCLUDEDIR}
    ${PC_Speex_INCLUDE_DIRS}
)
mark_as_advanced(Speex_INCLUDE_DIR)

find_library(Speex_LIBRARY
  NAMES
    speex
    libspeex
  HINTS
    ${PC_Speex_LIBDIR}
    ${PC_Speex_LIBRARY_DIRS}
)
mark_as_advanced(Speex_LIBRARY)

if(Speex_INCLUDE_DIR AND Speex_LIBRARY)
  include(Pj/CheckLibraryUsable)
  pj_check_library_usable(Speex
    FUNCTION speex_encoder_init
    LIBRARIES "${Speex_LIBRARY}"
    STATIC_LIBS m
  )
endif()

# Only pkg-config gives the version: speex.h has none, its
# SPEEX_LIB_GET_*_VERSION are speex_lib_ctl() request codes
if(DEFINED PC_Speex_VERSION AND NOT PC_Speex_VERSION STREQUAL "")
  set(Speex_VERSION "${PC_Speex_VERSION}")
endif()
mark_as_advanced(Speex_VERSION)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Speex
  REQUIRED_VARS
    Speex_INCLUDE_DIR
    Speex_LIBRARY
    Speex_USABLE
  VERSION_VAR
    Speex_VERSION
)

if(Speex_FOUND)
  set(Speex_INCLUDE_DIRS ${Speex_INCLUDE_DIR})
  set(Speex_LIBRARIES ${Speex_LIBRARY})
  set(Speex_DEFINITIONS ${PC_Speex_CFLAGS_OTHER})

  if(NOT TARGET Speex::Speex)
    add_library(Speex::Speex UNKNOWN IMPORTED)
    set_target_properties(Speex::Speex PROPERTIES
      IMPORTED_LOCATION "${Speex_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${Speex_INCLUDE_DIRS}"
      INTERFACE_COMPILE_OPTIONS "${Speex_DEFINITIONS}"
    )
    include(Pj/DebugLibrary)
    pj_import_debug_library(Speex::Speex Speex_LIBRARY)
  endif()
endif()
