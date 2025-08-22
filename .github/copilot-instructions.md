# PJSIP/PJPROJECT

PJSIP is a free and open source multimedia communication library written in C with high level API in C, C++, Java, C#, and Python languages. It implements standard based protocols such as SIP, SDP, RTP, STUN, TURN, and ICE. It combines signaling protocol (SIP) with rich multimedia framework and NAT traversal functionality into high level API that is portable and suitable for almost any type of systems ranging from desktops, embedded systems, to mobile handsets.

Always reference these instructions first and fallback to search or bash commands only when you encounter unexpected information that does not match the info here.

## Working Effectively

- Bootstrap, build, and test the repository:
  - `./configure` - takes ~7 seconds to configure build system
  - `make dep` - generate dependencies, takes ~13 seconds  
  - `make -j3` - build everything with parallel jobs, takes ~65 seconds. NEVER CANCEL. Set timeout to 120+ seconds.
- Run individual component tests:
  - `make pjlib-test` - core library tests, takes 5+ minutes. NEVER CANCEL. Set timeout to 600+ seconds.
  - `make pjlib-util-test` - utility library tests, takes ~3 minutes. NEVER CANCEL. Set timeout to 360+ seconds.
  - `make pjnath-test` - NAT traversal tests, takes 3+ minutes. NEVER CANCEL. Set timeout to 360+ seconds.
  - `make pjmedia-test` - media framework tests, takes 5+ minutes. NEVER CANCEL. Set timeout to 600+ seconds.
  - `make pjsip-test` - SIP stack tests, takes 5+ minutes. NEVER CANCEL. Set timeout to 600+ seconds.
  - `make pjsua-test` - high-level API tests (Python-based), takes 5+ minutes. NEVER CANCEL. Set timeout to 600+ seconds.
- Run main PJSUA application:
  - `./pjsip-apps/bin/pjsua-x86_64-pc-linux-gnu --help` - show all options
  - `./pjsip-apps/bin/pjsua-x86_64-pc-linux-gnu --null-audio --no-cli-console` - basic test run
- Get target architecture name:
  - `make infotarget` - returns target name (e.g., x86_64-pc-linux-gnu)

## Coding Style

- Follow the official PJSIP coding style guidelines: https://docs.pjsip.org/en/latest/get-started/coding-style.html
- The coding style covers naming conventions, indentation, formatting, and other code organization principles
- Consistent style is essential for maintainability across the large PJSIP codebase

## Validation

- ALWAYS run through at least one complete validation scenario after making changes.
- Test PJSUA basic functionality: run PJSUA with null audio device and verify it starts without errors.
- For quick validation: `cd tests/pjsua && python3 run.py mod_run.py scripts-run/100_simple.py` (takes ~2 seconds).
- ALWAYS run appropriate test suites for the component you're modifying before completing your work.
- SSL tests will fail with timeouts in sandboxed environments - this is expected and normal.

### Manual Validation Scenarios

After making changes, validate functionality by:
1. **Basic SIP functionality**: Run `echo "Cp\nq" | ./pjsip-apps/bin/pjsua-x86_64-pc-linux-gnu --null-audio --no-cli-console` and verify codec list includes speex, G.711, etc.
2. **Configuration check**: Run `./pjlib/bin/pjlib-test-x86_64-pc-linux-gnu --config --list | grep PJ_SSL` to verify SSL support is enabled
3. **Quick component test**: Run one of the shorter unit tests like `make pjlib-util-test` which completes in ~3 minutes

## Common Tasks

The following are outputs from frequently run commands. Reference them instead of viewing, searching, or running bash commands to save time.

### Repository Structure

```
/home/runner/work/pjproject/pjproject/
├── pjlib/              # Portable library (core)
├── pjlib-util/         # Utility library  
├── pjnath/            # NAT traversal library
├── pjmedia/           # Media framework
├── pjsip/             # SIP stack
├── pjsip-apps/        # Applications and samples
│   ├── bin/           # Built executables
│   └── src/           # Source for applications
├── third_party/       # Third-party dependencies
├── tests/             # Test frameworks
│   ├── pjsua/         # Python-based PJSUA tests
│   └── automated/     # Other automated tests
├── build/             # Build system files
├── .github/           # GitHub workflows and configs
├── configure          # Main configure script (calls aconfigure)
├── aconfigure         # Real autotools configure script
├── Makefile           # Main makefile
└── build.mak.in       # Build configuration template
```

### Main Applications Built

After running `make`:
- `pjsip-apps/bin/pjsua-x86_64-pc-linux-gnu` - Main SIP user agent application
- `pjsip-apps/bin/pjsystest-x86_64-pc-linux-gnu` - System test application
- `pjsip-apps/bin/samples/x86_64-pc-linux-gnu/` - Various sample applications

### Test Executables Built

- `pjlib/bin/pjlib-test-x86_64-pc-linux-gnu` - Core library tests
- `pjlib-util/bin/pjlib-util-test-x86_64-pc-linux-gnu` - Utility tests  
- `pjnath/bin/pjnath-test-x86_64-pc-linux-gnu` - NAT traversal tests
- `pjmedia/bin/pjmedia-test-x86_64-pc-linux-gnu` - Media framework tests
- `pjsip/bin/pjsip-test-x86_64-pc-linux-gnu` - SIP stack tests
- `pjsip/bin/pjsua2-test-x86_64-pc-linux-gnu` - High-level C++ API tests

### Key Configuration Options

The configure script supports many options. Key ones:
- `--disable-ssl` - Disable SSL/TLS support
- `--with-gnutls=/usr/` - Use GnuTLS instead of OpenSSL
- `--enable-shared` - Build shared libraries
- `CFLAGS="-g -fPIC"` - Add debug symbols and position-independent code

### Testing Framework

- Python-based test framework in `tests/pjsua/`
- Use `cd tests/pjsua && python3 run.py MODULE CONFIG` for individual tests
- Use `cd tests/pjsua && python3 runall.py` for complete test suite
- Test modules include: mod_run, mod_call, mod_pres, mod_sendto, mod_media_playrec

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
- SSL tests may timeout in sandboxed environments (this is normal)

### Media Codecs

Built-in codecs include:
- G.711 (PCMU/PCMA)
- G.722
- G.722.1
- GSM
- iLBC  
- L16 (linear PCM)
- Speex

### Important Paths for Development

- `pjlib/include/pj/config_site.h` - Main configuration overrides
- `user.mak` - User-specific build customizations
- `build.mak` - Generated build configuration  
- `pjsip-apps/src/pjsua/` - Main PJSUA application source
- `pjsip-apps/src/samples/` - Sample application sources

### CI Integration

- GitHub Actions workflows in `.github/workflows/`
- Primary CI runs on Linux, Mac, and Windows
- Uses `cirunner` tool for test execution with timeouts
- Builds test multiple configurations (SSL/no-SSL, video codecs, etc.)