find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_OpenH264 QUIET openh264)
  set(OpenH264_VERSION ${PC_OpenH264_VERSION})
endif()

find_path(OpenH264_INCLUDE_DIR
  NAMES
    "wels/codec_api.h"
    "wels/codec_app_def.h"
    "wels/codec_def.h"
  HINTS
    ${PC_OpenH264_INCLUDEDIR}
    ${PC_OpenH264_INCLUDE_DIRS}
)
mark_as_advanced(OpenH264_INCLUDE_DIR)

find_library(OpenH264_LIBRARY
  NAMES
    openh264
    welsdec
  HINTS
    ${PC_OpenH264_LIBDIR}
    ${PC_OpenH264_LIBRARY_DIRS}
)
mark_as_advanced(OpenH264_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenH264
  REQUIRED_VARS
    OpenH264_INCLUDE_DIR
    OpenH264_LIBRARY
  VERSION_VAR
    OpenH264_VERSION
)

if(OpenH264_FOUND)
  set(OpenH264_LIBRARIES ${OpenH264_LIBRARY})
  set(OpenH264_INCLUDE_DIRS ${OpenH264_INCLUDE_DIR})
  set(OpenH264_DEFINITIONS ${PC_OpenH264_CFLAGS_OTHER})

  if(NOT TARGET OpenH264::OpenH264)
    add_library(OpenH264::OpenH264 UNKNOWN IMPORTED)
    set_target_properties(OpenH264::OpenH264 PROPERTIES
      IMPORTED_LOCATION "${OpenH264_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${OpenH264_INCLUDE_DIRS}"
      INTERFACE_COMPILE_OPTIONS "${OpenH264_DEFINITIONS}"
    )
  endif()
endif()
