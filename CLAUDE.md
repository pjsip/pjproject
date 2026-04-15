# PJSIP/PJPROJECT

Open-source multimedia communication library in C (SIP, SDP, RTP, STUN, TURN, ICE).

## Build & Test

- **Build**: `./configure && make -j3` (timeout 120s+, NEVER CANCEL)
- **Skip `make dep`** — it often produces corrupt `.depend` files. `make` generates deps automatically. If build fails with "missing separator" in `.depend`, run `find . -name "*.depend" -delete` and retry.
- **Targeted build**: `cd pjlib/build && make` (or `pjsip/build`, etc.)
- **Full clean**: `make clean` (required when switching SSL backends or configure options)
- Get target arch: `make infotarget` (e.g., `x86_64-pc-linux-gnu`)
- **Three build systems** — when adding/removing source files, update all three:
  1. GNU Makefile (e.g., `pjlib/build/Makefile`)
  2. MSVC project (e.g., `pjlib/build/pjlib.vcxproj`)
  3. CMake (`CMakeLists.txt`)

### Running Tests

Run via Makefile (NEVER CANCEL, 5+ min each, timeout 600s):
```
make pjlib-test        # core library
make pjsip-test        # SIP stack
make pjsua-test        # high-level API (Python)
```

Run specific test directly — **must run from the bin/ directory**:
```
cd pjlib/bin && ./pjlib-test-x86_64-pc-linux-gnu timer_test
cd pjlib/bin && ./pjlib-test-x86_64-pc-linux-gnu --list
```
Options: `-w N` (worker threads), `--shuffle`, `--ci-mode`, `--stop-err`

Quick smoke test: `cd tests/pjsua && python3 run.py mod_run.py scripts-run/100_simple.py`

### Minimum Validation (mandatory after every change)

- **Always run `make -j3`** — zero errors, zero warnings (`-Wall` enabled).
- For targeted builds: `cd pjlib/build && make` or `cd pjsip/build && make`.
- **Known warning**: `PJ_TODO(id)` macro triggers `-Wunused-label` intentionally. Suppress with `#define PJ_TODO(x)` in `config_site.h`.

### Configure Options

- `--with-gnutls=/usr/` — GnuTLS instead of OpenSSL (requires `make clean` first)
- `--disable-ssl` — disable SSL/TLS
- `--enable-shared` — shared libraries
- Feature toggles: `#define` in `pjlib/include/pj/config_site.h`

### Config Site

- `pjlib/include/pj/config_site.h` — local configuration overrides, NOT tracked by git
- CI uses `config_site_test.h` (copied to `config_site.h` in workflows)
- SSL tests will fail with timeouts in sandboxed environments — expected

## Repository Structure

```
pjlib/           # Base framework (OS abstraction, data structures, pool allocator)
pjlib-util/      # Utilities (DNS, STUN, XML, JSON, WebSocket)
pjnath/          # NAT traversal (STUN, TURN, ICE)
pjmedia/         # Media framework (codecs, transport, audio/video)
pjsip/           # SIP stack (transactions, dialogs, UA layer)
pjsip-apps/      # Applications and samples
third_party/     # Bundled third-party libs (srtp, resample, etc.)
tests/pjsua/     # Python-based PJSUA tests
.github/         # CI workflows
```

### Architecture Layers (top → bottom)

1. **PJSUA2** (C++) / **PJSUA** (C high-level) — `pjsip/include/pjsua-lib/`
2. **PJSIP UA** — dialogs, calls, presence — `pjsip/include/pjsip-ua/`
3. **PJSIP Core** — transactions, transport, parsing — `pjsip/include/pjsip/`
4. **PJMEDIA** — media sessions, codecs, RTP — `pjmedia/include/pjmedia/`
5. **PJNATH** — ICE, STUN, TURN — `pjnath/include/pjnath/`
6. **PJLIB** — pool allocator, OS abstraction — `pjlib/include/pj/`

### Key Files

