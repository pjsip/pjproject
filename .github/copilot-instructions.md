# PJSIP/PJPROJECT

PJSIP is a free, open-source multimedia communication library written in C
implementing standard-based protocols (SIP, SDP, RTP, STUN, TURN, ICE).
It combines signaling protocol (SIP) with multimedia framework and NAT
traversal functionality into a high-level API (PJSUA/PJSUA2), portable across
desktops, embedded systems, and mobile handsets.

Always reference these instructions first and fallback to search or bash commands
only when you encounter unexpected information that does not match the info here.

## Build & Test

- Build:
  - `./configure` - configure build system (~7 seconds)
  - `make dep` - generate dependencies (~13 seconds)
  - `make -j3` - build everything (~65 seconds). NEVER CANCEL. Set timeout to 120+ seconds.
- Run individual component tests (NEVER CANCEL any of these):
  - `make pjlib-test` - core library tests (5+ min, timeout 600s)
  - `make pjlib-util-test` - utility library tests (~3 min, timeout 360s)
  - `make pjnath-test` - NAT traversal tests (3+ min, timeout 360s)
  - `make pjmedia-test` - media framework tests (5+ min, timeout 600s)
  - `make pjsip-test` - SIP stack tests (5+ min, timeout 600s)
  - `make pjsua-test` - high-level API tests, Python-based (5+ min, timeout 600s)
- Get target architecture: `make infotarget` (e.g., `x86_64-pc-linux-gnu`)
- Run main PJSUA application:
  - `./pjsip-apps/bin/pjsua-x86_64-pc-linux-gnu --help` - show all options
  - `./pjsip-apps/bin/pjsua-x86_64-pc-linux-gnu --null-audio --no-cli-console` - basic test run
  - Note: pjsua binary name depends on the target architecture name
- SSL tests will fail with timeouts in sandboxed environments - this is expected.
- Run a specific test by passing its name(s) as arguments to the test executable:
  - `./pjlib/bin/pjlib-test-x86_64-pc-linux-gnu timer_test` - run only timer_test
  - `./pjlib/bin/pjlib-test-x86_64-pc-linux-gnu --list` - list all available tests
  - Test names do NOT use `--test` flag — just pass the name directly as a positional argument.
  - Common options: `-w N` (worker threads), `--shuffle`, `--ci-mode`, `--stop-err` (stop on first error)

### Minimum Validation (mandatory after every change)
- **Always run `make -j3`** after changes — code must compile with zero errors and zero warnings (`-Wall` is enabled by default).
- Fix all warnings before considering the change done. CI will also catch these.
- For targeted builds, compile just the affected component: e.g., `cd pjlib/build && make` or `cd pjsip/build && make`.
- **Known warning**: `PJ_TODO(id)` macro (defined in `pjlib/include/pj/config.h`) expands to a label (`TODO___id:`) that intentionally triggers `-Wunused-label` as a compile-time reminder. These warnings are expected in the codebase. To suppress them locally (e.g., for clean CI builds), add `#define PJ_TODO(x)` in `pjlib/include/pj/config_site.h`.

### Recommended Validation
- Run the test suite for the component you modified (see test commands above).
- For quick smoke test: `cd tests/pjsua && python3 run.py mod_run.py scripts-run/100_simple.py` (~2 seconds).

### Manual Validation

1. **Basic SIP**: `echo "Cp\nq" | ./pjsip-apps/bin/pjsua-x86_64-pc-linux-gnu --null-audio --no-cli-console` - verify codec list includes speex, G.711, etc.
2. **Config check**: `./pjlib/bin/pjlib-test-x86_64-pc-linux-gnu --config --list | grep PJ_SSL` - verify SSL support
3. **Quick test**: `make pjlib-util-test`

### Configure Options

- `--disable-ssl` - Disable SSL/TLS support
- `--with-gnutls=/usr/` - Use GnuTLS instead of OpenSSL
- `--enable-shared` - Build shared libraries
- `CFLAGS="-g -fPIC"` - Debug symbols + position-independent code
- Feature toggles via `#define` in `config_site.h`: `PJMEDIA_HAS_VIDEO`, `PJSIP_HAS_TLS_TRANSPORT`, etc.

## Repository Structure

```
pjlib/              # Base framework (OS abstraction, data structures, pool allocator)
pjlib-util/         # Utilities (DNS, STUN, XML, JSON, PCAP)
pjnath/             # NAT traversal (STUN, TURN, ICE)
pjmedia/            # Media framework (codecs, transport, audio/video)
pjsip/              # SIP stack (transactions, dialogs, UA layer)
pjsip-apps/         # Applications and samples
  src/pjsua/        # PJSUA command-line reference app
  src/samples/      # Standalone sample applications
third_party/        # Bundled third-party libs (srtp, resample, etc.)
tests/              # Test frameworks
  pjsua/            # Python-based PJSUA tests
build/              # Build system files
.github/            # GitHub workflows and configs
```

