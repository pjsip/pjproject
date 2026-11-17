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
 * The target's RAM in KB, for the test sizes that scale with it. The build
 * may define it. Otherwise it is the board's RAM on Zephyr, which publishes
 * it as CONFIG_SRAM_SIZE in KB (0 on native_sim, which uses host memory),
 * and 1 GB everywhere else.
 */
#ifndef PJTEST_TARGET_RAM_KB
#   if defined(__ZEPHYR__) && defined(CONFIG_SRAM_SIZE) && CONFIG_SRAM_SIZE > 0
#       define PJTEST_TARGET_RAM_KB     CONFIG_SRAM_SIZE
#   else
#       define PJTEST_TARGET_RAM_KB     (1024 * 1024)
#   endif
#endif

/**
 * A test size scaled to the target's RAM: one unit per kb_per_unit KB, no
 * less than min and no more than max. A constant expression, usable in #if.
 */
#define PJTEST_RAM_SCALE(kb_per_unit, min, max) \
    ((PJTEST_TARGET_RAM_KB / (kb_per_unit)) < (min) ? (min) : \
     (PJTEST_TARGET_RAM_KB / (kb_per_unit)) > (max) ? (max) : \
     (PJTEST_TARGET_RAM_KB / (kb_per_unit)))

/**
 * Whether the target's CPU is slow, for the loop counts of the benchmarks.
 * There is no fact to read this from, so the RAM stands in: a target with
 * less than 2 MB is a microcontroller at a few hundred MHz. The build may
 * define it as 0 or 1.
 */
#ifndef PJTEST_SLOW_CPU
#   if PJTEST_TARGET_RAM_KB < 2048
#       define PJTEST_SLOW_CPU          1
#   else
#       define PJTEST_SLOW_CPU          0
#   endif
#endif

/**
 * Whether the clock advances while a thread is busy. On Zephyr's native_sim
 * simulated time stands still until every thread sleeps, so a test that
 * spins, or busy-waits on the clock, never ends there, and a benchmark
 * measures no time at all.
 */
#ifndef PJTEST_TIME_PASSES_WHEN_BUSY
#   if defined(__ZEPHYR__) && defined(CONFIG_ARCH_POSIX)
#       define PJTEST_TIME_PASSES_WHEN_BUSY    0
#   else
#       define PJTEST_TIME_PASSES_WHEN_BUSY    1
#   endif
#endif

/**
 * Whether the host can reach anything beyond its loopback interface. The
 * Zephyr test apps are built with loopback as their only interface: a
 * connection to any other address is refused at once, where a desktop
 * would keep trying.
 */
#ifndef PJTEST_HAS_OUTSIDE_ROUTE
#   if defined(__ZEPHYR__)
#       define PJTEST_HAS_OUTSIDE_ROUTE    0
#   else
#       define PJTEST_HAS_OUTSIDE_ROUTE    1
#   endif
#endif

/**
 * Exclude the benchmarks (pool, socket and ioqueue performance), which need
 * far more time than the functional tests, and a clock that runs while
 * they do.
 */
#ifndef PJ_EXCLUDE_BENCHMARK_TESTS
#   define PJ_EXCLUDE_BENCHMARK_TESTS   (PJTEST_SLOW_CPU || \
                                         !PJTEST_TIME_PASSES_WHEN_BUSY)
#endif

#endif  /* __PJLIB_TEST_PROFILE_H__ */
