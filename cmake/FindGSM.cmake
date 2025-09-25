find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_search_module(PC_GSM QUIET gsm libgsm)
  set(GSM_VERSION ${PC_GSM_VERSION})
endif()

find_path(GSM_INCLUDE_DIR
  NAMES
    "gsm.h"
  HINTS
    ${PC_GSM_INCLUDEDIR}
    ${PC_GSM_INCLUDE_DIRS}
  PATH_SUFFIXES
    gsm
)
mark_as_advanced(GSM_INCLUDE_DIR)

find_library(GSM_LIBRARY
  NAMES
    gsm
    libgsm
  HINTS
    ${PC_GSM_LIBDIR}
    ${PC_GSM_LIBRARY_DIRS}
)
mark_as_advanced(GSM_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GSM
  REQUIRED_VARS
    GSM_INCLUDE_DIR
    GSM_LIBRARY
  VERSION_VAR
    GSM_VERSION
)

if(GSM_FOUND)
  set(GSM_LIBRARIES ${GSM_LIBRARY})
  set(GSM_INCLUDE_DIRS ${GSM_INCLUDE_DIR})
  set(GSM_DEFINITIONS ${PC_GSM_CFLAGS_OTHER})

  if(NOT TARGET GSM::GSM)
    add_library(GSM::GSM UNKNOWN IMPORTED)
    set_target_properties(GSM::GSM PROPERTIES
      IMPORTED_LOCATION "${GSM_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${GSM_INCLUDE_DIRS}"
      INTERFACE_COMPILE_OPTIONS "${GSM_DEFINITIONS}"
    )
  endif()
endif()
