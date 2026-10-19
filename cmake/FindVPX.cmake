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

if(VPX_INCLUDE_DIR AND VPX_LIBRARY)
  set(_vpx_check_libs "${VPX_LIBRARY}")
  if(VPX_LIBRARY MATCHES "\\${CMAKE_STATIC_LIBRARY_SUFFIX}$" AND UNIX AND NOT APPLE)
    # A static libvpx needs libm and pthread
    find_package(Threads QUIET)
    list(APPEND _vpx_check_libs m)
    if(TARGET Threads::Threads)
      list(APPEND _vpx_check_libs Threads::Threads)
    endif()
  endif()

  include(Pj/CheckLibraryUsable)
  pj_check_library_usable(VPX
    INCLUDE_DIRS "${VPX_INCLUDE_DIR}"
    LIBRARIES ${_vpx_check_libs}
    DEFINITIONS ${PC_VPX_CFLAGS_OTHER}
    CODE [=[
      #include <vpx/vpx_encoder.h>
      #include <vpx/vp8cx.h>
      int main() { vpx_codec_iface_t *(*enc_if)() = &vpx_codec_vp8_cx; }
    ]=]
  )
  unset(_vpx_check_libs)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VPX
  REQUIRED_VARS
    VPX_INCLUDE_DIR
    VPX_LIBRARY
    VPX_USABLE
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
