# Zephyr RTOS support (PR description draft)

This file is the draft of the pull request description for the `zephyr-support` branch. It is tracked here so that it can be reviewed with the code, and will be removed before the merge.

---

## Zephyr RTOS support: pjproject as a Zephyr module, two samples, the test suites, CI

This is the third part of bringing PJSIP to Zephyr RTOS, after #5314 (portability fixes for minimal POSIX C libraries) and #5316 (CMake cross-compiling for generic targets). With this PR pjproject builds as a Zephyr module, a SIP user agent on pjsip and one on pjsua-lib run on a Cortex-M33 board, the five test suites pass there, and a CI workflow keeps it so. This description says what the PR does; [README_Zephyr.md](README_Zephyr.md) says what the result can do and how to use it, with the measurements.

### Objective

Run a SIP endpoint on a small microcontroller: an STM32H563 class device with a Cortex-M33 at 250 MHz, 640 KB of RAM and 2 MB of flash, as a statically linked firmware image with no MMU and no filesystem. The endpoint registers to a PBX, carries a few concurrent G.711 or G.722 calls with SDES-SRTP, and speaks SIP over TLS through Zephyr's Mbed TLS. No video, no echo canceller, no ICE.

The port had to be reusable: the Zephyr specific code is kept to a minimum and lives in a layout that a port to another RTOS can follow, and everything that could be fixed generically was fixed generically.

### What the target looks like

Zephyr provides a POSIX layer over its own kernel and network stack, and the port runs on that layer: BSD sockets, `select()`, `clock_gettime()` and, optionally, pthreads. It differs from a Unix in ways that matter to pjlib:

- There is no `getifaddrs()`; interfaces are enumerated through Zephyr's `net_if` API.
- pthread keys exist only for threads created with `pthread_create()`, so `pthread_setspecific()` fails on the main thread, where `pj_init()` is normally called.
- `sockaddr_in` is 8 bytes rather than 16, `inet_pton()` zeroes its output on failure, `bind()` writes the chosen port back into the caller's address, a failed `connect()` is flagged in both the write and the exception set of `select()`, and `socketpair()` only exists for AF_UNIX.
- The C library's `rand()` is unseeded, and `exit()` does not end the program.
- Everything is compiled with `-std=c99`, which defines `__STRICT_ANSI__`.
- Memory is scarce: on the 640 KB target the samples run with a 144 to 312 KB heap arena, the test suites with 250 to 500 KB, and `CONFIG_HW_STACK_PROTECTION` faults on any stack overflow instead of letting it pass silently.

### Tested targets

| Target | What ran there |
|---|---|
| `native_sim/native/64`, Zephyr as a Linux process | Both samples; all five suites in both thread modes; CI |
| `mps2/an521/cpu0`, a Cortex-M33 under QEMU limited to 640 KB of RAM | Both samples, all four embedded_ua builds; all five suites in both thread modes; CI builds |
| NUCLEO-H563ZI (STM32H563ZI, Cortex-M33 at 250 MHz, 640 KB of RAM) | Both samples, all four embedded_ua builds; all five suites in both thread modes; the CPU benchmarks |

- On every target embedded_ua registers to Kamailio and completes calls with zero packet loss, single threaded and with pjsip and the media on two pthreads, over UDP and over TLS. On the board each build took an address from DHCP and answered four INVITEs arriving at once, with round trips under 2 ms, a tickless clock within 50 ppm of the host's and the idle thread above 86% of the CPU.
- simple_pjsua registers and completes a G.711 call on every target, one call at a time, which is what it is sized for.
- The one suite that does not run everywhere is the threaded pjsip-test, which needs about 690 KB of RAM.
- Zephyr 4.5.0 is the release CI pins. Zephyr 4.4.2 was checked by hand: everything builds and pjlib-test passes, except the TLS overlay, whose Mbed TLS record-size options have 4.5's names (README section 1 gives the 4.4 setting).

README sections 3 to 6 have the procedures and the results; "Footprint" and "Processing cost" at its top have the numbers.

