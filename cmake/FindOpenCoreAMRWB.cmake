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

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenCoreAMRWB
  REQUIRED_VARS
    OpenCoreAMRWB_LIBRARY
    OpenCoreAMRWB_INCLUDE_DIR
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