### Architecture Layers (top -> bottom)

1. **PJSUA2** (C++ API) / **PJSUA** (C high-level API) - `pjsip/include/pjsua-lib/`
2. **PJSIP UA** - Dialog, call, presence, registration - `pjsip/include/pjsip-ua/`
3. **PJSIP Core** - SIP transactions, transport, message parsing - `pjsip/include/pjsip/`
4. **PJMEDIA** - Media sessions, codecs, RTP/RTCP - `pjmedia/include/pjmedia/`
5. **PJNATH** - ICE, STUN, TURN - `pjnath/include/pjnath/`
6. **PJLIB** - Pool allocator, OS abstraction, lists - `pjlib/include/pj/`

### Key Files

- `pjlib/include/pj/config_site.h` - Main configuration overrides (start from `config_site_sample.h`)
- `user.mak` - User-specific build customizations
- `build.mak` - Generated build configuration
- Adding a new SIP header: `pjsip/include/pjsip/sip_msg.h`, `pjsip/src/pjsip/sip_parser.c`
- Adding a new codec: `pjmedia/include/pjmedia/codec.h`, see `pjmedia/src/pjmedia-codec/`
- Modifying call handling: `pjsip/src/pjsua-lib/pjsua_call.c`
- Transport changes: `pjsip/include/pjsip/sip_transport.h`
- ICE/NAT: `pjnath/include/pjnath/ice_strmtp.h`

### Main Applications Built

After running `make`:
- `pjsip-apps/bin/pjsua-x86_64-pc-linux-gnu` - Main SIP user agent application
- `pjsip-apps/bin/pjsystest-x86_64-pc-linux-gnu` - Audio system test application
- `pjsip-apps/bin/samples/x86_64-pc-linux-gnu/` - Various sample applications

### Test Executables Built

- `pjlib/bin/pjlib-test-x86_64-pc-linux-gnu` - Core library tests
- `pjlib-util/bin/pjlib-util-test-x86_64-pc-linux-gnu` - Utility tests
- `pjnath/bin/pjnath-test-x86_64-pc-linux-gnu` - NAT traversal tests
- `pjmedia/bin/pjmedia-test-x86_64-pc-linux-gnu` - Media framework tests
- `pjsip/bin/pjsip-test-x86_64-pc-linux-gnu` - SIP stack tests
- `pjsip/bin/pjsua2-test-x86_64-pc-linux-gnu` - High-level C++ API tests

### Testing Framework

- Python-based test framework in `tests/pjsua/`
- Use `cd tests/pjsua && python3 run.py MODULE CONFIG` for individual tests
- Example: `cd tests/pjsua && python3 run.py mod_run.py scripts-run/100_simple.py`
- Use `cd tests/pjsua && python3 runall.py` for complete test suite
- Test modules include: mod_run, mod_call, mod_pres, mod_sendto, mod_media_playrec

## Coding Conventions

Follow the official PJSIP coding style: https://docs.pjsip.org/en/latest/get-started/coding-style.html

### Language Standard
- **ANSI C (C89/C90)** for core C modules. No declarations after statements.
- C++ `//` comments allowed only for highlighting disabled/suspicious code sections.

### Formatting
- **Indentation**: 4 spaces (no tabs)
- **Line length**: 80 characters (header files strict, .c files tolerate up to ~90)
- **Braces (K&R style)**:
  - Same line for control statements: `if (...) {`
  - Next line for functions, structs, enums
  - Exception: multiline conditions place opening brace on new line
- **Comments**: Use `/* */` style for C code, `/** */` for Doxygen API docs on all public APIs
- **Function signatures**: Align continuation parameters with opening parenthesis
- Keep functions small - PJSIP style favors many focused functions
- Observe existing code patterns when in doubt

### Naming
- Prefix by module: `pj_` (pjlib), `pjsip_` (sip), `pjmedia_` (media), `pjnath_` (nat)
- pjlib basic types use `_t` suffix: `pj_str_t`, `pj_pool_t`, `pj_status_t`
- Higher-level structs typically omit `_t`: `pjsip_dialog`, `pjmedia_stream`, `pjsip_inv_session`
- Callbacks typedef: `pjsip_module_on_rx_request`, `pjsua_callback`
- Constants/macros: ALL_CAPS with prefix (`PJSIP_MAX_URL_SIZE`, `PJ_TRUE`, `PJ_FALSE`)