### New concepts

- **pjproject as a Zephyr module.** `zephyr/module.yml` registers the tree; `zephyr/Kconfig` has the `CONFIG_PJPROJECT_*` options for what the build has to know (threads, TLS, floating point); `zephyr/CMakeLists.txt` drives pjproject's own CMake from Kconfig and defines one `pjproject` link target with every library. Codecs, logging and sizes stay in `config_site.h`, so that each setting has one place.
- **A platform preset the build insists on.** `PJ_CONFIG_ZEPHYR` in `config_site_sample.h` sizes pjsip and pjmedia for a few calls on a 640 KB board, with the reason next to each setting. The module checks that the user's `config_site.h` selects it, since pjlib's CMake would otherwise create an empty one and build with desktop defaults.
- **A target snippet.** `zephyr/snippets/pjproject-target` carries what each target needs beyond Zephyr's board files: the NUCLEO-H563ZI's full 640 KB of RAM, and for QEMU's AN521 and native_sim what makes them stand in for it (the same 640 KB limit, QEMU's SysTick rate, a seeded random generator, the TAP MTU). Applications select it with one line.
- **Settings as `CONFIG_*` macros on every platform.** Both samples read `CONFIG_EMBUA_*` and `CONFIG_SIMPLE_PJSUA_*`; on Zephyr they are Kconfig options, elsewhere `#define`s with defaults, so the same source builds on Linux, Windows, macOS and Zephyr.
- **Memory sized by measurement.** The samples' Kconfig files set the heap arena, the kernel heap and the packet pools per feature set to a measured peak plus 5%, with the measurement in the comment; the samples log the heaps (`mem` in embedded_ua, every ten seconds in simple_pjsua) so that a user can redo it on another board.
- **Thread-local storage in compiler TLS.** `PJ_THREAD_LOCAL` and `PJ_THREAD_LOCAL_USE_COMPILER_TLS` let pjlib keep its per-thread data in `__thread` storage where pthread keys do not serve the main thread.
- **A test profile.** `pjlib/src/pjlib-test/test_profile.h`, included by every suite, publishes the target's RAM as `PJTEST_TARGET_RAM_KB` and three facts about the host: whether time passes while the CPU is busy, whether there is a route beyond loopback, and a slow-CPU proxy. Each test states what it needs in those terms instead of asking whether the target is small.
- **The suites as Zephyr test applications.** Each library's test sources are an INTERFACE target that both the desktop and the Zephyr build compile; the Zephyr apps take the tests to run from `CONFIG_<SUITE>_TEST_ARGS` and run under twister, with the benchmarks behind `CONFIG_PJMEDIA_TEST_BENCHMARK` and `CONFIG_PJSIP_TEST_BENCHMARK`.
- **A west manifest and CI.** `west.yml` at the repository root makes pjproject a manifest repository, Zephyr pinned with only the modules the targets need; the "CI Zephyr" workflow builds every suite and sample for native_sim and the AN521 and runs pjlib-test on every pull request.
- **A cryptographic `pj_rand()` where the platform has one.** `PJ_RAND_IS_CSPRNG` says so; on Zephyr `pj_rand()` draws from the entropy driver, and SRTP's SDES keys come from it without a warning when no SSL library is built.

### Changes

- **The Zephyr module** (`zephyr/`):
  - `module.yml`, with a build dependency on Zephyr's Mbed TLS module so that pjproject is configured after it whatever the manifest order, and a `Kconfig` with the `CONFIG_PJPROJECT_*` options.
  - A `CMakeLists.txt` that checks that the tree's `config_site.h` selects the Zephyr preset, drives pjproject's own per-library CMake files with options derived from Kconfig, applies Zephyr's include paths and flags to every library, and defines the `pjproject` link target.
  - pjmedia-videodev gets the flags too, so that pjsua-lib's dependency on it compiles; with `PJMEDIA_HAS_VIDEO` off in the preset its files are empty, and nothing in the port links it.
  - `cmake/MbedTLSConfig.cmake` lets pjlib's Mbed TLS backend find Zephyr's Mbed TLS.

