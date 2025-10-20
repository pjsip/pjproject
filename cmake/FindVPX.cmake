find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_VPX QUIET vpx)
  set(VPX_VERSION ${PC_VPX_VERSION})
endif()

find_path(VPX_INCLUDE_DIR
  NAMES
    "vpx/vpx_encoder.h"
  HINTS
    ${PC_VPX_INCLUDEDIR}
    ${PC_VPX_INCLUDE_DIRS}
    ${VPX_ROOT}
)
mark_as_advanced(VPX_INCLUDE_DIR)

find_library(VPX_LIBRARY
  NAMES
    vpx
  HINTS
    ${PC_VPX_LIBDIR}
    ${PC_VPX_LIBRARY_DIRS}
    ${VPX_ROOT}
)
mark_as_advanced(VPX_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VPX
  REQUIRED_VARS
    VPX_INCLUDE_DIR
    VPX_LIBRARY
  VERSION_VAR
    VPX_VERSION
)

if(VPX_FOUND)
  set(VPX_LIBRARIES ${VPX_LIBRARY})
  set(VPX_INCLUDE_DIRS ${VPX_INCLUDE_DIR})
  set(VPX_DEFINITIONS ${PC_VPX_CFLAGS_OTHER})

  if(NOT TARGET VPX::VPX)
    add_library(VPX::VPX UNKNOWN IMPORTED)
    set_target_properties(VPX::VPX PROPERTIES
      IMPORTED_LOCATION "${VPX_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${VPX_INCLUDE_DIRS}"
      INTERFACE_COMPILE_OPTIONS "${VPX_DEFINITIONS}"
    )
  endif()
endif()
