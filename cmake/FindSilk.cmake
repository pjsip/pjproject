find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_Silk QUIET SKP_SILK_SDK)
  set(Silk_VERSION ${PC_Silk_VERSION})
endif()

find_path(Silk_INCLUDE_DIR
  NAMES
    "SKP_Silk_SDK_API.h"
  HINTS
    ${PC_Silk_INCLUDEDIR}
    ${PC_Silk_INCLUDE_DIRS}
)
mark_as_advanced(Silk_INCLUDE_DIR)

find_library(Silk_LIBRARY
  NAMES
    SKP_SILK_SDK
  HINTS
    ${PC_Silk_LIBDIR}
    ${PC_Silk_LIBRARY_DIRS}
)
mark_as_advanced(Silk_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Silk
  REQUIRED_VARS
    Silk_LIBRARY
    Silk_INCLUDE_DIR
  VERSION_VAR
    Silk_VERSION
)

if(Silk_FOUND)
  set(Silk_LIBRARIES "${Silk_LIBRARY}")
  set(Silk_INCLUDE_DIRS "${Silk_INCLUDE_DIR}")
  set(Silk_DEFINITIONS "${PC_Silk_CFLAGS_OTHER}")

  if(NOT TARGET Silk::Silk)
    add_library(Silk::Silk UNKNOWN IMPORTED)
    set_target_properties(Silk::Silk PROPERTIES
      IMPORTED_LOCATION "${Silk_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${Silk_INCLUDE_DIRS}"
      INTERFACE_COMPILE_OPTIONS "${Silk_DEFINITIONS}"
    )
  endif()
endif()
