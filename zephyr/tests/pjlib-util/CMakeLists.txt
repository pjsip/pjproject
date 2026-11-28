# Copyright (C) 2026 Teluu Inc. (http://www.teluu.com)
# SPDX-License-Identifier: GPL-2.0-or-later
#
# pjlib-util-test on Zephyr: the suite built as a Zephyr application with pjproject
# as a module from this same tree. Kconfig picks the tests; see testcase.yaml
# for running it under twister.

cmake_minimum_required(VERSION 3.20.0)

list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_LIST_DIR}/../../..)
# The emulated boards as stand-ins for the 640 KB target: zephyr/snippets
set(SNIPPET pjproject-target)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(pjlib_util_test)

target_sources(app PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../../../pjlib-util/src/pjlib-util-test/main.c
)
target_link_libraries(app PRIVATE pjlib-util-test-sources pjlib-util pjlib)

