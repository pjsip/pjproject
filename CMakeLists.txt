cmake_minimum_required(VERSION 3.28...4.0)

project(pjproject
  VERSION 2.15
  DESCRIPTION "Multimedia communication library"
  HOMEPAGE_URL "http://www.pjsip.org"
  LANGUAGES C CXX
)

# project modules
list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(SelectRuntime)

# ##############################################################################
# Testing support

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  include(CTest)
endif()

if(BUILD_TESTING)
  if(WIN32)
    set(PJ_TEST_MAIN "main_win32.c")
  elseif(CMAKE_SYSTEM_NAME MATCHES "rtems")
    set(PJ_TEST_MAIN "main_rtems.c")
  else()
    set(PJ_TEST_MAIN "main.c")
  endif()
endif()

# ##############################################################################
# Installation preparation

include(GNUInstallDirs)

# add `$ORIGIN` to targets' RPATH
if(BUILD_SHARED_LIBS)
  if(APPLE)
    list(APPEND CMAKE_INSTALL_RPATH
      "@loader_path"
      "@executable_path/../${CMAKE_INSTALL_LIBDIR}"
    )
  elseif(UNIX)
    list(APPEND CMAKE_INSTALL_RPATH
      "$ORIGIN"
      "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
    )
  endif()
endif()

# ##############################################################################
# Sub-directories

# base library
add_subdirectory(pjlib)

# third-party libraries
add_subdirectory(third_party)

# remaning libraries
add_subdirectory(pjlib-util)
add_subdirectory(pjnath)
add_subdirectory(pjmedia)
add_subdirectory(pjsip)
add_subdirectory(pjsip-apps)

# ##############################################################################
# Installation

set(install_targets
  pjlib
  pjlib-ssl
  pjlib++
  pjlib-util
  pjnath
  pjmedia
  pjmedia-codec
  pjmedia-audiodev
  pjmedia-videodev
  pjsip
  pjsip-simple
  pjsip-ua
  pjsua-lib
  pjsua2
  pjsua
)

# set `VERSION` and `SO_VERSION`
foreach(target IN LISTS install_targets)
  set_target_properties(${target} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
  )
endforeach()

# install targets
install(
  TARGETS ${install_targets}
  EXPORT PjTargets

  FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT PjDevelopment
  FILE_SET config_headers
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT PjDevelopment
  RUNTIME
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT PjRuntime
  LIBRARY
    DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    COMPONENT PjRuntime
    NAMELINK_COMPONENT PjDevelopment
  ARCHIVE
    DESTINATION  "${CMAKE_INSTALL_BINDIR}"
    COMPONENT PjDevelopment
)

# ##############################################################################
# CMAKe Package config

include(CMakePackageConfigHelpers)

set(PJ_CMAKE_INSTALL_DIR         "${CMAKE_INSTALL_LIBDIR}/cmake/Pj")
set(PJ_CMAKE_TARGETS_FILE        "PjTargets.cmake")
set(PJ_CMAKE_CONFIG_FILE         "PjConfig.cmake")
set(PJ_CMAKE_CONFIG_VERSION_FILE "jConfigVersion.cmake")

# install exports
install(
  EXPORT      PjTargets
  NAMESPACE   Pj::
  FILE        "${PJ_CMAKE_TARGETS_FILE}"
  DESTINATION "${PJ_CMAKE_INSTALL_DIR}"
  COMPONENT   PjDevelopment
)

# write package config
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Pj/${PJ_CMAKE_CONFIG_FILE}.in"
  "${CMAKE_CURRENT_BINARY_DIR}/${PJ_CMAKE_CONFIG_FILE}"

  INSTALL_DESTINATION "${PJ_CMAKE_INSTALL_DIR}"
)

# write package config version
write_basic_package_version_file(
  "${PJ_CMAKE_CONFIG_VERSION_FILE}"

  COMPATIBILITY SameMajorVersion
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/${PJ_CMAKE_CONFIG_FILE}"
  "${CMAKE_CURRENT_BINARY_DIR}/${PJ_CMAKE_CONFIG_VERSION_FILE}"

  DESTINATION "${PJ_CMAKE_INSTALL_DIR}"
  COMPONENT PjDevelopment
)

# ##############################################################################
# pkg-config file

set(PREFIX "${CMAKE_INSTALL_PREFIX}") # this should be done at install time
set(LIBDIR "${PREFIX}/${CMAKE_INSTALL_LIBDIR}")
set(INCLUDEDIR "${PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}")
set(PJ_VERSION "${PROJECT_VERSION}")
set(PJ_INSTALL_LDFLAGS "")
set(PJ_INSTALL_LDFLAGS "")

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/libpjproject.pc.in"
  "${CMAKE_CURRENT_BINARY_DIR}/libpjproject.pc"
)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/libpjproject.pc"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)
