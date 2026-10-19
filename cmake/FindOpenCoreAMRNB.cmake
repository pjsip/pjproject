find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_OpenCoreAMRNB QUIET opencore-amrnb)
  set(OpenCoreAMRNB_VERSION ${PC_OpenCoreAMRNB_VERSION})
endif()

find_path(OpenCoreAMRNB_INCLUDE_DIR
  NAMES
    "opencore-amrnb/interf_enc.h"
  HINTS
    ${PC_OpenCoreAMRNB_INCLUDEDIR}
    ${PC_OpenCoreAMRNB_INCLUDE_DIRS}
)
mark_as_advanced(OpenCoreAMRNB_INCLUDE_DIR)

find_library(OpenCoreAMRNB_LIBRARY
  NAMES
    opencore-amrnb
  HINTS
    ${PC_OpenCoreAMRNB_LIBDIRS}
    ${PC_OpenCoreAMRNB_LIBRARY_DIRS}
)
mark_as_advanced(OpenCoreAMRNB_LIBRARY)

if(OpenCoreAMRNB_INCLUDE_DIR AND OpenCoreAMRNB_LIBRARY)
  set(_amrnb_check_libs "${OpenCoreAMRNB_LIBRARY}")
  if(OpenCoreAMRNB_LIBRARY MATCHES "\\${CMAKE_STATIC_LIBRARY_SUFFIX}$" AND UNIX AND NOT APPLE)
    # A static libopencore-amrnb needs libm
    list(APPEND _amrnb_check_libs m)
  endif()

  include(Pj/CheckLibraryUsable)
  pj_check_library_usable(OpenCoreAMRNB
    FUNCTION Encoder_Interface_init
    LIBRARIES ${_amrnb_check_libs}
  )
  unset(_amrnb_check_libs)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenCoreAMRNB
  REQUIRED_VARS
    OpenCoreAMRNB_LIBRARY
    OpenCoreAMRNB_INCLUDE_DIR
    OpenCoreAMRNB_USABLE
  VERSION_VAR
    OpenCoreAMRNB_VERSION
)

if(OpenCoreAMRNB_FOUND)
  set(OpenCoreAMRNB_LIBRARIES ${OpenCoreAMRNB_LIBRARY})
  set(OpenCoreAMRNB_INCLUDE_DIRS ${OpenCoreAMRNB_INCLUDE_DIR})
  set(OpenCoreAMRNB_DEFINITIONS ${PC_OpenCoreAMRNB_CFLAGS_OTHER})

  if(NOT TARGET OpenCoreAMRNB::OpenCoreAMRNB)
    add_library(OpenCoreAMRNB::OpenCoreAMRNB UNKNOWN IMPORTED)
    set_target_properties(OpenCoreAMRNB::OpenCoreAMRNB PROPERTIES
      IMPORTED_LOCATION "${OpenCoreAMRNB_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${OpenCoreAMRNB_INCLUDE_DIRS}"
      INTERFACE_COMPILE_OPTIONS "${OpenCoreAMRNB_DEFINITIONS}"
    )
  endif()
endif()
