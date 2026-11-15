# Copyright (C) 2026 Teluu Inc. (http://www.teluu.com)
# SPDX-License-Identifier: GPL-2.0-or-later
#
# pjsip-test on Zephyr: the suite built as a Zephyr application with pjproject
# as a module from this same tree. Kconfig picks the tests; see testcase.yaml
# for running it under twister.

cmake_minimum_required(VERSION 3.20.0)

list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_LIST_DIR}/../../..)
# The emulated boards as stand-ins for the 640 KB target: zephyr/snippets
set(SNIPPET pjproject-target)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(pjsip_test)

target_sources(app PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../../../pjsip/src/test/main.c
)
target_link_libraries(app PRIVATE pjsip-test-sources pjsip-ua pjsip-simple pjsip pjmedia pjnath pjlib-util pjlib srtp)
# The suite's tests register more modules than the preset's 16 at a time,
# the ones of the dialog and invite tests staying until the end
zephyr_compile_definitions(PJSIP_MAX_MODULE=32)
# The tests create I/O queues for more sockets than the preset's 16. Set
# here rather than in the tree's config_site.h, which is the user's.
# CONFIG_ZVFS_OPEN_MAX in prj.conf must be at least this.
zephyr_compile_definitions(PJ_IOQUEUE_MAX_HANDLES=64)
