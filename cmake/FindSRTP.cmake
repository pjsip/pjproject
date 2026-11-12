include(Pj/PackageRoot)

function(_srtp_find version)
  if(DEFINED SRTP_FIND_VERSION_MAJOR
      AND NOT SRTP_FIND_VERSION_MAJOR EQUAL ${version})
    return()
  endif()

  # version-specific names
  if(version EQUAL 2)
    set(libs libsrtp2 srtp2)
    set(shared_libs libsrtp2.dylib libsrtp2.so srtp2.dylib srtp2.so)
    set(headers srtp2/srtp.h)
  else()
    set(libs libsrtp srtp)
    set(shared_libs libsrtp.dylib libsrtp.so srtp.dylib srtp.so)
    set(headers srtp/srtp.h)
  endif()

  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND)
    pj_pkg_search_module(PC_SRTP QUIET ${libs})
  endif()

  find_path(SRTP_INCLUDE_DIR
    NAMES
      ${headers}
    HINTS
      ${PC_SRTP_INCLUDEDIR}
      ${PC_SRTP_INCLUDE_DIRS}
    )

  # Prefer shared libraries over static
  find_library(SRTP_LIBRARY
    NAMES
      ${shared_libs}
      ${libs}
    HINTS
      ${PC_SRTP_LIBDIR}
      ${PC_SRTP_LIBRARY_DIRS}
  )

  if(NOT SRTP_LIBRARY STREQUAL "" AND NOT SRTP_LIBRARY STREQUAL "")
    if(NOT PC_SRTP_VERSION STREQUAL "")
      set(SRTP_VERSION "${PC_SRTP_VERSION}" PARENT_SCOPE)
    else()
      set(SRTP_VERSION "${version}" PARENT_SCOPE)
    endif()

    set(SRTP_VERSION_MAJOR "${version}" PARENT_SCOPE)
    set(SRTP_FOUND TRUE PARENT_SCOPE)
  endif()
endfunction()

_srtp_find(2)
if(NOT SRTP_FOUND)
  _srtp_find(1)
endif()

# Usable when srtp_init() links, as autoconf checks. Which of srtp_deinit()
# and srtp_shutdown() it has picks how pjmedia ends it. All three are checked
# again when the library changes, e.g. with a new SRTP_ROOT.
if(SRTP_INCLUDE_DIR AND SRTP_LIBRARY)
  include(Pj/CheckLibraryUsable)
  # once check_function_exists() results, which were never checked again
  unset(SRTP_HAS_DEINIT CACHE)
  unset(SRTP_HAS_SHUTDOWN CACHE)
  if(SRTP_FIND_QUIETLY)
    set(_srtp_quiet QUIET)
  endif()
  foreach(_srtp_fn IN ITEMS init deinit shutdown)
    string(TOUPPER "${_srtp_fn}" _srtp_upper)
    pj_check_library_usable(SRTP_${_srtp_upper} ${_srtp_quiet}
      FUNCTION srtp_${_srtp_fn}
      LIBRARIES "${SRTP_LIBRARY}"
      DEFINITIONS ${PC_SRTP_CFLAGS_OTHER}
    )
  endforeach()
  set(SRTP_USABLE ${SRTP_INIT_USABLE})
  set(SRTP_HAS_DEINIT ${SRTP_DEINIT_USABLE})
  set(SRTP_HAS_SHUTDOWN ${SRTP_SHUTDOWN_USABLE})
  unset(_srtp_quiet)
  unset(_srtp_fn)
  unset(_srtp_upper)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SRTP
  REQUIRED_VARS
    SRTP_INCLUDE_DIR
    SRTP_LIBRARY
    SRTP_USABLE
  VERSION_VAR
    SRTP_VERSION
)

if(SRTP_FOUND)
  set(SRTP_LIBRARIES ${SRTP_LIBRARY})
  set(SRTP_INCLUDE_DIRS ${SRTP_INCLUDE_DIR})
  set(SRTP_DEFINITIONS ${PC_SRTP_CFLAGS_OTHER})

  if(NOT TARGET SRTP::SRTP)
    add_library(SRTP::SRTP UNKNOWN IMPORTED GLOBAL)
    set_target_properties(SRTP::SRTP PROPERTIES
      IMPORTED_LOCATION "${SRTP_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${SRTP_INCLUDE_DIRS}"
      INTERFACE_COMPILE_OPTIONS "${SRTP_DEFINITIONS}"

      # target metadata
      SRTP_HAS_DEINIT "$<BOOL:${SRTP_HAS_DEINIT}>"
      SRTP_HAS_SHUTDOWN "$<BOOL:${SRTP_HAS_SHUTDOWN}>"
      SRTP_VERSION_MAJOR "${SRTP_VERSION_MAJOR}"
    )
    include(Pj/DebugLibrary)
    pj_import_debug_library(SRTP::SRTP SRTP_LIBRARY)
  endif()
endif()

mark_as_advanced(SRTP_INCLUDE_DIR SRTP_LIBRARY)
