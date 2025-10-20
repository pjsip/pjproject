find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_Lyra lyra)
  set(Lyra_VERSION ${PC_Lyra_VERSION})
endif()

find_path(Lyra_INCLUDE_DIR
  NAMES
    "lyra_decoder.h"
  HINTS
    ${PC_Lyra_INCLUDEDIR}
    ${PC_Lyra_INCLUDE_DIRS}
)
mark_as_advanced(Lyra_INCLUDE_DIR)

find_library(Lyra_LIBRARY
  NAMES
    lyra
  HINTS
    ${PC_Lyra_LIBDIR}
    ${PC_Lyra_LIBRARY_DIRS}
)
mark_as_advanced(Lyra_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Lyra
  REQUIRED_VARS
    Lyra_LIBRARY
    Lyra_INCLUDE_DIR
  VERSION_VAR
    Lyra_VERSION
)

if(Lyra_FOUND)
  set(Lyra_LIBRARIES ${Lyra_LIBRARY})
  set(Lyra_INCLUDE_DIRS ${Lyra_INCLUDE_DIR})
  set(Lyra_DEFINITIONS ${PC_Lyra_CFLAGS_OTHER})

  # get installation prefix
  if(IS_DIRECTORY Lyra_ROOT)
    set(Lyra_PREFIX "${Lyra_ROOT}")
  else()
    cmake_path(GET Lyra_INCLUDE_DIR PARENT_PATH Lyra_PREFIX)
  endif()

  if(NOT TARGET Lyra::Lyra)
    add_library(Lyra::Lyra UNKNOWN IMPORTED)
    set_target_properties(Lyra::Lyra PROPERTIES
      IMPORTED_LOCATION "${Lyra_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${Lyra_INCLUDE_DIRS}"
      INTERFACE_COMPILE_OPTIONS "${Lyra_DEFINITIONS}"
      # installation prefix
      LYRA_PREFIX "${Lyra_PREFIX}"
    )
  endif()
endif()
