find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_search_module(PC_UPNP QUIET upnp libupnp)
endif()

find_path(UPNP_INCLUDE_DIR
  NAMES
    "upnp.h"
  HINTS
    ${PC_UPNP_INCLUDEDIR}
    ${PC_UPNP_INCLUDE_DIRS}
    ${UPNP_ROOT}
  PATH_SUFFIXES
    upnp
)
mark_as_advanced(UPNP_INCLUDE_DIR)

find_library(UPNP_UPNP_LIBRARY
  NAMES
    upnp
    libupnp
  HINTS
    ${PC_UPNP_LIBDIR}
    ${PC_UPNP_LIBRARY_DIRS}
)
mark_as_advanced(UPNP_UPNP_LIBRARY)

find_library(UPNP_IXML_LIBRARY
  NAMES
    ixml
    libixml
  HINTS
    ${PC_UPNP_LIBDIR}
    ${PC_UPNP_LIBRARY_DIRS}
)
mark_as_advanced(UPNP_IXML_LIBRARY)

if(DEFINED PC_UPNP_VERSION AND NOT PC_UPNP_VERSION STREQUAL "")
  set(UPNP_VERSION "${PC_UPNP_VERSION}")
elseif(UPNP_INCLUDE_DIR)
  cmake_path(APPEND _upnp_config_header "${UPNP_INCLUDE_DIR}" upnpconfig.h)

  if(EXISTS "${_upnp_config_header}")
    include(Pj/GetMacroValue)
    pj_get_macro_value("${_upnp_config_header}" UPNP_VERSION_STRING
      _upnp_version_value
      TYPE string
    )

    if(NOT _upnp_version_value STREQUAL "")
      set(UPNP_VERSION "${_upnp_version_value}")
    endif()

    unset(_upnp_version_value)
  endif()

  unset(_upnp_config_header)
endif()

if(UPNP_INCLUDE_DIR AND UPNP_UPNP_LIBRARY)
  set(_upnp_check_libs "${UPNP_UPNP_LIBRARY}")
  if(UPNP_IXML_LIBRARY)
    list(APPEND _upnp_check_libs "${UPNP_IXML_LIBRARY}")
  endif()
  if(UPNP_UPNP_LIBRARY MATCHES "\\${CMAKE_STATIC_LIBRARY_SUFFIX}$" AND UNIX)
    # A static libupnp needs pthread
    find_package(Threads QUIET)
    if(TARGET Threads::Threads)
      list(APPEND _upnp_check_libs Threads::Threads)
    endif()
  endif()

  # As autoconf: rejects a library built for another target
  include(Pj/CheckLibraryUsable)
  pj_check_library_usable(UPNP
    INCLUDE_DIRS "${UPNP_INCLUDE_DIR}"
    LIBRARIES ${_upnp_check_libs}
    DEFINITIONS ${PC_UPNP_CFLAGS_OTHER}
    CODE [=[
      #include <upnp.h>
      int main(void) { return UpnpInit2(NULL, 0); }
    ]=]
  )
  unset(_upnp_check_libs)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(UPNP
  REQUIRED_VARS
    UPNP_UPNP_LIBRARY
    UPNP_INCLUDE_DIR
    UPNP_USABLE
  VERSION_VAR
    UPNP_VERSION
)

if(UPNP_FOUND)
  set(UPNP_LIBRARIES ${UPNP_UPNP_LIBRARY})
  set(UPNP_INCLUDE_DIRS ${UPNP_INCLUDE_DIR})
  set(UPNP_DEFINITIONS ${PC_UPNP_CFLAGS_OTHER})

  if(NOT TARGET UPNP::UPNP)
    add_library(UPNP::UPNP UNKNOWN IMPORTED)
    # Quoted: an empty value would shift the property/value pairs
    set_target_properties(UPNP::UPNP PROPERTIES
      IMPORTED_LOCATION "${UPNP_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${UPNP_INCLUDE_DIRS}"
      INTERFACE_COMPILE_OPTIONS "${UPNP_DEFINITIONS}"
    )
    if(UPNP_IXML_LIBRARY)
      set_target_properties(UPNP::UPNP PROPERTIES
        INTERFACE_LINK_LIBRARIES "${UPNP_IXML_LIBRARY}"
      )
    endif()
  endif()
endif()

