# Copyright (C) 2026 Teluu Inc. (http://www.teluu.com)
# SPDX-License-Identifier: GPL-2.0-or-later
#
# simple_pjsua on Zephyr: the pjsua-lib sample from pjsip-apps/src/samples,
# built as a Zephyr application with pjproject as a module from this same
# tree.

cmake_minimum_required(VERSION 3.20.0)

list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_LIST_DIR}/../../..)
# The emulated boards as stand-ins for the 640 KB target: zephyr/snippets
set(SNIPPET pjproject-target)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(simple_pjsua)

target_sources(app PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../../../pjsip-apps/src/samples/simple_pjsua.c
)
target_link_libraries(app PRIVATE pjproject)
# pjsua-lib registers 20 modules with the endpoint; the preset's 16 is for
# an application on pjsip, the recommended way on a small target. The value
# sizes structures in pjsip, so it is set for the whole build
zephyr_compile_definitions(PJSIP_MAX_MODULE=32)
