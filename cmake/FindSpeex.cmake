find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_search_module(PC_Speex QUIET speex libspeex)
endif()

find_path(Speex_INCLUDE_DIR
  NAMES
    "speex/speex.h"
  HINTS
    ${PC_Speex_INCLUDEDIR}
    ${PC_Speex_INCLUDE_DIRS}
)
mark_as_advanced(Speex_INCLUDE_DIR)

find_library(Speex_LIBRARY
  NAMES
    speex
    libspeex
  HINTS
    ${PC_Speex_LIBDIR}
    ${PC_Speex_LIBRARY_DIRS}
)
mark_as_advanced(Speex_LIBRARY)

if(DEFINED PC_Speex_VERSION AND NOT PC_Speex_VERSION STREQUAL "")
  set(Speex_VERSION "${PC_Speex_VERSION}")
elseif(Speex_INCLUDE_DIR)
  cmake_path(APPEND _speex_header "${UPNP_INCLUDE_DIR}" speex.h)

  if(IS_READABLE _speex_header)
    include(Pj/GetMacroValue)
    foreach(label in ITEMS major minor micro extra)
      string(TOUPPER "SPEEX_LIB_GET_${label}_VERSION" macro)
      pj_get_macro_value("${_speex_header}" "${macro}" "_speex_${label}")
    endforeach()
    unset(label)
    unset(macro)

    if (NOT _speex_major STREQUAL "" AND
        NOT _speex_minor STREQUAL "" AND
        NOT _speex_micro STREQUAL "")
      set(_speex_version
        "${_speex_major}.${_speex_minor}.${_speex_micro}" PARENT_SCOPE
      )

      if(NOT _speex_extra STREQUAL "")
        set(_speex_version "${_speex_version}.${_speex_extra}")
      endif()

      set(SPEEX_VERSION "${_speex_version}")

      unset(_speex_major)
      unset(_speex_minor)
      unset(_speex_micro)
      unset(_speex_extra)
      unset(_speex_version)
    endif()
  endif()

  unset(_speex_header)
endif()
mark_as_advanced(Speex_VERSION)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Speex
  REQUIRED_VARS
    Speex_INCLUDE_DIR
    Speex_LIBRARY
  VERSION_VAR
    Speex_VERSION
)

if(Speex_FOUND)
  set(Speex_INCLUDE_DIRS ${Speex_INCLUDE_DIR})
  set(Speex_LIBRARIES ${Speex_LIBRARY})
  set(Speex_DEFINITIONS ${PC_Speex_CFLAGS_OTHER})

  if(NOT TARGET Speex::Speex)
    add_library(Speex::Speex UNKNOWN IMPORTED)
    set_target_properties(Speex::Speex PROPERTIES
      IMPORTED_LOCATION "${Speex_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${Speex_INCLUDE_DIRS}"
      INTERFACE_COMPILE_OPTIONS "${Speex_DEFINITIONS}"
    )
  endif()
endif()
