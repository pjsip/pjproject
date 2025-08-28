# PJSIP Project

PJSIP is a free and open source multimedia communication library written in C with high level API in C, C++, Java, C#, and Python languages. It implements standard based protocols such as SIP, SDP, RTP, STUN, TURN, and ICE. It combines signaling protocol (SIP) with rich multimedia framework and NAT traversal functionality into high level API that is portable and suitable for almost any type of systems ranging from desktops, embedded systems, to mobile handsets.

**Always reference these instructions first and fallback to search or bash commands only when you encounter unexpected information that does not match the info here.**

## Working Effectively

### Build System Requirements
- **GNU make** (required - Makefiles use GNU-specific commands)
- **gcc** and **g++** compilers
- **bash** shell recommended (for `echo -n` commands in dependency generation)
- **ar** and **ranlib** from GNU binutils

### Bootstrap, Build, and Test the Repository

**CRITICAL TIMING - NEVER CANCEL these commands:**
1. `./configure` -- takes 5 seconds. Detects system and creates build configuration.
2. `make dep` -- takes 15 seconds. NEVER CANCEL. Set timeout to 60+ seconds. Generates build dependencies.
3. `make` -- takes 100 seconds (1m39s). NEVER CANCEL. Set timeout to 180+ seconds. Builds all libraries and applications.

**Complete build sequence:**
```bash
cd /path/to/pjproject
./configure
make dep  
make
```

### Test Suites - NEVER CANCEL, Use Long Timeouts

**CRITICAL: All test commands take significant time. NEVER CANCEL. Set timeouts appropriately:**

- `make pjlib-test` -- takes 5m41s. NEVER CANCEL. Set timeout to 600+ seconds.
- `make pjlib-util-test` -- takes 3m3s. NEVER CANCEL. Set timeout to 300+ seconds.  
- `make pjmedia-test` -- takes 33s. NEVER CANCEL. Set timeout to 120+ seconds.
- `make pjnath-test` -- takes 10+ minutes. NEVER CANCEL. Set timeout to 900+ seconds.
- `make pjsip-test` -- takes 2-5 minutes. NEVER CANCEL. Set timeout to 600+ seconds.
- `make pjsua-test` -- takes 10+ minutes. NEVER CANCEL. Set timeout to 900+ seconds.

**Run all tests:** `make run_test` (from self-test.mak) -- takes 20+ minutes total. NEVER CANCEL. Set timeout to 1800+ seconds.

### Running Applications

**Main SIP User Agent:**
- `pjsip-apps/bin/pjsua-x86_64-pc-linux-gnu --help` -- shows comprehensive help
- `echo q | pjsip-apps/bin/pjsua-x86_64-pc-linux-gnu --null-audio` -- basic test run
- Use `--null-audio` for testing without audio hardware
- Use `--config-file filename` to specify configuration

**Other Applications:**
- `pjsip-apps/bin/pjsystest-x86_64-pc-linux-gnu` -- system/codec testing tool
- Sample applications in `pjsip-apps/bin/samples/x86_64-pc-linux-gnu/`

## Validation

**ALWAYS manually validate new code by:**
1. Building successfully with `make` (1m39s)
2. Running relevant test suites for your changes (timing above)
3. Testing pjsua basic functionality: `echo "CP\nq" | pjsip-apps/bin/pjsua-x86_64-pc-linux-gnu --null-audio --log-level 3`
4. Verifying the application shows account list and command prompt before exiting

**End-to-end validation scenario:**
```bash
# Build everything
./configure && make dep && make

# Test core functionality  
echo "CP" | timeout 10 pjsip-apps/bin/pjsua-x86_64-pc-linux-gnu --null-audio --config-file /dev/null --log-level 3

# Should show:
# - "pjsua version X.X initialized"  
# - Account list with local accounts
# - Command menu with call/buddy/account commands
# - Ready state
```

## Configuration and Customization

**Key configuration files:**
- `pjlib/include/pj/config_site.h` -- main customization (create if needed)
- `user.mak` -- custom compiler/linker flags (create if needed)
- `pjlib/include/pj/config_site_sample.h` -- example configurations for different platforms
- `pjlib/include/pj/config_site_test.h` -- test configuration with all features enabled

**To enable test configuration:** `cp pjlib/include/pj/config_site_test.h pjlib/include/pj/config_site.h`

**Common configuration options (in config_site.h):**
- `#define PJMEDIA_HAS_VIDEO 1` -- enable video support
- `#define PJ_LOG_MAX_LEVEL 3` -- reduce logging for performance
- `#define PJSUA_MAX_CALLS 4` -- limit concurrent calls for embedded systems

## Project Structure and Navigation

**Core libraries (build order):**
- `pjlib/` -- Portable OS abstraction library (foundation)
- `pjlib-util/` -- Utilities (XML, encryption, STUN, DNS resolution, HTTP client)  
- `pjnath/` -- NAT traversal library (STUN, TURN, ICE)
- `third_party/` -- Third-party dependencies (codecs, crypto)
- `pjmedia/` -- Media framework (RTP, codecs, audio/video devices)
- `pjsip/` -- SIP stack and user agent library
- `pjsip-apps/` -- Sample applications and tools

**Important files to check after making changes:**
- Always run `make dep` after changing header dependencies
- Check `build.mak` and `pjlib/include/pj/config_site.h` for configuration
- Test with `pjsua` application for SIP functionality
- Test with relevant `*-test` executables for specific subsystems

**Test executables locations:**
```
pjlib/bin/pjlib-test-x86_64-pc-linux-gnu
pjlib-util/bin/pjlib-util-test-x86_64-pc-linux-gnu  
pjnath/bin/pjnath-test-x86_64-pc-linux-gnu
pjmedia/bin/pjmedia-test-x86_64-pc-linux-gnu
pjsip/bin/pjsip-test-x86_64-pc-linux-gnu
```

## Troubleshooting

**Build issues:**
- SSL test failures are expected in sandboxed environments
- Missing dependencies: install with system package manager (apt-get, yum, etc.)
- Cross-compilation: edit `build.mak` manually (see README-configure)

**Common test failures:**
- SSL/network tests may fail in restricted environments (this is normal)
- Audio tests require `--null-audio` in headless environments
- Some pjsua tests may fail due to network restrictions (registration tests)

**Performance notes:**
- Default build includes debug symbols (`-g`) 
- Use `CFLAGS="-O2 -DNDEBUG"` for release builds
- Tests with timing dependencies may be sensitive to system load

## Platform-Specific Notes

**Linux (current platform):**
- Uses select() I/O queue by default
- OpenSSL support enabled automatically if available
- ALSA audio disabled in current build (null audio only)
- IPv6 support enabled

**Supported platforms:** Windows, macOS, Linux, Android, iOS, Symbian, Windows Mobile
**Cross-compilation:** Supported via manual `build.mak` editing

**CI Information:**
- GitHub Actions CI on Linux/Mac/Windows
- Extensive test matrix including video codecs, TLS variants
- Coverity scan and CodeQL analysis available