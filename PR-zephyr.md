# Zephyr RTOS support (PR description draft)

This file is the draft of the pull request description for the `zephyr-support` branch. It is tracked here so that it can be reviewed with the code, and will be removed before the merge.

---

## Zephyr RTOS support: pjproject as a Zephyr module, embedded_ua sample, the test suites on twister

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

Tested targets are `native_sim/native/64`, which runs Zephyr as a Linux process, `mps2/an521/cpu0`, a Cortex-M33 under QEMU limited to 640 KB of RAM to match the STM32H563, and the NUCLEO-H563ZI itself. All three register to a SIP server and complete calls with zero packet loss, single threaded and with pjsip and the media on two pthreads; on the board each of the four builds (UDP or TLS, with or without threads) took an address from DHCP and answered four INVITEs arriving at once, with zero loss, round trips under 2 ms, a tickless clock within 50 ppm of the host's and the idle thread above 86% of the CPU. The heap peaks on the board match the emulator's to within a few hundred bytes, which is what the sample's Kconfig sizes for (README section 6 and "Footprint").

### Changes

- **The Zephyr module** (`zephyr/`):
  - `module.yml`, and a `Kconfig` with the `CONFIG_PJPROJECT_*` options for what the build has to know: threads, TLS, floating point. Codecs, logging and queue sizes stay in `config_site.h`, so that each setting has one place.
  - A `CMakeLists.txt` that checks that the tree's `config_site.h` selects the Zephyr preset (nothing is generated: the user writes `pjlib/include/pj/config_site.h` as on every other platform), drives pjproject's own per-library CMake files with options derived from Kconfig, applies Zephyr's include paths and flags to every library, and defines a `pjproject` link target.
  - pjmedia-videodev gets the flags too, so that pjsua-lib's dependency on it compiles; with `PJMEDIA_HAS_VIDEO` off in the preset its files are empty, and nothing in the port links it.
  - `cmake/MbedTLSConfig.cmake` lets pjlib's Mbed TLS backend find Zephyr's Mbed TLS.

- **The platform layer in pjlib**:
  - `pj/compat/os_zephyr.h` describes the platform from Kconfig (available headers and socket functions, threads, TCP, TLS, no IPv6, per-thread data in compiler TLS), selected by `pj/config.h` when `__ZEPHYR__` is defined.
  - `pj/ip_helper_zephyr.c` enumerates interfaces and routes through `net_if`.
  - A `PJ_CONFIG_ZEPHYR` preset in `config_site_sample.h` sizes pjsip and pjmedia for a small endpoint.
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
  - The preset also sets `PJSIP_MAX_MODULE` 16, `PJSIP_MAX_RESOLVED_ADDRESSES` 2, `PJSIP_MAX_PKT_LEN` 2200, `PJMEDIA_EVENT_MAX_EVENTS` 4, `PJ_GRP_LOCK_POOL_SIZE` 256 without threads and 512 with, `PJSIP_GENERIC_ARRAY_MAX_COUNT` 16 and `PJMEDIA_CODEC_MGR_MAX_CODECS` 4 (the last two gained `#ifndef` guards), and the sample sets its jitter buffer to 200 ms (`CONFIG_EMBUA_JB_SIZE`) instead of pjmedia's 500.
  - Measured on the AN521 with SIP over TLS and one G.711 call, the heap in use went from 167 KB to 133 KB, and a MESSAGE transaction's transmit buffer from 7168 to 4096 bytes.

