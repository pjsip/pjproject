# Copyright (C) 2026 Teluu Inc. (http://www.teluu.com)
# SPDX-License-Identifier: GPL-2.0-or-later
#
# pjlib-test on Zephyr: PJLIB's own test suite, built as a Zephyr application
# with pjproject as a module from this same tree. See testcase.yaml for
# running it under twister.

cmake_minimum_required(VERSION 3.20.0)

list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_LIST_DIR}/../../..)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(pjlib_test)

target_sources(app PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../../../pjlib/src/pjlib-test/main.c
)
target_link_libraries(app PRIVATE pjlib-test-sources pjlib)
# Test sizes follow the board's RAM: PJTEST_SMALL_FOOTPRINT in
# pjlib/src/pjlib-test/test_profile.h
