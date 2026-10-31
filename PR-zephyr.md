# Zephyr RTOS support (PR description draft)

This file is the draft of the pull request description for the `zephyr-support` branch. It is tracked here so that it can be reviewed with the code, and will be removed before the merge.

---

## Zephyr RTOS support: pjproject as a Zephyr module, embedded_ua sample, pjlib-test on twister

This is the third part of bringing PJSIP to Zephyr RTOS, after #5314 (portability fixes for minimal POSIX C libraries) and #5316 (CMake cross-compiling for generic targets). With this PR, pjproject builds as a Zephyr module and a complete SIP user agent runs on a Cortex-M33.

### Objective

Run a SIP endpoint on a small microcontroller: an STM32H563 class device with a Cortex-M33 at 250 MHz, 640 KB of RAM and 2 MB of flash, as a statically linked firmware image with no MMU and no filesystem. The endpoint registers to a PBX, carries a few concurrent G.711 or G.722 calls with SDES-SRTP, and will speak SIP over TLS through Zephyr's Mbed TLS. No video, no echo canceller, no ICE.

The port had to be reusable: the Zephyr specific code is kept to a minimum and lives in a layout that a port to another RTOS can follow, and everything that could be fixed generically was fixed generically.

### What the target looks like

Zephyr provides a POSIX layer over its own kernel and network stack, and the port runs on that layer: BSD sockets, `select()`, `clock_gettime()` and, optionally, pthreads. It differs from a Unix in ways that matter to pjlib:

- There is no `getifaddrs()`; interfaces are enumerated through Zephyr's `net_if` API.
- pthread keys exist only for threads created with `pthread_create()`, so `pthread_setspecific()` fails on the main thread, where `pj_init()` is normally called.
- `sockaddr_in` is 8 bytes rather than 16, `inet_pton()` zeroes its output on failure, `bind()` writes the chosen port back into the caller's address, a failed `connect()` is flagged in both the write and the exception set of `select()`, and `socketpair()` only exists for AF_UNIX.
- The C library's `rand()` is unseeded, and `exit()` does not end the program.
- Everything is compiled with `-std=c99`, which defines `__STRICT_ANSI__`.
- Memory is scarce: on the 640 KB target the test suite runs with 250 KB of heap and a 32 KB main stack, and `CONFIG_HW_STACK_PROTECTION` faults on any stack overflow instead of letting it pass silently.

Tested targets are `native_sim/native/64`, which runs Zephyr as a Linux process, and `mps2/an521/cpu0`, a Cortex-M33 under QEMU limited to 640 KB of RAM to match the STM32H563. Both register to a SIP server and complete calls with zero packet loss, single threaded and with pjsip and the media on two pthreads.

### Changes

**The Zephyr module** (`zephyr/`): `module.yml`, a `Kconfig` with the `CONFIG_PJPROJECT_*` options (threads, TLS, SRTP, codecs, resampling, floating point, log level, ioqueue size, custom `config_site.h`), and a `CMakeLists.txt` that drives pjproject's own per-library CMake files with options derived from Kconfig, generates `config_site.h`, applies Zephyr's include paths and flags to every library, and defines a `pjproject` link target. `cmake/MbedTLSConfig.cmake` lets pjlib's Mbed TLS backend find Zephyr's Mbed TLS.

**The platform layer in pjlib**: `pj/compat/os_zephyr.h` describes the platform from Kconfig (available headers and socket functions, threads, TCP, TLS, no IPv6), selected by `pj/config.h` when `__ZEPHYR__` is defined. `pj/ip_helper_zephyr.c` enumerates interfaces and routes through `net_if`. A `PJ_CONFIG_ZEPHYR` preset in `config_site_sample.h` sizes pjsip and pjmedia for a small endpoint and follows the Kconfig codec and logging choices. `compat/rand.h` uses Zephyr's entropy-fed generator.