- **The platform layer in pjlib**:
  - `pj/compat/os_zephyr.h` describes the platform from Kconfig (available headers and socket functions, threads, TCP, TLS, no IPv6, per-thread data in compiler TLS, a cryptographic `pj_rand()`), selected by `pj/config.h` when `__ZEPHYR__` is defined.
  - `pj/ip_helper_zephyr.c` enumerates interfaces and routes through `net_if`.
  - The `PJ_CONFIG_ZEPHYR` preset in `config_site_sample.h`.
  - `compat/rand.h` uses Zephyr's entropy-fed generator.

- **Thread-local storage in compiler TLS**:
  - A new `PJ_THREAD_LOCAL` keyword in the compiler headers and a `PJ_THREAD_LOCAL_USE_COMPILER_TLS` option make `pj_thread_local_*()` in `os_core_unix.c` use a `__thread` slot array instead of pthread keys.
  - Slots carry a generation number, in a per-thread array that makes the TLS block about 270 bytes, so that a freed and reallocated slot reads NULL in every thread, as a new pthread key would. The same array serves the no-threads build.
  - Thread attributes are now kept in the thread record until join or destroy, because Zephyr allocates the thread's stack in `pthread_attr_init()` and refuses to destroy the attributes while the thread runs.

- **Generic fixes exposed by the target**:
  - `pj_sock_bind()` passes a copy of the address; `pj_inet_aton()` restores `INADDR_NONE` on failure.
  - The socketpair fallback binds the client socket explicitly, and `pj_sock_socketpair()` falls back for every "not supported" errno.
  - The select ioqueue queues one event for a failed connect, since the exception event was a no-op that still counted in the poll result.
  - `__attribute__` based macros are defined under `__STRICT_ANSI__` too; ARM endianness and the generic ARM target name are derived from compiler macros; `pj_jmp_buf` can be sized by the platform header.
  - A few macro collisions and float promotion warnings in pjlib, pjlib-util and pjmedia are fixed.

- **Memory per message and per object**:
  - `pjsip_tx_data_create()` allocates the packet buffer right after the `pjsip_tx_data` struct, while the pool's first block is still empty; allocated after the message's headers, as before, it never fit the remainder and every transmit buffer grew by a block for it.
  - The group lock's private pool size is now `PJ_GRP_LOCK_POOL_SIZE` (default 512, unchanged).
  - `pj_ioqueue_set_concurrency()` accepts disabling concurrency without `PJ_IOQUEUE_HAS_SAFE_UNREG` when `PJ_HAS_THREADS` is 0, since there is no other thread to be safe from; the Zephyr preset ties safe unregistration to threads, so the no-thread build allocates ioqueue keys on registration from the registering object's pool instead of pre-allocating `PJ_IOQUEUE_MAX_HANDLES` of them.
  - The preset sets small limits for modules, resolved addresses, packet length, media events, group lock pools, header values and registered codecs (the last two gained `#ifndef` guards), and the sample sets its jitter buffer to 200 ms (`CONFIG_EMBUA_JB_SIZE`) instead of pjmedia's 500.
  - Measured on the AN521 with SIP over TLS and one G.711 call, the heap in use went from 167 KB to 133 KB, and a MESSAGE transaction's transmit buffer from 7168 to 4096 bytes.

