find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_OpenCoreAMRWB QUIET opencore-amrwb)
  set(OpenCoreAMRWB_VERSION ${PC_OpenCoreAMRWB_VERSION})
endif()

find_path(OpenCoreAMRWB_INCLUDE_DIR
  NAMES
    "opencore-amrwb/dec_if.h"
  HINTS
    ${PC_OpenCoreAMRWB_INCLUDEDIR}
    ${PC_OpenCoreAMRWB_INCLUDE_DIRS}
)
mark_as_advanced(OpenCoreAMRWB_INCLUDE_DIR)

find_library(OpenCoreAMRWB_LIBRARY
  NAMES
    opencore-amrwb
  HINTS
    ${PC_OpenCoreAMRWB_LIBDIR}
    ${PC_OpenCoreAMRWB_LIBRARY_DIRS}
)
mark_as_advanced(OpenCoreAMRWB_LIBRARY)

if(OpenCoreAMRWB_INCLUDE_DIR AND OpenCoreAMRWB_LIBRARY)
  set(_amrwb_check_libs "${OpenCoreAMRWB_LIBRARY}")
  if(OpenCoreAMRWB_LIBRARY MATCHES "\\${CMAKE_STATIC_LIBRARY_SUFFIX}$" AND UNIX AND NOT APPLE)
    # A static libopencore-amrwb needs libm
    list(APPEND _amrwb_check_libs m)
  endif()

  include(Pj/CheckLibraryUsable)
  pj_check_library_usable(OpenCoreAMRWB
    FUNCTION D_IF_init
    LIBRARIES ${_amrwb_check_libs}
  )
  unset(_amrwb_check_libs)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenCoreAMRWB
  REQUIRED_VARS
    OpenCoreAMRWB_LIBRARY
    OpenCoreAMRWB_INCLUDE_DIR
    OpenCoreAMRWB_USABLE
  VERSION_VAR
    OpenCoreAMRWB_VERSION
)

if(OpenCoreAMRWB_FOUND)
  set(OpenCoreAMRWB_LIBRARIES ${OpenCoreAMRWB_LIBRARY})
  set(OpenCoreAMRWB_INCLUDE_DIRS ${OpenCoreAMRWB_INCLUDE_DIR})
  set(OpenCoreAMRWB_DEFINITIONS ${PC_OpenCoreAMRWB_CFLAGS_OTHER})

  if(NOT TARGET OpenCoreAMRWB::OpenCoreAMRWB)
    add_library(OpenCoreAMRWB::OpenCoreAMRWB UNKNOWN IMPORTED)
    set_target_properties(OpenCoreAMRWB::OpenCoreAMRWB PROPERTIES
      IMPORTED_LOCATION "${OpenCoreAMRWB_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${OpenCoreAMRWB_INCLUDE_DIRS}"
      INTERFACE_COMPILE_OPTIONS "${OpenCoreAMRWB_DEFINITIONS}"
    )
  endif()
endif()
