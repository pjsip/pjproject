find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_search_module(PC_YUV QUIET libyuv yuv)
  set(YUV_VERSION ${PC_YUV_VERSION})
endif()

find_path(YUV_INCLUDE_DIR
  NAMES
    "libyuv.h"
  HINTS
    ${PC_YUV_INCLUDEDIR}
    ${PC_YUV_INCLUDE_DIRS}
)

find_library(YUV_LIBRARY
  NAMES
    libyuv
    yuv
  HINTS
    ${PC_YUV_LIBDIR}
    ${PC_YUV_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(YUV
  REQUIRED_VARS
    YUV_LIBRARY
    YUV_INCLUDE_DIR
  VERSION_VAR
    YUV_VERSION
)

if(YUV_FOUND)
  set(YUV_LIBRARIES ${YUV_LIBRARY})
  set(YUV_INCLUDE_DIRS ${YUV_INCLUDE_DIR})
  set(YUV_DEFINITIONS ${PC_YUV_CFLAGS_OTHER})

  if(NOT TARGET YUV::YUV)
    add_library(YUV::YUV UNKNOWN IMPORTED)
    set_target_properties(YUV::YUV PROPERTIES
      IMPORTED_LOCATION "${YUV_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${YUV_INCLUDE_DIRS}"
      INTERFACE_COMPILE_OPTIONS "${YUV_DEFINITIONS}"
    )
  endif()
endif()

mark_as_advanced(YUV_INCLUDE_DIR YUV_LIBRARY)