- `pjlib/include/pj/config_site.h` — main configuration overrides
- `pjlib/include/pj/config.h` — default configuration values
- `build.mak` — generated build configuration
- ICE/NAT: `pjnath/include/pjnath/ice_strmtp.h`
- Call handling: `pjsip/src/pjsua-lib/pjsua_call.c`
- Transport: `pjsip/include/pjsip/sip_transport.h`

## Coding Conventions

Follow: https://docs.pjsip.org/en/latest/get-started/coding-style.html

Documentation source (for when the docs site is not directly accessible):
https://github.com/pjsip/pjproject_docs

### Essentials
- **ANSI C (C89/C90)** for core modules. No declarations after statements.
- **4 spaces** indentation (no tabs). ~80 char lines.
- **K&R braces**: same line for control statements, next line for functions/structs.
- **`/* */` comments** for C code, `/** */` for Doxygen on public APIs.
- **Module prefixes**: `pj_` (pjlib), `pjsip_` (sip), `pjmedia_` (media), `pjnath_` (nat)
- **Constants**: ALL_CAPS with prefix (`PJSIP_MAX_URL_SIZE`, `PJ_TRUE`)

### Memory Management
- Core C modules use **pool-based allocation** (`pj_pool_t`).
- Use `PJ_POOL_ALLOC_T()`, `PJ_POOL_ZALLOC_T()`, `pj_pool_alloc()`.
- Pools freed as a whole — no individual deallocation.
- **No `malloc()`/`free()`** in core C code.

### String Handling
- `pj_str_t` (pointer + length), NOT null-terminated.
- Never assume null-termination. Use `pj_strcmp()`, `pj_stricmp()`.

### Error Handling
- Functions return `pj_status_t`. `PJ_SUCCESS` (0) = success.
- Always check return values. Use `PJ_ASSERT_RETURN()` for preconditions.

### Logging
- `PJ_LOG(level, (sender, format, ...))` — note double parentheses.
- Each file defines `THIS_FILE`. Levels: 0 (fatal) to 6 (trace).

## Architecture Notes

### Threading Model
- Single-threaded (polling) or multi-threaded.
- Use `pj_mutex_t` / `pj_lock_t` — NOT pthread directly.
- Callbacks may fire from worker threads — be thread-safe.
- **Avoid invoking user callbacks while holding a mutex** — release lock first to prevent deadlock.

### Group Lock (`pj_grp_lock_t`)
- Mutual exclusion + reference counting + lock ordering in one primitive.
- `add_ref()` before scheduling timer/callback, `dec_ref()` in the callback.
- Never destroy a lock that other threads may reference — use ref counting.
- Lock ordering: PJSUA_LOCK > dialog grp_lock > transaction grp_lock.
- Debug: enable `PJ_GRP_LOCK_DEBUG` in `config_site.h`.

### Platform Differences
- **I/O Queue backends**: epoll (Linux), kqueue (macOS/BSD), select (portable), IOCP (Windows)
- IOCP backend has different threading semantics than epoll/select.

## Do's and Don'ts

### DO:
- Reference existing patterns before writing new code
- Follow module prefix naming strictly
- Check `pj_status_t` return values everywhere
- Declare all variables at top of block (C89)
- Add Doxygen `/** */` for new public APIs
- Prefer param structs over many positional arguments for extensibility

### DON'T:
- Don't use malloc/free in core C modules
- Don't assume null-terminated strings with `pj_str_t`
- Don't add C++ in core C modules
- Don't modify public headers without understanding downstream impact
- Don't destroy objects with pending timers/async callbacks
- Don't declare variables after statements (C89)

## CI Integration

- GitHub Actions: `.github/workflows/` (Linux, Mac, Windows)
- Uses `cirunner` tool for test execution with timeouts
- Tests multiple configurations (SSL/no-SSL, GnuTLS, video codecs, etc.)
- `--ci-mode` widens timing tolerances for slow CI runners

### Writing Robust Tests
- **No precise timing** — use generous tolerances or event-driven sync. CI runners can be 5-10x slower.
- Verify ASan compatibility for platform-specific features (sigaltstack, thread stacks).
