include(Pj/PackageRoot)
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pj_pkg_search_module(PC_V4L2 QUIET libv4l2 v4l2)
  set(V4L2_VERSION ${PC_V4L2_VERSION})
endif()

find_path(V4L2_INCLUDE_DIR
  NAMES
    "libv4l2.h"
  HINTS
    ${PC_V4L2_INCLUDEDIR}
    ${PC_V4L2_INCLUDE_DIRS}
  PATH_SUFFIXES
    gsm
)
mark_as_advanced(V4L2_INCLUDE_DIR)

find_library(V4L2_LIBRARY
  NAMES
    libv4l2
    v4l2
  HINTS
    ${PC_V4L2_LIBDIR}
    ${PC_V4L2_LIBRARY_DIRS}
    ${V4L2_ROOT}
)
mark_as_advanced(V4L2_LIBRARY)

if(V4L2_INCLUDE_DIR AND V4L2_LIBRARY)
  set(_v4l2_check_libs "${V4L2_LIBRARY}")
  if(V4L2_LIBRARY MATCHES "\\${CMAKE_STATIC_LIBRARY_SUFFIX}$" AND UNIX)
    # A static libv4l2 needs pthread
    find_package(Threads QUIET)
    if(TARGET Threads::Threads)
      list(APPEND _v4l2_check_libs Threads::Threads)
    endif()
  endif()

  include(Pj/CheckLibraryUsable)
  pj_check_library_usable(V4L2
    FUNCTION v4l2_open
    LIBRARIES ${_v4l2_check_libs}
  )
  unset(_v4l2_check_libs)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(V4L2
  REQUIRED_VARS
    V4L2_INCLUDE_DIR
    V4L2_LIBRARY
    V4L2_USABLE
)

if(V4L2_FOUND)
  set(V4L2_LIBRARIES ${V4L2_LIBRARY})
  set(V4L2_INCLUDE_DIRS ${V4L2_INCLUDE_DIR})
  set(V4L2_DEFINITIONS ${PC_V4L2_CFLAGS_OTHER})

  if(NOT TARGET V4L2::V4L2)
    add_library(V4L2::V4L2 UNKNOWN IMPORTED)
    set_target_properties(V4L2::V4L2 PROPERTIES
      IMPORTED_LOCATION "${V4L2_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${V4L2_INCLUDE_DIRS}"
      INTERFACE_COMPILE_OPTIONS "${V4L2_DEFINITIONS}"
    )
  endif()
endif()
