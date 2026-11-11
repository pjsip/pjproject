include(Pj/PackageRoot)
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pj_pkg_search_module(PC_BCG729 bcg729 libbcg729)
  set(BCG729_VERSION ${PC_BCG729_VERSION})
endif()

find_path(BCG729_INCLUDE_DIR
  NAMES
    "bcg729/encoder.h"
  HINTS
    ${PC_BCG729_INCLUDEDIR}
    ${PC_BCG729_INCLUDE_DIRS}
)
mark_as_advanced(BCG729_INCLUDE_DIR)

find_library(BCG729_LIBRARY
  NAMES
    bcg729
    libbcg729
  HINTS
    ${PC_BCG729_LIBDIR}
    ${PC_BCG729_LIBRARY_DIRS}
)
mark_as_advanced(BCG729_LIBRARY)

if(BCG729_INCLUDE_DIR AND BCG729_LIBRARY)
  include(Pj/CheckLibraryUsable)
  pj_check_library_usable(BCG729
    CODE "
#include <bcg729/encoder.h>
#include <bcg729/decoder.h>
int main(void) { initBcg729EncoderChannel(0); return 0; }
"
    INCLUDE_DIRS ${BCG729_INCLUDE_DIR}
    LIBRARIES "${BCG729_LIBRARY}"
    STATIC_LIBS m
  )
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(BCG729
  REQUIRED_VARS
    BCG729_LIBRARY
    BCG729_INCLUDE_DIR
    BCG729_USABLE
  VERSION_VAR
    BCG729_VERSION
)

if(BCG729_FOUND)
  set(BCG729_LIBRARIES ${BCG729_LIBRARY})
  set(BCG729_INCLUDE_DIRS ${BCG729_INCLUDE_DIR})
  set(BCG729_DEFINITIONS ${PC_BCG729_CFLAGS_OTHER})

  if(NOT TARGET BCG729::BCG729)
    add_library(BCG729::BCG729 UNKNOWN IMPORTED)
    set_target_properties(BCG729::BCG729 PROPERTIES
      IMPORTED_LOCATION "${BCG729_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${BCG729_INCLUDE_DIRS}"
      INTERFACE_COMPILE_OPTIONS "${BCG729_DEFINITIONS}"
    )
    include(Pj/DebugLibrary)
    pj_import_debug_library(BCG729::BCG729 BCG729_LIBRARY)
  endif()
endif()
