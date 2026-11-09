include(Pj/PackageRoot)
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pj_pkg_search_module(PC_Resample QUIET resample libresample)
  set(Resample_VERSION ${PC_Resample_VERSION})
endif()

find_path(Resample_INCLUDE_DIR
  NAMES
    "resamplesubs.h"
  HINTS
    ${PC_Resample_INCLUDEDIR}
    ${PC_Resample_INCLUDE_DIRS}
  PATH_SUFFIXES
    resample
)
mark_as_advanced(Resample_INCLUDE_DIR)

find_library(Resample_LIBRARY
  NAMES
    resample
    libresample
  HINTS
    ${PC_Resample_LIBDIR}
    ${PC_Resample_LIBRARY_DIRS}
)
mark_as_advanced(Resample_LIBRARY)

if(Resample_INCLUDE_DIR AND Resample_LIBRARY)
  set(_resample_check_libs "${Resample_LIBRARY}")
  if(Resample_LIBRARY MATCHES "\\${CMAKE_STATIC_LIBRARY_SUFFIX}$" AND UNIX AND NOT APPLE)
    # A static libresample needs libm
    list(APPEND _resample_check_libs m)
  endif()

  include(Pj/CheckLibraryUsable)
  pj_check_library_usable(Resample
    FUNCTION res_Resample
    LIBRARIES ${_resample_check_libs}
  )
  unset(_resample_check_libs)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Resample
  REQUIRED_VARS
    Resample_INCLUDE_DIR
    Resample_LIBRARY
    Resample_USABLE
  VERSION_VAR
    Resample_VERSION
)

if(Resample_FOUND)
  set(Resample_LIBRARIES ${Resample_LIBRARY})
  set(Resample_INCLUDE_DIRS ${Resample_INCLUDE_DIR})
  set(Resample_DEFINITIONS ${PC_Resample_CFLAGS_OTHER})

  if(NOT TARGET Resample::Resample)
    add_library(Resample::Resample UNKNOWN IMPORTED)
    set_target_properties(Resample::Resample PROPERTIES
      IMPORTED_LOCATION "${Resample_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${Resample_INCLUDE_DIRS}"
      INTERFACE_COMPILE_OPTIONS "${Resample_DEFINITIONS}"
    )
    include(Pj/DebugLibrary)
    pj_import_debug_library(Resample::Resample Resample_LIBRARY)
  endif()
endif()