**Thread-local storage in compiler TLS**: a new `PJ_THREAD_LOCAL` keyword in the compiler headers and a `PJ_THREAD_LOCAL_USE_COMPILER_TLS` option make `pj_thread_local_*()` in `os_core_unix.c` use a `__thread` slot array instead of pthread keys. Slots carry a generation number, in a per-thread array that makes the TLS block about 270 bytes, so that a freed and reallocated slot reads NULL in every thread, as a new pthread key would. The same array serves the no-threads build. Thread attributes are now kept in the thread record until join or destroy, because Zephyr allocates the thread's stack in `pthread_attr_init()` and refuses to destroy the attributes while the thread runs.

**Generic fixes exposed by the target**: `pj_sock_bind()` passes a copy of the address; `pj_inet_aton()` restores `INADDR_NONE` on failure; the socketpair fallback binds the client socket explicitly and `pj_sock_socketpair()` falls back for every "not supported" errno; the select ioqueue queues one event for a failed connect, since the exception event was a no-op that still counted in the poll result; `__attribute__` based macros are defined under `__STRICT_ANSI__` too; ARM endianness and the generic ARM target name are derived from compiler macros; `pj_jmp_buf` can be sized by the platform header; a few macro collisions and float promotion warnings in pjlib, pjlib-util and pjmedia are fixed.

**embedded_ua on Zephyr** (`zephyr/samples/embedded_ua/`): the sample in `pjsip-apps/src/samples/embedded_ua.c`, with its build settings renamed from `EMBUA_*` to `CONFIG_EMBUA_*` and all of them exposed in the sample's Kconfig (the on/off ones as ints, so that they are always defined), builds as a Zephyr sample for native_sim and the an521 board, with a threads overlay and a `run-qemu.sh` that sets up the TAP networking.

**pjlib-test on Zephyr** (`zephyr/tests/pjlib/`): pjlib's test suite builds as a Zephyr test application and runs under twister on both targets, single threaded and with threads. The test sources became an INTERFACE target in `pjlib/CMakeLists.txt` so that both builds compile the same list. A new `test_profile.h` defines `PJTEST_SMALL_FOOTPRINT` for boards with less than 2 MB of RAM and `PJTEST_TIME_PASSES_WHEN_BUSY`, 0 on native_sim whose simulated clock stands still while the CPU is busy, and derives the benchmark exclusion and the timer test size from them; the other test suites include it too. The ioqueue stress test's 1.2 MB configuration table became a small const table plus one reused descriptor. Tests that could not run or hang on the target were fixed rather than skipped: the IOCP unregister test now stops its worker on the error path, the stress test accepts an OS without `SO_SNDBUF`, and the TLS slot semantics above are covered by an extended `thread_local_test`.

**Documentation**: `README_Zephyr.md` at the tree root is the full guide, from installing Zephyr to running on native_sim, QEMU and real hardware, using pjproject in an application, configuration, how the port is put together, known limitations and troubleshooting. docs.pjsip.org will link to or copy it.

**SIP over TLS** (`zephyr/samples/embedded_ua/overlay-tls.conf`): pjlib's Mbed TLS backend builds unchanged against Zephyr's Mbed TLS 4.1, found through `zephyr/cmake/MbedTLSConfig.cmake`; the one change in the backend is that it logs the library version from `MBEDTLS_VERSION_STRING_FULL` instead of calling a function that needs `MBEDTLS_VERSION_C`. The overlay configures Mbed TLS as a TLS 1.2 client with one ciphersuite, adds 64 KB to the heap arena for its contexts, and names a TF-PSA-Crypto user configuration that turns on Mbed TLS's pthread threading layer in threaded builds, since PSA Crypto is then called from two threads and Zephyr has no Kconfig option for it. Both targets register over TLS to Kamailio and complete calls; the an521 image is 452 KB of flash and 461 KB of RAM, 528 KB with threads.

### Not in this PR

Footprint figures for the final configuration, and the IPv6 socket address layout. These follow in separate PRs.
