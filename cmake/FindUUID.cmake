find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_search_module(PC_UUID QUIET uuid libuuid)
  set(UUID_VERSION ${PC_UUID_VERSION})
endif()

find_path(UUID_INCLUDE_DIR
  NAMES
    "uuid/uuid.h"
  HINTS
    ${PC_UUID_INCLUDEDIR}
    ${PC_UUID_INCLUDE_DIRS}
)
mark_as_advanced(UUID_INCLUDE_DIR)

find_library(UUID_LIBRARY
  NAMES
    uuid
    libuuid
  HINTS
    ${PC_UUID_LIBDIR}
    ${PC_UUID_LIBRARY_DIRS}
)
mark_as_advanced(UUID_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(UUID
  REQUIRED_VARS
    UUID_INCLUDE_DIR
    UUID_LIBRARY
  VERSION_VAR
    UUID_VERSION
)

if(UUID_FOUND)
  set(UUID_LIBRARIES ${UUID_LIBRARY})
  set(UUID_INCLUDE_DIRS ${UUID_INCLUDE_DIR})
  set(UUID_DEFINITIONS ${PC_UUID_CFLAGS_OTHER})

  if(NOT TARGET UUID::UUID)
    add_library(UUID::UUID UNKNOWN IMPORTED)
    set_target_properties(UUID::UUID PROPERTIES
      IMPORTED_LOCATION "${UUID_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${UUID_INCLUDE_DIRS}"
      INTERFACE_COMPILE_OPTIONS "${UUID_DEFINITIONS}"
    )
  endif()
endif()
