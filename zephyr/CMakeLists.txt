# Copyright (C) 2026 Teluu Inc. (http://www.teluu.com)
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Zephyr module build for pjproject. It drives pjproject's own per-library
# CMakeLists, so no source list is kept here: Kconfig chooses the features,
# this file maps them onto pjproject's CMake options, and the application
# links the "pjproject" target.

if(NOT CONFIG_PJPROJECT)
  return()
endif()

cmake_path(SET PJ_DIR NORMALIZE ${ZEPHYR_CURRENT_MODULE_DIR})

# Kconfig is the source of truth, so the cache is overwritten every time.
function(pj_set_option var value)
  set(${var} "${value}" CACHE STRING "" FORCE)
endfunction()
function(pj_set_bool var kconfig)
  if(${kconfig})
    set(${var} ON CACHE BOOL "" FORCE)
  else()
    set(${var} OFF CACHE BOOL "" FORCE)
  endif()
endfunction()

pj_set_bool(PJ_SKIP_EXPERIMENTAL_NOTICE TRUE)
pj_set_bool(BUILD_SHARED_LIBS FALSE)
pj_set_bool(BUILD_TESTING FALSE)
pj_set_bool(PJ_WITH_CXX FALSE)
pj_set_bool(PJ_BUILD_APPS FALSE)

pj_set_bool(PJLIB_WITH_THREADS CONFIG_PJPROJECT_THREADS)
pj_set_bool(PJLIB_WITH_FLOATING_POINT CONFIG_PJPROJECT_FLOATING_POINT)
pj_set_bool(PJLIB_WITH_LIBUUID FALSE)
pj_set_option(PJLIB_WITH_IOQUEUE select)
if(CONFIG_PJPROJECT_TLS)
  pj_set_option(PJLIB_WITH_SSL mbedtls)
  set(MbedTLS_DIR ${CMAKE_CURRENT_LIST_DIR}/cmake CACHE PATH "" FORCE)
else()
  pj_set_option(PJLIB_WITH_SSL "")
endif()

# Bundled third-party libraries: libsrtp and libresample are always built,
# and an application that does not use them does not link them. The codecs
# needing other libraries are out.
foreach(_dep G7221 GSM ILBC SPEEX WEBRTC WEBRTC_AEC3 YUV)
  pj_set_option(PJ_DEP_${_dep} none)
endforeach()
pj_set_option(PJ_DEP_SRTP bundled)
# libsrtp's own crypto; the host's OpenSSL must not be picked up
pj_set_bool(SRTP_WITH_OPENSSL FALSE)
pj_set_option(PJ_DEP_RESAMPLE bundled)
pj_set_option(PJMEDIA_WITH_RESAMPLE libresample)
pj_set_bool(PJNATH_WITH_UPNP FALSE)

# Codecs and media. G.711, G.722 and L16 need no library and follow
# config_site.h (PJMEDIA_HAS_*_CODEC); the rest are out.
foreach(_opt OPENCORE_AMRNB_CODEC OPENCORE_AMRWB_CODEC SILK_CODEC OPUS_CODEC
             BCG729_CODEC)
  pj_set_bool(PJMEDIA_WITH_${_opt} FALSE)
endforeach()
pj_set_bool(PJMEDIA_WITH_VIDEO FALSE)
pj_set_bool(PJMEDIA_WITH_AUDIODEV TRUE)
pj_set_bool(PJMEDIA_WITH_AUDIODEV_NULL TRUE)

# pjproject's configure-time checks compile a test program and would fail
# trying to link one for Zephyr; compiling is enough for them.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
# pthreads live in Zephyr itself, there is no libpthread to find.
set(CMAKE_HAVE_LIBC_PTHREAD 1 CACHE INTERNAL "")

# What pjproject's root CMakeLists would have provided to its subdirectories
list(INSERT CMAKE_MODULE_PATH 0 ${PJ_DIR}/cmake)
include(Pj/Option)
include(Pj/PkgConfig)
include(GNUInstallDirs)

# config_site.h is written by the user, as on every other platform, and
# must select the Zephyr preset. Checked here because pjlib's CMake would
# otherwise create an empty one, which builds with desktop defaults.
set(_site ${PJ_DIR}/pjlib/include/pj/config_site.h)
set(_site_content "")
if(EXISTS ${_site})
  file(READ ${_site} _site_content)
endif()
if(NOT _site_content MATCHES "PJ_CONFIG_ZEPHYR")
  message(FATAL_ERROR "${_site} is missing or does not select the Zephyr "
    "preset. Create it with at least:\n"
    "    #define PJ_CONFIG_ZEPHYR 1\n"
    "    #include <pj/config_site_sample.h>\n"
    "See README_Zephyr.md, section 2.")
endif()

foreach(_lib third_party pjlib pjlib-util pjnath pjmedia pjsip)
  add_subdirectory(${PJ_DIR}/${_lib} ${CMAKE_CURRENT_BINARY_DIR}/${_lib}
                   EXCLUDE_FROM_ALL)
endforeach()

# Zephyr's include paths, flags and generated headers for every target
foreach(_t pjlib pjlib-util pjnath pjmedia pjmedia-codec pjmedia-audiodev
           pjsip pjsip-simple pjsip-ua pjsua-lib srtp resample)
  if(TARGET ${_t})
    target_link_libraries(${_t} PRIVATE zephyr_interface)
    add_dependencies(${_t} zephyr_generated_headers)
  endif()
endforeach()
# libsrtp's x86 byte swap uses the asm keyword, which Zephyr's -std=c99
# takes away (only native_sim is affected). Zephyr's own options come after
# the target's, so a -std override is lost; spell the keyword instead.
if(TARGET srtp)
  target_compile_definitions(srtp PRIVATE asm=__asm__)
endif()

# What an application links. pjnath is there because pjmedia.h includes its
# headers; it costs nothing in the image unless the application uses ICE.
add_library(pjproject INTERFACE)
target_link_libraries(pjproject INTERFACE
  pjsip-ua pjsip-simple pjsip pjmedia-codec pjmedia-audiodev pjmedia pjnath
  pjlib-util pjlib
)