- **embedded_ua on Zephyr** (`zephyr/samples/embedded_ua/`):
  - The sample in `pjsip-apps/src/samples/embedded_ua.c`, with its build settings renamed from `EMBUA_*` to `CONFIG_EMBUA_*` and all of them exposed in the sample's Kconfig (the on/off ones as ints, so that they are always defined), builds as a Zephyr sample for the NUCLEO-H563ZI and, as its stand-ins, native_sim and the an521 board, with a threads overlay and a `run-qemu.sh` that sets up the TAP networking.
  - `prj.conf` is written for hardware (DHCP, the board's random number generator); the emulated boards' static TAP address is in the sample's `boards/` files.
  - Two of the settings are new with this PR: `CONFIG_EMBUA_LOG_LEVEL` (default 3, where it used to hard-code 5) because writing every SIP message to the console from the single thread stalls the media on a slow UART, and `CONFIG_EMBUA_JB_SIZE`.
  - The sample's Kconfig sizes the memory per feature set, from measurements on the AN521 with four calls arriving at once plus 5% for the heap's bookkeeping and fragmentation (the NUCLEO-H563ZI peaks at the same bytes, leaving 10 to 15 KB of each arena at the four-call peak): the heap arena (236 KB over UDP, 257 KB with threads, 293 KB over TLS, 312 KB with both), the kernel heap for the media thread's stack (17 KB) and the transmit pool (48 buffers, 96 over TLS: TCP keeps every segment until it is acknowledged, and with fewer buffers Zephyr's TCP only logs the failed allocation and the stream that reaches the server is corrupt).
  - `prj.conf` sizes the receive pool for four INVITEs at once (96 packets, 384 buffers, which QEMU's Ethernet model needs because it wedges once a buffer runs out; the NUCLEO-H563ZI took the same burst with 40 and 80) and raises the IPv4 MTU from Zephyr's 576 to 1500, which the `pjproject-target` snippet caps at 1400 on the emulated boards to match the TAP workaround for QEMU's LAN9118 model.
  - A `pjproject-target` snippet in `zephyr/snippets`, registered by the module and selected by the sample and the tests, carries what each target needs beyond Zephyr's board files: the NUCLEO-H563ZI's full 640 KB of RAM, and for the emulated boards what makes them stand in for it. It limits the an521 to the same 640 KB, sets SysTick to the 20 MHz of QEMU's model where Zephyr's board files declare the hardware's 25 MHz (without it the guest's clock runs at 80% of real time and the sample sends 40 RTP packets a second instead of 50), and seeds native_sim's entropy source from the host so that runs do not repeat Call-IDs.

- **pjlib-test on Zephyr** (`zephyr/tests/pjlib/`):
  - pjlib's test suite builds as a Zephyr test application and runs under twister on both targets, single threaded and with threads. The test sources became an INTERFACE target in `pjlib/CMakeLists.txt` so that both builds compile the same list.
  - A new `test_profile.h` defines `PJTEST_SMALL_FOOTPRINT` for boards with less than 2 MB of RAM and `PJTEST_TIME_PASSES_WHEN_BUSY`, 0 on native_sim whose simulated clock stands still while the CPU is busy, and derives the benchmark exclusion and the timer test size from them; the other test suites include it too.
  - The ioqueue stress test's 1.2 MB configuration table became a small const table plus one reused descriptor.
  - Tests that could not run or hang on the target were fixed rather than skipped: the IOCP unregister test now stops its worker on the error path, the stress test accepts an OS without `SO_SNDBUF`, and the TLS slot semantics above are covered by an extended `thread_local_test`.
  - The suite also runs on the NUCLEO-H563ZI itself, in both thread modes, with a board file that leaves the Ethernet driver out.
  - `test_profile.h` publishes the target's RAM as `PJTEST_RAM_KB`, and the tests that run many threads size themselves from it (4 on a 640 KB board, 16 elsewhere: the ioqueue stress test, the parallel receive test and the runner's scheduling self-test), which is also what let the kernel heap for their stacks drop from 160 KB to 64 KB.
  - Two error paths that the small board exposed are fixed for every platform: `pj_test_create_text_runner()` destroyed worker threads that were still suspended when a later one failed to start, and the parallel receive test left its threads polling an ioqueue it then destroyed.