### Memory Management
- Core C modules (pjlib, pjsip, pjmedia, pjnath) use **pool-based allocation** (`pj_pool_t`).
- Every module/object gets a pool from a pool factory (`pj_caching_pool`).
- Use `pj_pool_alloc()`, `pj_pool_calloc()`, `PJ_POOL_ALLOC_T()`, `PJ_POOL_ZALLOC_T()`.
- Pools are freed as a whole - no individual object deallocation.
- Avoid `malloc()`/`free()` in core C code (exceptions: pool implementation itself, platform-specific code).
- PJSUA2 (C++ layer) uses `new`/`delete` for C++ objects — this is normal and expected there.

### String Handling
- PJSIP uses `pj_str_t` (pointer + length), NOT null-terminated C strings.
- Use `pj_str()` to wrap literals, `pj_strdup()` to copy with pool.
- Never assume null-termination. Use `pj_strcmp()`, `pj_stricmp()`.

### Error Handling
- Functions return `pj_status_t`. `PJ_SUCCESS` (0) = success.
- Always check return values. Use `PJ_ASSERT_RETURN()` for preconditions.
- Log errors with `PJ_PERROR()` or `pj_strerror()`.

### Logging
- Logging levels: 0 (fatal) to 6 (trace). Default compile-time max is 5.
- Use `PJ_LOG(level, (sender, format, ...))` — note the double parentheses.
- Each source file defines `THIS_FILE` for the sender parameter.
- Level 5 recommended during development/troubleshooting. Level 3 for production.

## Architecture Notes

### Threading Model
- PJSIP can run single-threaded (with polling) or multi-threaded.
- SIP worker threads process events via `pjsip_endpt_handle_events()`.
- Media and SIP run on separate thread groups (media endpoint has its own ioqueue worker).
- Use `pj_mutex_t` / `pj_lock_t` for synchronization - NOT pthread directly.
- Callback functions may be called from worker threads - be thread-safe.
- Never invoke user callbacks while holding a mutex — this is prone to deadlock. Release the lock before calling callbacks, then re-acquire if needed.
- `pj_init()` and `pj_shutdown()` must be called from the main thread; calls must be balanced.
- Non-main threads must register via `pj_thread_register()` after `pj_init()`.

### Group Lock (`pj_grp_lock_t`)
- Provides mutual exclusion, reference counting, and lock ordering in one primitive.
- Used by dialogs, transactions, ICE sessions, and other long-lived objects.
- `pj_grp_lock_add_ref()` / `pj_grp_lock_dec_ref()` manage lifetime. `dec_ref` returns `PJ_EGONE` when the object is destroyed.
- Always ensure object lifetime extends beyond pending timers and async I/O. Use `pj_grp_lock_add_ref()` before scheduling a timer/callback and `pj_grp_lock_dec_ref()` in the callback itself.
- Never destroy a mutex/lock that other threads may still reference. Use reference counting (`pj_grp_lock_t`) to ensure the lock is only destroyed when the last reference is released.
- Members register destruction handlers via `pj_grp_lock_add_handler()`.
- Lock ordering: PJSUA_LOCK > dialog grp_lock > transaction grp_lock.
- Chain external locks with `pj_grp_lock_chain_lock()` (negative pos = before, positive = after).
- Enable `PJ_GRP_LOCK_DEBUG` in `config_site.h` to trace ref count changes with file/line info.

### SIP Module System
- SIP processing uses a layered module system (`pjsip_module`).
- Modules register callbacks (`on_rx_request`, `on_rx_response`, `on_tx_request`, `on_tx_response`).
- Messages pass through modules in priority order (lower number = higher priority):
  - `PJSIP_MOD_PRIORITY_TRANSPORT_LAYER` (8) - transport
  - `PJSIP_MOD_PRIORITY_TSX_LAYER` (16) - transaction
  - `PJSIP_MOD_PRIORITY_UA_PROXY_LAYER` (32) - UA/dialog
  - `PJSIP_MOD_PRIORITY_DIALOG_USAGE` (48) - dialog usages (invite, subscribe)
  - `PJSIP_MOD_PRIORITY_APPLICATION` (64) - application modules
- Each module gets an ID used to store per-message/per-transaction data via `mod_data[]`.

```c
static pjsip_module my_module = {
    .name = pj_str("my-module"),
    .priority = PJSIP_MOD_PRIORITY_APPLICATION,
    .on_rx_request = &on_rx_request,
    .on_rx_response = &on_rx_response,
};
pjsip_endpt_register_module(endpt, &my_module);
```

