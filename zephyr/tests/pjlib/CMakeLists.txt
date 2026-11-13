# Copyright (C) 2026 Teluu Inc. (http://www.teluu.com)
# SPDX-License-Identifier: GPL-2.0-or-later
#
# pjlib-test on Zephyr: PJLIB's own test suite, built as a Zephyr application
# with pjproject as a module from this same tree. See testcase.yaml for
# running it under twister.

cmake_minimum_required(VERSION 3.20.0)

list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_LIST_DIR}/../../..)
# The emulated boards as stand-ins for the 640 KB target: zephyr/snippets
set(SNIPPET pjproject-sim)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(pjlib_test)

target_sources(app PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../../../pjlib/src/pjlib-test/main.c
)
target_link_libraries(app PRIVATE pjlib-test-sources pjlib)
# The ioqueue tests register more sockets than the preset's 16 per I/O
# queue. Set here rather than in the tree's config_site.h, which is the
# user's. CONFIG_ZVFS_OPEN_MAX in prj.conf must be at least this.
zephyr_compile_definitions(PJ_IOQUEUE_MAX_HANDLES=64)
# Test sizes follow the board's RAM: PJTEST_SMALL_FOOTPRINT in
# pjlib/src/pjlib-test/test_profile.h
