cmake_minimum_required(VERSION 3.28...4.0)

project(pjproject
  VERSION 2.15
  LANGUAGES C CXX
)

# project modules
list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

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
