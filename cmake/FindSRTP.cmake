function(_srtp_find version)
  if(DEFINED SRTP_FIND_VERSION_MAJOR
      AND NOT SRTP_FIND_VERSION_MAJOR EQUAL ${version})
    return()
  endif()

  # version-specific names
  if(version EQUAL 2)
    set(libs libsrtp2 srtp2)
    set(headers srtp2/srtp.h)
  else()
    set(libs libsrtp srtp)
    set(headers srtp/srtp.h)
  endif()

  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND)
    pkg_search_module(PC_SRTP QUIET ${libs})
  endif()

  find_path(SRTP_INCLUDE_DIR
    NAMES
      ${headers}
    HINTS
      ${PC_SRTP_INCLUDEDIR}
      ${PC_SRTP_INCLUDE_DIRS}
    )

  find_library(SRTP_LIBRARY
    NAMES
      libsrtp2
      srtp2
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

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SRTP
  REQUIRED_VARS
    SRTP_INCLUDE_DIR
    SRTP_LIBRARY
  VERSION_VAR
    SRTP_VERSION
)

if(SRTP_FOUND)
  set(SRTP_LIBRARIES ${SRTP_LIBRARY})
  set(SRTP_INCLUDE_DIRS ${SRTP_INCLUDE_DIR})
  set(SRTP_DEFINITIONS ${PC_SRTP_CFLAGS_OTHER})

  # check for `srtp_deinit` and `srtp_shutdown`
  block(SCOPE_FOR VARIABLES PROPAGATE SRTP_HAS_DEINIT SRTP_HAS_SHUTDOWN)
    include(CheckFunctionExists)
    include(CMakePushCheckState)

    cmake_push_check_state(RESET)
      set(CMAKE_REQUIRED_INCLUDES "${SRTP_INCLUDE_DIRS}")
      set(CMAKE_REQUIRED_LIBRARIES "${SRTP_LIBRARIES}")
      set(CMAKE_REQUIRED_DEFINITIONS "${SRTP_DEFINITIONS}")

      check_function_exists(srtp_deinit SRTP_HAS_DEINIT)
      check_function_exists(srtp_shutdown SRTP_HAS_SHUTDOWN)
    cmake_pop_check_state()
  endblock()

  if(NOT TARGET SRTP::SRTP)
    add_library(SRTP::SRTP UNKNOWN IMPORTED)
    set_target_properties(SRTP::SRTP PROPERTIES
      IMPORTED_LOCATION "${SRTP_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${SRTP_INCLUDE_DIRS}"
      INTERFACE_COMPILE_OPTIONS "${SRTP_DEFINITIONS}"

      # target metadata
      SRTP_HAS_DEINIT "$<BOOL:${SRTP_HAS_DEINIT}>"
      SRTP_HAS_SHUTDOWN "$<BOOL:${SRTP_HAS_SHUTDOWN}>"
      SRTP_VERSION_MAJOR "${SRTP_VERSION_MAJOR}"
    )
  endif()
endif()

mark_as_advanced(SRTP_INCLUDE_DIR SRTP_LIBRARY)