### SIP Call Flow (outgoing)
```
pjsua_call_make_call()
  -> pjsip_inv_create_uac() (create INVITE session)
    -> pjsip_dlg_create_uac() (create dialog)
      -> pjsip_tsx_create_uac() (create transaction)
        -> pjsip_transport_send() (send over UDP/TCP/TLS)
```

### Media Pipeline
```
Sound device (mic) -> pjmedia_snd_port -> Conference bridge
  -> pjmedia_stream (encode) -> pjmedia_transport (RTP/SRTP) -> Network

Network -> pjmedia_transport (RTP) -> pjmedia_stream (jitter buffer + decode)
  -> Conference bridge -> pjmedia_snd_port -> Sound device (speaker)
```
- Sound device drives the clock via playback/recording callbacks.
- Media endpoint has its own ioqueue worker thread for RTP packet polling.
- Conference bridge mixes audio from all connected ports.

### Platform Differences
- **I/O Queue backends**: epoll (Linux), kqueue (macOS/BSD), select (portable), IOCP (Windows)
- IOCP backend (`ioqueue_winnt.c`) has different threading semantics than epoll/select
- `PJ_IOQUEUE_IMP_IOCP = 3` (defined in `pjlib/include/pj/config.h`)

## API Layers Guide

| Layer | Language | When to use |
|-------|----------|-------------|
| **PJSUA2** | C++ / Java / Python / C# | Default for most apps. Clean OOP API with persistence. |
| **PJSUA-LIB** | C | C-based clients, tighter control than PJSUA2. |
| **PJSIP/PJMEDIA/PJNATH** | C | Non-client apps, individual components, tight footprint. |

- PJSUA2 and PJSUA-LIB can drop down to lower layers when needed.
- SWIG bindings (Java/Python/C#) cannot access lower C layers directly.
- PJSUA2 uses exceptions (`pj::Error`); C layers use `pj_status_t` return codes.
- For PJSUA2 with GC languages (Java/Python): set `threadCnt=0` and poll manually, call `delete()` explicitly on PJSUA2 objects.

## Do's and Don'ts

### DO:
- Reference existing patterns in the codebase before generating new code
- Follow the module prefix naming convention strictly
- Check `pj_status_t` return values everywhere
- Use pool allocation for all dynamic memory in core C modules
- For new APIs, prefer a param struct over many positional arguments to allow future extension without breaking the signature (e.g., `pjsua_call_setting`, `pjsua_acc_config`). Not needed for simple/specific functions with stable parameters.
- Declare all variables at the top of the block (C89 requirement)
- Add Doxygen `/** */` comments for all new public API functions, structs, and enums

### DON'T:
- Don't use malloc/free in core C modules (use pool allocation instead)
- Never assume null-terminated strings with `pj_str_t`
- Never modify public headers without understanding downstream impact
- Don't add C++ in core C modules (pjlib, pjsip, pjmedia, pjnath)
- Don't ignore thread safety in callback implementations
- Don't add external library dependencies without discussion
- Don't declare variables after statements in C code (violates C89)
- Don't destroy objects that have pending timers or async callbacks — cancel or ensure completion first

## Common Reference

### Build System Details
- Uses autotools (configure/make) build system
- Main build controlled by top-level Makefile
- Each component has its own build/ subdirectory with component-specific Makefiles
- Parallel builds supported with `make -j<N>`
- Dependencies generated with `make dep`

### Key Environment Variables
- `TARGET_NAME` - Target architecture (set by configure)
- `CI_ARGS` - Additional arguments for CI testing
- `CI_MODE` - CI mode flag
- `CI_RUNNER` - CI runner command wrapper

### SSL/TLS Support
- Default build includes OpenSSL support
- SSL implementation ID: 1=OpenSSL, 2=GnuTLS
- Check SSL support: `./pjlib/bin/pjlib-test-x86_64-pc-linux-gnu --config --list | grep SSL`

### Built-in Media Codecs
- G.711 (PCMU/PCMA), G.722, G.722.1, GSM, iLBC, L16 (linear PCM), Speex

### CI Integration
- GitHub Actions workflows in `.github/workflows/`
- Primary CI runs on Linux, Mac, and Windows
- Uses `cirunner` tool for test execution with timeouts
- Tests multiple configurations (SSL/no-SSL, video codecs, etc.)

### Writing Robust Tests
- Tests must not depend on precise timing. Use generous tolerances or event-driven synchronization (barriers, semaphores) instead of fixed delays. CI runners can be 5-10x slower than local machines.
- When using platform-specific features (sigaltstack, thread stacks), verify compatibility with AddressSanitizer. CI runs ASan builds on macOS ARM64. Test with `ASAN_OPTIONS` when touching thread/signal code.