- **embedded_ua on Zephyr** (`zephyr/samples/embedded_ua/`):
  - The sample in `pjsip-apps/src/samples/embedded_ua.c`, with its build settings renamed from `EMBUA_*` to `CONFIG_EMBUA_*` and all of them in the sample's Kconfig (the on/off ones as ints, so that they are always defined), builds as a Zephyr sample for the NUCLEO-H563ZI and its two stand-ins, with threads and TLS overlays and a `run-qemu.sh` that attaches the AN521's Ethernet to the TAP.
  - `prj.conf` is written for hardware (DHCP, the board's random number generator); the emulated boards' static TAP address is in the sample's `boards/` files.
  - Two settings are new: `CONFIG_EMBUA_LOG_LEVEL` (default 3, where it used to hard-code 5) because writing every SIP message to the console from the single thread stalls the media on a slow UART, and `CONFIG_EMBUA_JB_SIZE`.
  - The sample's Kconfig sizes the heap arena, the kernel heap and the transmit pool per feature set, from measurements on the AN521 with four calls arriving at once plus 5%; the NUCLEO-H563ZI peaks at the same bytes. The transmit pool matters over TLS: TCP keeps every segment until it is acknowledged, and with too few buffers Zephyr's TCP only logs the failed allocation while the stream that reaches the server is corrupt.
  - `prj.conf` sizes the receive pool for four INVITEs at once, which QEMU's Ethernet model needs because it wedges once a buffer runs out (the board took the same burst with less than half), and raises the IPv4 MTU from Zephyr's 576 to 1500, which the snippet caps at 1400 on the emulated boards.
  - The `pjproject-target` snippet, registered by the module and selected by the samples and the tests: the NUCLEO-H563ZI's full 640 KB of RAM through a devicetree overlay, the same limit on the AN521, SysTick at the 20 MHz of QEMU's model where Zephyr's board files declare the hardware's 25 MHz (without it the guest's clock runs at 80% of real time), native_sim's entropy source seeded from the host so that runs do not repeat Call-IDs, and SLIP turned off on the AN521 for Zephyr 4.4 (the samples' own AN521 files select QEMU's Ethernet there).

- **pjlib-test on Zephyr** (`zephyr/tests/pjlib/`):
  - pjlib's test suite builds as a Zephyr test application and runs under twister on both emulated targets, single threaded and with threads, and on the board. The test sources became an INTERFACE target in `pjlib/CMakeLists.txt` so that both builds compile the same list.
  - `test_profile.h`, which the other suites include too, scales test sizes to `PJTEST_TARGET_RAM_KB` (`CONFIG_SRAM_SIZE` on Zephyr, 1 GB elsewhere) with a `PJTEST_RAM_SCALE` helper, and defines `PJTEST_TIME_PASSES_WHEN_BUSY` (0 on native_sim), `PJTEST_HAS_OUTSIDE_ROUTE` (0 on Zephyr) and `PJTEST_SLOW_CPU` (the RAM standing in for the CPU speed, which sets the benchmarks' loop counts and excludes the benchmarks by default).
  - The tests that run many threads take one thread per 160 KB of RAM (4 on a 640 KB board, 16 on a desktop), which let the kernel heap for their stacks drop from 160 KB to 64 KB; the timer test, pjsip-test's pool capacity and its transmit-buffer counts scale the same way, and pjnath-test's trickle ICE test and pjmedia-test's hundred-call benchmarks are in from 1 MB and 2 MB of RAM. At 640 KB every value is what the earlier small-footprint profile gave.
  - The ioqueue stress test's 1.2 MB configuration table became a small const table plus one reused descriptor.
  - Tests that could not run or hang on the target were fixed rather than skipped: the IOCP unregister test now stops its worker on the error path, the stress test accepts an OS without `SO_SNDBUF`, and the TLS slot semantics above are covered by an extended `thread_local_test`.
  - Two error paths that the small board exposed are fixed for every platform: `pj_test_create_text_runner()` destroyed worker threads that were still suspended when a later one failed to start, and the parallel receive test left its threads polling an ioqueue it then destroyed.

- **pjmedia-test on Zephyr** (`zephyr/tests/pjmedia/`):
  - The same arrangement as pjlib-test, with the suite's sources as a `pjmedia-test-sources` INTERFACE target in `pjmedia/CMakeLists.txt`, and a Kconfig for the tests to run (`CONFIG_PJMEDIA_TEST_ARGS`) and for building the MIPS benchmark that `test_profile.h` otherwise leaves out of a slow CPU (`CONFIG_PJMEDIA_TEST_BENCHMARK`).
  - The event manager itself creates no worker thread in a build without threads and delivers posted events synchronously then, which is what `pj_thread_create()` requires there.
  - `mips_test` takes the CPU clock from Zephyr, leaves the hundred-stream conference entries out below 2 MB of RAM and the resampler entries out when no resampler is built.
  - Its streams now set the receive payload type as well as the transmit one: without it every packet of a codec other than PCMU was rejected as a bad payload type and the stream rows measured the encoder alone, on every platform.
  - On the NUCLEO-H563ZI it puts a G.711 call with SRTP at 2% of the core and a G.722 call at 13% at `-O2` (README "Processing cost").

- **pjlib-util-test, pjnath-test and pjsip-test on Zephyr** (`zephyr/tests/{pjlib-util,pjnath,pjsip}/`):
  - The same arrangement for the remaining suites, each with its sources as an INTERFACE target in the library's `CMakeLists.txt`, a `CONFIG_<SUITE>_TEST_ARGS` Kconfig, a shared `zephyr/tests/overlay-threads.conf`, and twister entries for native_sim in both thread modes and an AN521 build.
  - All five suites pass on native_sim, on the AN521 under QEMU and on the NUCLEO-H563ZI, single threaded and with threads, except the threaded pjsip-test, which needs about 690 KB of RAM (README section 5 has the table).
  - What the small, loopback-only targets turned up is fixed in the tests, for every platform:
    - Zephyr drops a datagram whose source and destination endpoints are the same, so pjsip-test's round-trip, transaction and registration tests now send between two UDP transports.
    - Connections to an outside address are refused at once on such a host, so the TCP selector test connects to a listener of its own and the transaction-destroy cases that need a connection in progress run only with `PJTEST_HAS_OUTSIDE_ROUTE`.
    - The ICE test set its loopback host candidates on the legacy STUN setting only, which left the server-reflexive variants without a base candidate (`PJNATH_EICENOHOSTCAND`) on a loopback-only host.
    - The TURN test server kept reading a connection the client had closed, a busy wait that never ends where the simulated clock stands still; pjsip-test's multithreaded round-trip benchmark stops for the same reason and runs only with `PJTEST_TIME_PASSES_WHEN_BUSY`.
    - The STUN authentication test released neither its client pools nor the timers its sessions still held.
    - The SHA1, HTTP, WebSocket, DNS round-robin, transmit-buffer and ACK tests were sized for a desktop (a million-byte block, an 8 MB body, 64 KB buffers on an 8 KB thread stack, eight hundred queries in one pool) and now fit a few hundred KB.
  - Three stay out on a small board, with the reason in the `test.h` concerned: pjnath-test's trickle ICE test (448 KB of heap, in from 1 MB of RAM, where the Zephyr test's Kconfig grows the arena for it) and its fifty concurrency rounds (eight threads polling one select ioqueue make Zephyr's `select()` return at once), and the pjsua tests, which are not built for Zephyr at all since pjsua is not part of the port.
  - `PJSIP_MAX_MODULE` in the Zephyr preset became overridable for the test app, and the WebSocket buffer sizes in `websock.h` became overridable so that the preset can set them: 8 KB buffers and 16 KB messages instead of 384 KB per endpoint.
  - Fixed for every platform along the way: `srv_failover_test` wrote past `PJSIP_MAX_RESOLVED_ADDRESSES` when that is below three (it is two in the Zephyr preset, now behind `#ifndef`; the test asserts at compile time that it is at least six, its three servers with a TCP copy of each, and the Zephyr test app sets six), `msg_logger.c` printed a `pj_ssize_t` with `%ld`, and two format strings in pjnath-test and pjlib-util-test printed `int` arguments with `%lu` and `%ld`.
  - pjsip-test's Kconfig has `CONFIG_PJSIP_TEST_BENCHMARK`, the counterpart of pjmedia-test's, for the URI and message parsing benchmarks that `test_profile.h` leaves out of a slow CPU; the README's "Processing cost" section has the figures from the NUCLEO-H563ZI.
  - On a slow CPU the benchmarks run 2,000 rounds instead of 40,000 and 100,000; their per-second figures are computed with nanosecond resolution now (a Cortex-M prints a URI in 4 µs, which the microsecond arithmetic rounded to a quarter); the message benchmark no longer divides by zero where the clock stood still; and the transaction benchmark depends on the loop transport it runs over (`PJSIP_TEST_HAS_LOOP`), as the other loop-transport tests do.

- **pjsua-lib and simple_pjsua on Zephyr** (`zephyr/samples/simple_pjsua/`):
  - pjsua-lib is part of the module's `pjproject` link target; the libraries are static archives, so an application that does not call it carries nothing of it (embedded_ua's image is unchanged).
  - `simple_pjsua.c` takes its settings as `CONFIG_SIMPLE_PJSUA_*` macros on every platform, the sample's Kconfig options on Zephyr, has a Zephyr `main()` since there is no command line or stdin, and can run on pjsua's null sound device, which the port needs as it has no sound device driver.
  - The Zephyr sample builds with threads: pjsua's worker, which also polls RTP, pjmedia's event thread and the null sound device's clock, with `CONFIG_HW_STACK_PROTECTION` on the boards. Its Kconfig sizes the heaps from measurements on the AN521 with one G.711 call; the image, what pjsua-lib costs against embedded_ua, and the heap at each stage of a call are in README "Footprint".
  - The preset sizes pjsua-lib's static state from 195 KB on a Cortex-M at the desktop defaults (159 KB of it the 256 buddies) down to 9 KB, with limits of 1 account, 2 buddies, 2 players and 2 recorders, and leaves the echo canceller off since none is built. Built with `CONFIG_SYS_HEAP_RUNTIME_STATS`, the sample logs both heaps' use and peak every ten seconds, as embedded_ua's "mem" command does.
  - The sample sets `PJSIP_MAX_MODULE` 32 for its build, as pjsua-lib registers 20 modules; the preset keeps 16, for an application on pjsip, the recommended way on a small target. Two preset settings came from it: `PJMEDIA_CONF_THREADS` 1 (the parallel conference bridge would start three more threads on a single core) and `PJSUA_DEFAULT_CLOCK_RATE` 8000 (the bridge runs at G.711's rate as the preset builds no resampler).
  - Verified on native_sim, on the AN521 under QEMU and on the NUCLEO-H563ZI against Kamailio: registration, a G.711 call from pjsua with no loss either way, and the hangup. Tested with one call at a time, which is what the sample is sized for.

- **CI Zephyr** (`.github/workflows/ci-zephyr.yml`, `west.yml`):
  - A workflow of its own, on every pull request that touches the libraries, the samples or the Zephyr files: every test suite and sample built for native_sim and for the AN521, where the link fails when an image outgrows the 640 KB of RAM, and pjlib-test run on native_sim single threaded and with threads, four jobs in parallel. The other suites are built but not run: a library change breaks the build or the footprint on Zephyr, not a suite that passes on Linux, and the builds take a fraction of the time.
  - `west.yml` at the repository root makes pjproject a west manifest repository: Zephyr pinned to 4.5.0 with only the five modules the targets need, 810 MB instead of all of Zephyr. CI builds from it through `zephyrproject-rtos/action-zephyr-setup`, which caches the SDK's Arm toolchain and ccache between runs, and `west init -m https://github.com/pjsip/pjproject` gives a user the same workspace.
  - Found by the CI workspace, where pjproject is the manifest repository and so the first module: the build dependency on mbedtls in `module.yml` (pjlib's `find_package(MbedTLS)` resolves to the shim in `zephyr/cmake/`, whose `MbedTLS::` targets clashed with the aliases the module creates later); the samples' twister tag is `pjsip` alone, as Zephyr 4.5's twister rejects the space-separated `net pjsip` and the `net` tag matched the AN521's ignore list, so twister had never built the samples there; and the two format strings above, which twister's warnings-as-errors rejected on 32-bit.
  - Found by Zephyr 4.4.2: the snippet's `description` key is gone, as 4.4's schema rejects it, and 4.4 defaults a QEMU target's networking to SLIP over a UART pipe the AN521 has not got, so the samples' AN521 files select QEMU's Ethernet and the snippet turns the SLIP TAP off for the loopback-only tests, both no-ops on 4.5.

- **Documentation**:
  - `README_Zephyr.md` at the tree root is the full guide, from installing Zephyr to running on native_sim, QEMU and real hardware, using pjproject in an application, configuration, how the port is put together, known limitations and troubleshooting, with the footprint and processing-cost measurements at the top. docs.pjsip.org will link to or copy it. The main README links to it and carries the CI Zephyr badge.

- **SIP over TLS** (`zephyr/samples/embedded_ua/overlay-tls.conf`):
  - pjlib's Mbed TLS backend builds unchanged against Zephyr's Mbed TLS 4.1, found through `zephyr/cmake/MbedTLSConfig.cmake`; the one change in the backend is that it logs the library version from `MBEDTLS_VERSION_STRING_FULL` instead of calling a function that needs `MBEDTLS_VERSION_C`.
  - The overlay configures Mbed TLS as a TLS 1.2 client with one ciphersuite, adds to the heap arena for its contexts, and names a TF-PSA-Crypto user configuration that turns on Mbed TLS's pthread threading layer in threaded builds, since PSA Crypto is then called from two threads and Zephyr has no Kconfig option for it.
  - All three targets register over TLS to Kamailio and complete calls; the image sizes are in README "Footprint".

### Compatibility

Two changes can break an existing build:

- **CMake: `PJMEDIA_HAS_SRTP` and `PJMEDIA_RESAMPLE_IMP` are no longer compile definitions.** pjmedia's CMake used to put them on the command line of pjmedia and of everything that links it (PUBLIC definitions), which made them impossible to set from `config_site.h` without a redefinition. They now go into the generated `pjmedia/config_auto.h` behind `#ifndef`, like the codec macros, so `config_site.h` wins. A consumer that read them from pjmedia's `INTERFACE_COMPILE_DEFINITIONS`, or that compiled code against them without including pjmedia's headers, no longer sees them; and `PJMEDIA_WITH_SRTP=OFF` or `PJMEDIA_WITH_RESAMPLE=none` no longer overrides a consumer that defines the macro itself. The GNU make and Visual Studio builds never had the definition and are unaffected.
- **embedded_ua's build settings are renamed and renumbered.** `EMBUA_*` became `CONFIG_EMBUA_*`, so a `-DEMBUA_...` on a command line is silently ignored. The values of `CONFIG_EMBUA_AUDIO_CODEC` (1 G.711, 2 G.722, 0 none) and `CONFIG_EMBUA_SIP_TRANSPORT` (1 UDP, 2 TLS) changed too; the names `CODEC_G711`, `CODEC_G722`, `SIP_UDP` and `SIP_TLS` still work, and the listening ports are unchanged.

Behaviour changes that existing code should not notice, listed for review:

- `PJ_ATTR_NORETURN` and `PJ_ATTR_MAY_ALIAS` in `cc_gcc.h` and `cc_clang.h` are now defined under strict ANSI too (`-std=c89`, `-std=c99`, `-ansi`), where they used to be empty. `__attribute__` works under any `-std`; the empty definitions made `exception.c` warn about a noreturn function returning.
- `pj_sock_bind()` binds through a copy, so an OS that writes the chosen port back into the address (Zephyr does) no longer modifies the caller's; use `pj_sock_getsockname()` for the port, as on Linux all along. The length is now also checked against `sizeof(pj_sockaddr)`.
- `pj_inet_aton()` sets the output to `PJ_INADDR_NONE` when it fails, instead of leaving whatever `inet_pton()` left.
- The select ioqueue does not dispatch an exception event for a descriptor that was also writable in the same poll: a failed `connect()` that an OS flags in both sets completes once and counts once. Linux never sets both; Windows sets only the exception set.
- `pj_thread_create()` initialises pthread attributes only when a stack size or address is given, and keeps them for the thread's lifetime (Zephyr frees the stack when the attributes are destroyed, which it refuses while the thread runs). A thread with the default stack is created with a null attribute pointer.
- Thread-local storage in the slot-array implementation (the no-threads build, and the new compiler-TLS option) now behaves like pthread keys: a slot that is freed and allocated again reads NULL in every thread, where the stale value used to remain.
- `pj_rand()` draws from the platform's cryptographic generator where `PJ_RAND_IS_CSPRNG` says there is one (Zephyr's entropy driver); elsewhere it is unchanged. SRTP's SDES key generation uses it without the "simple random generator" warning on such a platform.
- The test suites: every suite's `test.h` includes the new `test_profile.h`, which scales test sizes to the target's RAM and gives `PJ_EXCLUDE_BENCHMARK_TESTS` a default (1 on a target with less than 2 MB of RAM or where the clock stands still while the CPU is busy, 0 elsewhere); a build that defines it still wins. pjlib-test's sources are a new CMake INTERFACE target, `pjlib-test-sources`; the `pjlib-test` executable is unchanged.
- `sip_config.h`: the dialog and user-agent pool sizes (`PJSIP_POOL_LEN_DIALOG`, `PJSIP_POOL_INC_DIALOG`, `PJSIP_POOL_LEN_USER_AGENT`, `PJSIP_POOL_INC_USER_AGENT`) gained `#ifndef` guards, so that `config_site.h` can set them like the other pools. Defaults unchanged.
- `pjsip_tx_data_create()` allocates the `PJSIP_MAX_PKT_LEN` packet buffer at creation rather than on the first `pjsip_tx_data_encode()`; the same memory from the same pool, earlier. `pjsip_tx_data_encode()` still allocates it if absent.
- `pj_ioqueue_set_concurrency(key, PJ_FALSE)` no longer fails with `PJ_EINVAL` in a build with `PJ_HAS_THREADS` 0 and `PJ_IOQUEUE_HAS_SAFE_UNREG` 0; it used to, which made every media UDP transport fail to create in such a build.
- `PJ_GRP_LOCK_POOL_SIZE` replaces the hard-coded 512 for a group lock's pool, with the same default.
- The generic array header parser stops storing values at `PJSIP_GENERIC_ARRAY_MAX_COUNT` instead of writing past the array; a header with more values than that, 32 by default, used to overflow it.
- The select, epoll and kqueue ioqueues drain pending write callbacks on unregistration (#4864, #4878) in a build without `PJ_IOQUEUE_HAS_SAFE_UNREG` too; that path used to drop them, which pjlib-test's activesock test catches. In the kqueue backend the key is now unlocked before its group lock reference is dropped on that path, as on the other path and in the other backends.
- Additions with no effect unless used: the `PJ_THREAD_LOCAL` keyword macro in the compiler headers, the `PJ_THREAD_LOCAL_USE_COMPILER_TLS` option (default 0), `PJ_RAND_IS_CSPRNG` (default 0), `pj/compat/os_zephyr.h` selected only when `__ZEPHYR__` is defined, the `PJ_CONFIG_ZEPHYR` preset, `west.yml` at the repository root, and the Mbed TLS backend no longer requiring `MBEDTLS_VERSION_C`.

### Not in this PR

Further footprint reduction, which needs code changes (hard-coded pool sizes in the invite session and SRTP transport, the stream's copy of its info, fixed SDP arrays, the select ioqueue's up-front keys in the threaded build, the WSOLA history every codec instance carries), the IPv6 socket address layout, a `pj_csrand_bytes()` API to replace the cryptographic `pj_rand()` route, and a second board, the 1 MB STM32H753. These follow in separate PRs. README_Zephyr.md has the measured RAM per call with the preset's sizing.
