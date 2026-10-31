# Copyright (C) 2026 Teluu Inc. (http://www.teluu.com)
# SPDX-License-Identifier: GPL-2.0-or-later
#
# embedded_ua on Zephyr: the sample from pjsip-apps/src/samples, built as a
# Zephyr application with pjproject as a module from this same tree.

cmake_minimum_required(VERSION 3.20.0)

list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_LIST_DIR}/../../..)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(embedded_ua)

target_sources(app PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../../../pjsip-apps/src/samples/embedded_ua.c
)
target_link_libraries(app PRIVATE pjproject)
# For psa_user_config.h, which Mbed TLS's own build has to find
zephyr_include_directories(${CMAKE_CURRENT_LIST_DIR})

# Running under QEMU: see run-qemu.sh. "west build -t run" does not work on
# the MPS2 boards, whose LAN9118 is on the system bus: Zephyr adds a pluggable
# "-device lan9118" that QEMU rejects, and its Kconfig choice has no way to
# leave it out.
