/*
 * Copyright (C) 2026 Teluu Inc. (http://www.teluu.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __PJLIB_TEST_PROFILE_H__
#define __PJLIB_TEST_PROFILE_H__

/**
 * @file test_profile.h
 * @brief Test profile shared by the test suites of every library.
 *
 * Each suite's test.h includes this file first and derives the defaults of
 * its own settings, such as test sizes and which tests to build, from the
 * profile. Every such setting can still be set individually by the build.
 */

/**
 * Select the small footprint profile: test sizes that fit a small embedded
 * target with a few hundred KB of RAM, and no benchmarks. The build may
 * define it as 0 or 1. Otherwise it is 1 on a Zephyr board with less than
 * 2 MB of RAM, which Zephyr publishes as CONFIG_SRAM_SIZE in KB (0 on
 * native_sim, which uses host memory), and 0 everywhere else.
 */
#ifndef PJTEST_SMALL_FOOTPRINT
#   if defined(__ZEPHYR__) && defined(CONFIG_SRAM_SIZE) && \
       CONFIG_SRAM_SIZE > 0 && CONFIG_SRAM_SIZE < 2048
#       define PJTEST_SMALL_FOOTPRINT   1
#   else
#       define PJTEST_SMALL_FOOTPRINT   0
#   endif
#endif

/**
 * Exclude the benchmarks (pool, socket and ioqueue performance), which need
 * far more memory and time than the functional tests.
 */
#ifndef PJ_EXCLUDE_BENCHMARK_TESTS
#   define PJ_EXCLUDE_BENCHMARK_TESTS   PJTEST_SMALL_FOOTPRINT
#endif

#endif  /* __PJLIB_TEST_PROFILE_H__ */
