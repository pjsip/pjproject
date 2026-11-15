# Copyright (C) 2026 Teluu Inc. (http://www.teluu.com)
# SPDX-License-Identifier: GPL-2.0-or-later
#
# pjmedia-test on Zephyr: PJMEDIA's test suite, built as a Zephyr application
# with pjproject as a module from this same tree. Kconfig picks the tests;
# see testcase.yaml for running it under twister.

cmake_minimum_required(VERSION 3.20.0)

list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_LIST_DIR}/../../..)
# The emulated boards as stand-ins for the 640 KB target: zephyr/snippets
set(SNIPPET pjproject-target)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(pjmedia_test)

target_sources(app PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../../../pjmedia/src/test/main.c
)
target_link_libraries(app PRIVATE pjmedia-test-sources pjmedia-codec
  pjmedia-audiodev pjmedia pjnath pjlib-util pjlib srtp resample)
# The preset builds G.711 only and no resampler; the tests cover G.722 and
# the bundled libresample too. Set here rather than in the tree's
# config_site.h, which is the user's.
zephyr_compile_definitions(PJMEDIA_HAS_G722_CODEC=1
  PJMEDIA_RESAMPLE_IMP=PJMEDIA_RESAMPLE_LIBRESAMPLE)
if(CONFIG_PJMEDIA_TEST_BENCHMARK)
  zephyr_compile_definitions(PJ_EXCLUDE_BENCHMARK_TESTS=0)
endif()