- **pjmedia-test on Zephyr** (`zephyr/tests/pjmedia/`):
  - The same arrangement as pjlib-test, with the suite's sources as a `pjmedia-test-sources` INTERFACE target in `pjmedia/CMakeLists.txt`, and a Kconfig for the tests to run (`CONFIG_PJMEDIA_TEST_ARGS`) and for building the MIPS benchmark that `test_profile.h` otherwise leaves out of a small board (`CONFIG_PJMEDIA_TEST_BENCHMARK`).
  - The event manager itself creates no worker thread in a build without threads and delivers posted events synchronously then, which is what `pj_thread_create()` requires there.
  - `mips_test` takes the CPU clock from Zephyr, leaves the hundred-stream conference entries out on a small board and the resampler entries out when no resampler is built.
  - Its streams now set the receive payload type as well as the transmit one: without it every packet of a codec other than PCMU was rejected as a bad payload type and the stream rows measured the encoder alone, on every platform.
  - On the NUCLEO-H563ZI it puts a G.711 call with SRTP at 2% of the core and a G.722 call at 13% at `-O2` (README "Processing cost").

- **pjlib-util-test, pjnath-test and pjsip-test on Zephyr** (`zephyr/tests/{pjlib-util,pjnath,pjsip}/`):
  - The same arrangement for the remaining suites, each with its sources as an INTERFACE target in the library's `CMakeLists.txt`, a `CONFIG_<SUITE>_TEST_ARGS` Kconfig, a shared `zephyr/tests/overlay-threads.conf`, and twister entries for native_sim in both thread modes and an AN521 build.
  - All five suites pass on native_sim, on the AN521 under QEMU and on the NUCLEO-H563ZI, single threaded and with threads, except the threaded pjsip-test, which needs about 690 KB of RAM (README section 5 has the table).
  - What the small, loopback-only targets turned up is fixed in the tests, for every platform:
    - Zephyr drops a datagram whose source and destination endpoints are the same, so pjsip-test's round-trip, transaction and registration tests now send between two UDP transports.
    - Connections to an outside address are refused at once on such a host, so the TCP selector test connects to a listener of its own and the transaction-destroy cases that need a connection in progress run only with `PJTEST_HAS_OUTSIDE_ROUTE` (new in `test_profile.h`, 0 on Zephyr).
    - The ICE test set its loopback host candidates on the legacy STUN setting only, which left the server-reflexive variants without a base candidate (`PJNATH_EICENOHOSTCAND`) on a loopback-only host.
    - The TURN test server kept reading a connection the client had closed, a busy wait that never ends where the simulated clock stands still; pjsip-test's multithreaded round-trip benchmark stops for the same reason and runs only with `PJTEST_TIME_PASSES_WHEN_BUSY`.
    - The STUN authentication test released neither its client pools nor the timers its sessions still held.
    - The SHA1, HTTP, WebSocket, DNS round-robin, transmit-buffer and ACK tests were sized for a desktop (a million-byte block, an 8 MB body, 64 KB buffers on an 8 KB thread stack, eight hundred queries in one pool) and now fit a few hundred KB, with `PJTEST_SMALL_FOOTPRINT` scaling the two benchmark-like ones.
  - Three stay out on a small board, with the reason in the `test.h` concerned: pjnath-test's trickle ICE test (448 KB of heap) and its fifty concurrency rounds (eight threads polling one select ioqueue make Zephyr's `select()` return at once), and the pjsua tests, which are not built for Zephyr at all since pjsua is not part of the port.
  - `PJSIP_MAX_MODULE` in the Zephyr preset became overridable for the test app, and the WebSocket buffer sizes in `websock.h` became overridable so that the preset can set them: 8 KB buffers and 16 KB messages instead of 384 KB per endpoint.
  - Fixed for every platform along the way: `srv_failover_test` wrote past `PJSIP_MAX_RESOLVED_ADDRESSES` when that is below three (it is two in the Zephyr preset, now behind `#ifndef`; the test asserts at compile time that it is at least six, its three servers with a TCP copy of each, and the Zephyr test app sets six), and `msg_logger.c` printed a `pj_ssize_t` with `%ld`.
  - pjsip-test's Kconfig has `CONFIG_PJSIP_TEST_BENCHMARK`, the counterpart of pjmedia-test's, for the URI and message parsing benchmarks that `test_profile.h` leaves out of a small board; the README's "Processing cost" section has the figures from the NUCLEO-H563ZI.
  - On a small board the benchmarks run 2,000 rounds instead of 40,000 and 100,000; their per-second figures are computed with nanosecond resolution now (a Cortex-M prints a URI in 4 µs, which the microsecond arithmetic rounded to a quarter); the message benchmark no longer divides by zero where the clock stood still; and the transaction benchmark depends on the loop transport it runs over (`PJSIP_TEST_HAS_LOOP`), as the other loop-transport tests do.

