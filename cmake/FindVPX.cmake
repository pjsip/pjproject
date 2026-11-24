include(Pj/PackageRoot)
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pj_pkg_search_module(PC_VPX QUIET vpx)
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
  include(Pj/CheckLibraryUsable)
  pj_check_library_usable(VPX
    INCLUDE_DIRS "${VPX_INCLUDE_DIR}"
    LIBRARIES "${VPX_LIBRARY}"
    STATIC_LIBS m Threads::Threads
    DEFINITIONS ${PC_VPX_CFLAGS_OTHER}
    CODE [=[
      #include <vpx/vpx_encoder.h>
      #include <vpx/vp8cx.h>
      int main(void) { return vpx_codec_vp8_cx() == 0; }
    ]=]
  )
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
    include(Pj/DebugLibrary)
    pj_import_debug_library(VPX::VPX VPX_LIBRARY)
    # A static vpx.lib built with /GL (vcpkg's) makes MSVC link with /LTCG
    # and warn that RelWithDebInfo's /INCREMENTAL is ignored (LNK4075). An
    # import library names its DLL's import descriptor near its start.
    if(MSVC)
      file(STRINGS "${VPX_LIBRARY}" _vpx_import
        REGEX "__IMPORT_DESCRIPTOR_" LIMIT_INPUT 65536 LIMIT_COUNT 1)
      if(NOT _vpx_import)
        set_property(TARGET VPX::VPX APPEND PROPERTY INTERFACE_LINK_OPTIONS
          "$<$<CONFIG:RelWithDebInfo>:/INCREMENTAL:NO>")
      endif()
      unset(_vpx_import)
    endif()
  endif()
endif()
