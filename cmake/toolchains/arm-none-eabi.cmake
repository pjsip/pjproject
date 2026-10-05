# Toolchain file for an ARM target without an operating system
# (CMAKE_SYSTEM_NAME Generic) and a GCC toolchain, for example the Arm GNU
# toolchain or the one in the Zephyr SDK. It is for configuring pjproject
# with no OS underneath; an RTOS build driven by the RTOS's own build system
# (such as a Zephyr module) brings its own toolchain settings and does not
# use this file.
#
#   cmake -S . -B build-arm --toolchain cmake/toolchains/arm-none-eabi.cmake \
#         -DPJ_WITH_CXX=OFF -DPJ_BUILD_APPS=OFF -DBUILD_TESTING=OFF \
#         -DPJLIB_WITH_SSL=
#
# Settings, given with -D or in the environment:
#   PJ_TOOLCHAIN_PREFIX   compiler prefix, including a path when it is not on
#                         PATH (default: arm-none-eabi-), for example
#                         /opt/sdk/arm-zephyr-eabi/bin/arm-zephyr-eabi-
#   PJ_TARGET_CPU_FLAGS   code generation flags
#                         (default: -mcpu=cortex-m33 -mthumb)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

foreach(_var PJ_TOOLCHAIN_PREFIX PJ_TARGET_CPU_FLAGS)
  if(NOT ${_var} AND DEFINED ENV{${_var}})
    set(${_var} "$ENV{${_var}}")
  endif()
endforeach()
if(NOT PJ_TOOLCHAIN_PREFIX)
  set(PJ_TOOLCHAIN_PREFIX arm-none-eabi-)
endif()
if(NOT PJ_TARGET_CPU_FLAGS)
  set(PJ_TARGET_CPU_FLAGS "-mcpu=cortex-m33 -mthumb")
endif()

set(CMAKE_C_COMPILER   ${PJ_TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${PJ_TOOLCHAIN_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${PJ_TOOLCHAIN_PREFIX}gcc)

set(CMAKE_C_FLAGS_INIT   "${PJ_TARGET_CPU_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${PJ_TARGET_CPU_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${PJ_TARGET_CPU_FLAGS}")

# No startup code or linker script: the probes compile, they do not link
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Search the toolchain's own tree only, never the host's libraries and CMake
# packages. GCC's sysroot is used when it reports one. A toolchain built
# without one (the Zephyr SDK, Debian's gcc-arm-none-eabi) is located through
# the libc it links, which lives at <target tree>/lib[/<multilib>]/libc.a.
if(NOT CMAKE_FIND_ROOT_PATH)
  execute_process(COMMAND ${CMAKE_C_COMPILER} -print-sysroot
    RESULT_VARIABLE _pj_cc_result
    OUTPUT_VARIABLE _pj_sysroot OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if(_pj_cc_result EQUAL 0 AND NOT _pj_sysroot)
    execute_process(COMMAND ${CMAKE_C_COMPILER} -print-file-name=libc.a
      OUTPUT_VARIABLE _pj_libc OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
    if(IS_ABSOLUTE "${_pj_libc}")
      get_filename_component(_pj_libc "${_pj_libc}" ABSOLUTE)
      if(_pj_libc MATCHES "^(.*)/lib(/.*)?/libc\\.a$")
        set(_pj_sysroot "${CMAKE_MATCH_1}")
      endif()
    endif()
  endif()
  if(_pj_sysroot AND IS_DIRECTORY "${_pj_sysroot}")
    set(CMAKE_FIND_ROOT_PATH "${_pj_sysroot}")
  elseif(_pj_cc_result EQUAL 0)
    # without a root the ONLY modes below would search the host instead
    message(FATAL_ERROR "cannot locate the target tree of ${CMAKE_C_COMPILER}; "
      "pass -DCMAKE_FIND_ROOT_PATH=<directory holding lib/libc.a>")
  endif()
endif()
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