- **pjsua-lib and simple_pjsua on Zephyr** (`zephyr/samples/simple_pjsua/`):
  - pjsua-lib is part of the module's `pjproject` link target; the libraries are static archives, so an application that does not call it carries nothing of it (embedded_ua's image is unchanged).
  - `simple_pjsua.c` takes its settings as `CONFIG_SIMPLE_PJSUA_*` macros on every platform, the sample's Kconfig options on Zephyr (domain, user name, password, port, calls at once, log level, the null sound device and a URL to call at start), has a Zephyr `main()` since there is no command line or stdin, and can run on pjsua's null sound device, which the port needs as it has no sound device driver.
  - The Zephyr sample builds with threads (pjsua's worker, which also polls RTP, pjmedia's event thread and the null sound device's clock, 20 KB of stack each as the worker uses 17.4 KB at its deepest, with `CONFIG_HW_STACK_PROTECTION`) and its Kconfig sizes the heaps from measurements on the AN521 with one G.711 call: a 144 KB arena (138 KB peak at the call's teardown) and a 64 KB kernel heap. The image is 626 KB of flash and 368 KB of RAM on the NUCLEO-H563ZI (609 KB and 357 KB on the AN521), against embedded_ua's 368 KB and 422 KB over UDP with threads: 258 KB more flash, and less RAM only because it is sized for one call. The heap arena holds 54 KB registered and idle, 119 KB with one call up (peak 135 KB), 138 KB peak at the hangup; the figures are in README "Footprint".
  - The preset sizes pjsua-lib's static state, 195 KB on a Cortex-M at the desktop defaults (159 KB of it the 256 buddies), down to 9 KB: `PJSUA_MAX_ACC` 1, `PJSUA_MAX_BUDDIES` 2, `PJSUA_MAX_PLAYERS` and `PJSUA_MAX_RECORDERS` 2, `PJSUA_MAX_CONF_PORTS` derived from those, and `PJSUA_DEFAULT_EC_TAIL_LEN` 0 since no echo canceller is built. Built with `CONFIG_SYS_HEAP_RUNTIME_STATS`, the sample logs both heaps' use and peak every ten seconds, as embedded_ua's "mem" command does.
  - The sample sets `PJSIP_MAX_MODULE` 32 for its build, as pjsua-lib registers 20 modules; the preset keeps 16, for an application on pjsip, the recommended way on a small target. Two preset settings came from it: `PJMEDIA_CONF_THREADS` 1 (the parallel conference bridge would start three more threads on a single core) and `PJSUA_DEFAULT_CLOCK_RATE` 8000 (the bridge runs at G.711's rate as the preset builds no resampler).
  - Verified on native_sim, on the AN521 under QEMU and on the NUCLEO-H563ZI against Kamailio: registration, a G.711 call from pjsua with no loss either way, and the hangup. Tested with one call at a time, which is what the sample is sized for.

- **Documentation**:
  - `README_Zephyr.md` at the tree root is the full guide, from installing Zephyr to running on native_sim, QEMU and real hardware, using pjproject in an application, configuration, how the port is put together, known limitations and troubleshooting. docs.pjsip.org will link to or copy it.

- **SIP over TLS** (`zephyr/samples/embedded_ua/overlay-tls.conf`):
  - pjlib's Mbed TLS backend builds unchanged against Zephyr's Mbed TLS 4.1, found through `zephyr/cmake/MbedTLSConfig.cmake`; the one change in the backend is that it logs the library version from `MBEDTLS_VERSION_STRING_FULL` instead of calling a function that needs `MBEDTLS_VERSION_C`.
  - The overlay configures Mbed TLS as a TLS 1.2 client with one ciphersuite, adds 64 KB to the heap arena for its contexts, and names a TF-PSA-Crypto user configuration that turns on Mbed TLS's pthread threading layer in threaded builds, since PSA Crypto is then called from two threads and Zephyr has no Kconfig option for it.
  - Both targets register over TLS to Kamailio and complete calls; the an521 image is 452 KB of flash and 461 KB of RAM, 528 KB with threads.

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
- The test suites: every suite's `test.h` includes the new `test_profile.h`, which gives `PJ_EXCLUDE_BENCHMARK_TESTS` a default (1 on a Zephyr board with less than 2 MB of RAM or where the clock stands still while the CPU is busy, 0 elsewhere); a build that defines it still wins. pjlib-test's sources are a new CMake INTERFACE target, `pjlib-test-sources`; the `pjlib-test` executable is unchanged.
- `sip_config.h`: the dialog and user-agent pool sizes (`PJSIP_POOL_LEN_DIALOG`, `PJSIP_POOL_INC_DIALOG`, `PJSIP_POOL_LEN_USER_AGENT`, `PJSIP_POOL_INC_USER_AGENT`) gained `#ifndef` guards, so that `config_site.h` can set them like the other pools. Defaults unchanged.
- `pjsip_tx_data_create()` allocates the `PJSIP_MAX_PKT_LEN` packet buffer at creation rather than on the first `pjsip_tx_data_encode()`; the same memory from the same pool, earlier. `pjsip_tx_data_encode()` still allocates it if absent.
- `pj_ioqueue_set_concurrency(key, PJ_FALSE)` no longer fails with `PJ_EINVAL` in a build with `PJ_HAS_THREADS` 0 and `PJ_IOQUEUE_HAS_SAFE_UNREG` 0; it used to, which made every media UDP transport fail to create in such a build.
- `PJ_GRP_LOCK_POOL_SIZE` replaces the hard-coded 512 for a group lock's pool, with the same default.
- The generic array header parser stops storing values at `PJSIP_GENERIC_ARRAY_MAX_COUNT` instead of writing past the array; a header with more values than that, 32 by default, used to overflow it.
- The select, epoll and kqueue ioqueues drain pending write callbacks on unregistration (#4864, #4878) in a build without `PJ_IOQUEUE_HAS_SAFE_UNREG` too; that path used to drop them, which pjlib-test's activesock test catches. In the kqueue backend the key is now unlocked before its group lock reference is dropped on that path, as on the other path and in the other backends.
- Additions with no effect unless used: the `PJ_THREAD_LOCAL` keyword macro in the compiler headers, the `PJ_THREAD_LOCAL_USE_COMPILER_TLS` option (default 0), `pj/compat/os_zephyr.h` selected only when `__ZEPHYR__` is defined, the `PJ_CONFIG_ZEPHYR` preset, and the Mbed TLS backend no longer requiring `MBEDTLS_VERSION_C`.

### Not in this PR

Further footprint reduction, which needs code changes (hard-coded pool sizes in the invite session and SRTP transport, the stream's copy of its info, fixed SDP arrays, the select ioqueue's up-front keys in the threaded build, the WSOLA history every codec instance carries), and the IPv6 socket address layout. These follow in separate PRs. README_Zephyr.md has the measured RAM per call with the preset's sizing.
