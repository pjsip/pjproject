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

  # The header too: both versions use the same cache entries, so on a
  # reconfigure the libsrtp 2 search sees what the libsrtp 1 search set
  if(SRTP_INCLUDE_DIR AND SRTP_LIBRARY
     AND EXISTS "${SRTP_INCLUDE_DIR}/${headers}")
    if(PC_SRTP_VERSION)
      set(SRTP_VERSION "${PC_SRTP_VERSION}" PARENT_SCOPE)
    else()
      set(SRTP_VERSION "${version}" PARENT_SCOPE)
    endif()

    set(SRTP_VERSION_MAJOR "${version}" PARENT_SCOPE)
    set(_srtp_major "${version}" PARENT_SCOPE)
    set(PC_SRTP_CFLAGS_OTHER "${PC_SRTP_CFLAGS_OTHER}" PARENT_SCOPE)
    set(PC_SRTP_STATIC_LIBRARIES "${PC_SRTP_STATIC_LIBRARIES}" PARENT_SCOPE)
  endif()
endfunction()

unset(_srtp_major)
unset(SRTP_USABLE)
_srtp_find(2)
if(NOT _srtp_major)
  _srtp_find(1)
endif()

# OpenSSL as pjlib finds it, so that both link the same one
macro(_srtp_find_openssl)
  if(NOT TARGET OpenSSL::Crypto)
    pj_find_openssl(QUIET COMPONENTS Crypto)
  endif()
endmacro()

# Usable when srtp_init() links, as autoconf checks. Which of srtp_deinit()
# and srtp_shutdown() it has picks how pjmedia ends it. All three are checked
# again when the library changes, e.g. with a new SRTP_ROOT. Only for a
# version found: with neither, both entries may still be set.
if(_srtp_major)
  include(Pj/CheckLibraryUsable)
  # once check_function_exists() results, which were never checked again
  unset(SRTP_HAS_DEINIT CACHE)
  unset(SRTP_HAS_SHUTDOWN CACHE)
  if(SRTP_FIND_QUIETLY)
    set(_srtp_quiet QUIET)
  endif()

  # A static libsrtp needs the libraries it was built with, which pkg-config
  # lists as private. OpenSSL's are taken as pjproject finds OpenSSL. When
  # none are listed, OpenSSL's libcrypto is tried if the library alone fails.
  set(_srtp_link_libs)
  if(UNIX AND SRTP_LIBRARY MATCHES "\\${CMAKE_STATIC_LIBRARY_SUFFIX}$")
    foreach(_srtp_lib IN LISTS PC_SRTP_STATIC_LIBRARIES)
      if(_srtp_lib MATCHES "^(lib)?srtp[12]?$")
        continue()
      elseif(_srtp_lib MATCHES "^(crypto|ssl)$")
        _srtp_find_openssl()
        if(_srtp_lib STREQUAL "ssl" AND TARGET OpenSSL::SSL)
          list(APPEND _srtp_link_libs OpenSSL::SSL)
        elseif(TARGET OpenSSL::Crypto)
          list(APPEND _srtp_link_libs OpenSSL::Crypto)
        else()
          list(APPEND _srtp_link_libs ${_srtp_lib})
        endif()
      else()
        list(APPEND _srtp_link_libs ${_srtp_lib})
      endif()
    endforeach()
    list(REMOVE_DUPLICATES _srtp_link_libs)

    if(NOT _srtp_link_libs)
      pj_check_library_usable(SRTP_ALONE ${_srtp_quiet}
        FUNCTION srtp_init
        LIBRARIES "${SRTP_LIBRARY}"
        DEFINITIONS ${PC_SRTP_CFLAGS_OTHER}
      )
      if(NOT SRTP_ALONE_USABLE)
        _srtp_find_openssl()
        if(TARGET OpenSSL::Crypto)
          set(_srtp_link_libs OpenSSL::Crypto)
        endif()
      endif()
    endif()
  endif()

  foreach(_srtp_fn IN ITEMS init deinit shutdown)
    string(TOUPPER "${_srtp_fn}" _srtp_upper)
    pj_check_library_usable(SRTP_${_srtp_upper} ${_srtp_quiet}
      FUNCTION srtp_${_srtp_fn}
      LIBRARIES "${SRTP_LIBRARY}" ${_srtp_link_libs}
      DEFINITIONS ${PC_SRTP_CFLAGS_OTHER}
    )
  endforeach()
  set(SRTP_USABLE ${SRTP_INIT_USABLE})
  set(SRTP_HAS_DEINIT ${SRTP_DEINIT_USABLE})
  set(SRTP_HAS_SHUTDOWN ${SRTP_SHUTDOWN_USABLE})
  unset(_srtp_quiet)
  unset(_srtp_fn)
  unset(_srtp_upper)
  unset(_srtp_lib)
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
  set(SRTP_LIBRARIES ${SRTP_LIBRARY} ${_srtp_link_libs})
  set(SRTP_INCLUDE_DIRS ${SRTP_INCLUDE_DIR})
  set(SRTP_DEFINITIONS ${PC_SRTP_CFLAGS_OTHER})

  if(NOT TARGET SRTP::SRTP)
    add_library(SRTP::SRTP UNKNOWN IMPORTED GLOBAL)
    set_target_properties(SRTP::SRTP PROPERTIES
      IMPORTED_LOCATION "${SRTP_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${SRTP_INCLUDE_DIRS}"
      INTERFACE_COMPILE_OPTIONS "${SRTP_DEFINITIONS}"
      INTERFACE_LINK_LIBRARIES "${_srtp_link_libs}"

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
unset(_srtp_link_libs)
unset(_srtp_major)
