# PJSIP on Zephyr RTOS

This guide takes you from a clean machine to a SIP call running on [Zephyr RTOS](https://www.zephyrproject.org/): first on [`native_sim`](https://docs.zephyrproject.org/latest/boards/native/native_sim/doc/index.html), which runs Zephyr as an ordinary Linux process, then on a [Cortex-M33](https://developer.arm.com/Processors/Cortex-M33) emulated by QEMU, and from there to your own application and board. It is the reference for the Zephyr port; [docs.pjsip.org](https://docs.pjsip.org) links to or copies this file.

## What the port provides

- pjproject is a [Zephyr module](https://docs.zephyrproject.org/latest/develop/modules.html). Add this source tree to a Zephyr application and PJLIB, PJLIB-UTIL, PJNATH, PJMEDIA and PJSIP are built with the application, configured through Kconfig like any other part of Zephyr.
- The port runs on Zephyr's POSIX layer: BSD sockets, `select()`, `clock_gettime()` and, optionally, pthreads. There is one Zephyr-specific source file in PJLIB (network interface enumeration) and one platform header. Everything else is the same code that runs on Linux.
- [`embedded_ua.c`](pjsip-apps/src/samples/embedded_ua.c), a complete headless SIP user agent on pjsip and pjmedia without pjsua-lib, builds as a Zephyr sample. It registers to a SIP server (or not), supports TLS via [Mbed TLS](https://www.trustedfirmware.org/projects/mbed-tls/), answers calls with audio loopback, speaks SDES-SRTP, and is driven by SIP MESSAGE requests. It is the bring-up and reference application of the port.
- Tested targets:

  - `native_sim/native/64`
  - `mps2/an521/cpu0` (Cortex-M33 under QEMU, limited to 640 KB of RAM to match an STM32H563). Both register and complete calls with zero packet loss.
- pjlib-test, PJLIB's own test suite, runs on both targets through twister, single threaded and with threads.

Current status:

- Registration and four concurrent calls
- SIP transport: UDP or TLS
- Audio codec: G.711 or G.722
- SDES-SRTP
- Single threaded or pjsip and the media on two pthreads
- The standard pjsip and pjmedia feature set are working: RFC 3261, registration refresh, digest authentication (RFC 2617), rport (RFC 3581), SDP offer/answer (RFC 3264) with the RTCP (RFC 3605), re-INVITE and UPDATE (RFC 3311), PRACK (RFC 3262), call hold and resume, mid-call codec change, SIP MESSAGE (RFC 3428), RTCP reports and quality statistics, DTMF (RFC 4733), adaptive jitter buffer (up to 500 ms by default), packet loss concealment, VAD with silence suppression, symmetric RTP and ptime negotiation.
- The rest of the libraries compiles too, among them SUBSCRIBE/NOTIFY and PUBLISH, session timers, REFER, ICE, STUN and TURN, the conference bridge and the WAV ports, but *has not been exercised on Zephyr*, and there is no Zephyr sound device backend.

The preset builds G.711 only; G.722 and the resampler are a `config_site.h` setting away.


## Footprint

As an indication of the footprint, these are the sizes of the whole AN521 image, Zephyr and its network stack included, for the four ways the sample builds. The sample is built as it ships: G.711 with SDES-SRTP, and room for four concurrent calls (`MAX_CALLS` 4, with the 320 KB heap arena that holds them, 384 KB with TLS). The figures are the FLASH and RAM lines of the memory usage summary that `west build` prints at the end of the link, rounded to KB. RAM is the static allocation, data and BSS including the heap arena, the kernel heap and the thread stacks, not the run-time use. pjproject's pools and libsrtp's allocations come out of the heap arena, so they are inside this number, not on top of it; what they take at run time is the next table:

| Build | Flash | RAM |
|---|---|---|
| SIP over UDP, single thread | 343 KB | 402 KB |
| SIP over UDP, threads | 351 KB | 468 KB |
| SIP over TLS, single thread | 451 KB | 522 KB |
| SIP over TLS, threads | 460 KB | 589 KB |

Most of the RAM is what the configuration sets aside rather than pjproject's own data: a 320 KB heap arena for pjlib's pools (384 KB with TLS), the kernel heap the thread stacks come from in the threaded build, and the network buffers. The image size is the same however many calls are active, since calls draw on the heap arena at run time. What the arena holds was measured on the AN521, in the single-threaded UDP build with SRTP, with the sample's `mem` command (section 3) and `CONFIG_SYS_HEAP_RUNTIME_STATS=y`:

| State | Heap in use |
|---|---|
| Registered, idle | 51 KB |
| Each established call | about 55 KB |
| Four concurrent calls | 271 KB, peak 280 KB while they were set up |

Each reading includes a few KB for the SIP transactions still timing out, among them the MESSAGE that asked for it, so the steady idle figure is about 35 KB: the SIP endpoint, the transport, the media endpoint and the registration. A call is its media stream with the jitter buffer (10 KB), the dialog and INVITE session (12 KB), the 200 OK kept for retransmission (8 KB), the UDP media transport (5.5 KB), SRTP (4 KB), the call's own pool, codec, locks and INVITE transaction (6 KB), and libsrtp's own allocations (3 KB); nearly all of it is pool memory. The sample's 320 KB arena therefore holds the four calls that `MAX_CALLS` allows with room to spare; the four-call row was measured with the four INVITEs arriving within 10 ms of each other and all four calls carrying RTP both ways with no loss. These figures are with the preset's sizing (section 8): with pjproject's stock pool sizes, packet buffers and SDP limits a call took 82 KB and four calls peaked at 436 KB.

## 1. Install Zephyr and get pjproject

If you already have a working Zephyr workspace and SDK, skip to "Get pjproject" at the end of this section. Otherwise follow the [Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html); the steps below are its Ubuntu version, condensed.

Install the host tools (Ubuntu 24.04 or later):

```sh
sudo apt install --no-install-recommends git cmake ninja-build gperf \
  ccache dfu-util device-tree-compiler wget python3-dev python3-venv python3-tk \
  xz-utils file make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1
```

Zephyr needs CMake 3.28 or later and Python 3.12. Check with `cmake --version` and `python3 --version`.

Create the workspace. Zephyr lives in a Python virtual environment with its tool, `west`:

```sh
python3 -m venv ~/zephyrproject/.venv
source ~/zephyrproject/.venv/bin/activate
pip install west
west init -m https://github.com/zephyrproject-rtos/zephyr ~/zephyrproject
cd ~/zephyrproject
west update
west packages pip --install
west zephyr-export
```

`west update` downloads Zephyr's modules and takes a while. Zephyr 4.4 is the release the port is developed and tested on, and the oldest one it supports. Zephyr 4.4 moved to Mbed TLS 4.1 with the separate TF-PSA-Crypto module; pjproject's TLS backend works with both Mbed TLS 3.6 and 4.x, so Zephyr 4.3 and older, which ship Mbed TLS 3.6, may well work too, but they are not tested. To check out a specific release, add `--mr v4.4.0` to the `west init` command.

Install the Zephyr SDK, which has the cross compilers and a QEMU that knows the MPS2 boards:

```sh
cd ~/zephyrproject/zephyr
west sdk install
```

Add `-t arm-zephyr-eabi` to install only the Arm toolchain. The SDK is installed under your home directory (for example `~/zephyr-sdk-1.0.1`) and registers itself with CMake, so no environment variable is needed.

Every time you open a new terminal to work with Zephyr, activate the virtual environment again:

```sh
source ~/zephyrproject/.venv/bin/activate
```

### Get pjproject

Clone pjproject anywhere; it does not need to be inside the Zephyr workspace:

```sh
git clone https://github.com/pjsip/pjproject.git ~/pjproject
export PJ_DIR=~/pjproject
```

The rest of this guide refers to that location as `$PJ_DIR`; set the variable in every shell you run the commands from. No `./configure` or `make` is needed: the Zephyr build compiles pjproject from source as part of the application.

## 2. Configure

Two kinds of settings shape a build. pjproject's own compile-time settings, which codecs, how much logging, pool and queue sizes, are in `config_site.h` as on every other platform. Zephyr, the sample and the few things the build has to know about pjproject (threads, TLS, floating point) are [Kconfig](https://docs.zephyrproject.org/latest/build/kconfig/index.html) options. Section 8 is the reference for both; this section is what you need before the first build.

### config_site.h

pjproject's compile-time settings are in `pjlib/include/pj/config_site.h` in the pjproject tree. The file is not part of the repository: create it, and for Zephyr it must select the Zephyr preset:

```sh
cat > $PJ_DIR/pjlib/include/pj/config_site.h <<'EOF'
#define PJ_CONFIG_ZEPHYR 1
#include <pj/config_site_sample.h>
EOF
```

This is enough for the sample. The preset in [`config_site_sample.h`](pjlib/include/pj/config_site_sample.h) builds G.711 only, SDES-SRTP, no sample rate conversion, full logging and 16 sockets per I/O queue; section 8 says how to change any of that in the same file. The build stops with a message if the `config_site.h` is missing or does not define `PJ_CONFIG_ZEPHYR`. 

### Kconfig

[Kconfig](https://docs.zephyrproject.org/latest/build/kconfig/index.html) configures Zephyr, the sample and the pjproject module, like any Zephyr application. The sample's [`prj.conf`](zephyr/samples/embedded_ua/prj.conf) is a complete, commented configuration that works as is; the overlays [`overlay-threads.conf`](zephyr/samples/embedded_ua/overlay-threads.conf) and [`overlay-tls.conf`](zephyr/samples/embedded_ua/overlay-tls.conf) in the same directory add threads and TLS, applied with `-DEXTRA_CONF_FILE=`. Any option can also be given on the `west build` command line after `--`, as the examples in the next section do; everything after `--` goes to CMake.

The examples that register with a SIP server take its address from a shell variable. Set it once, to the address of your server:

```sh
export SIP_SERVER=192.168.1.19
```

The interactive menu needs a build directory. The first `west build` creates one from the sample's source directory and the board, and that is the only time they have to be given: the directory remembers both, and `west build` run inside it reuses them, together with whatever the menu saved. Keep the build directories in a folder of their own inside the workspace, so that they stay apart from Zephyr's; `west` works from any directory under the workspace:

```sh
mkdir -p ~/zephyrproject/mypjproject
cd ~/zephyrproject/mypjproject
west build -b native_sim/native/64 -d build-native $PJ_DIR/zephyr/samples/embedded_ua -t menuconfig
```

This configures the `build-native` build directory and opens the whole config tree as a menu (`-t guiconfig` opens a window instead).

Once the directory exists, work from inside it (or use `-d` to work from outside it), and the same command serve every build directory in this guide:

```sh
cd build-native
west build -t menuconfig
```

pjproject's options are under "Modules" then "pjproject". The sample's own options are in the "embedded_ua" menu at the top level. Zephyr's networking, stack and heap sizes are in their usual places. What menuconfig saves goes into the build directory's `.config` only, and a pristine build (`-p always`) starts from `prj.conf` again; to keep a setting, put it in `prj.conf` or give it after `--`.

## 3. Build and run the sample on native_sim

[`native_sim`](https://docs.zephyrproject.org/latest/boards/native/native_sim/doc/index.html) is a Zephyr board that is a Linux executable. It runs the real Zephyr kernel, network stack and POSIX layer, talks to the host through a TAP interface, and can be debugged with gdb. It is the fastest way to see the port working.

### Create the TAP interface

The emulated board needs a network link to the host. Create it once (it disappears on reboot):

```sh
sudo ip tuntap add dev zeth mode tap user $USER
sudo ip addr add 192.0.2.2/24 dev zeth
sudo ip link set zeth mtu 1400 up
```

The host is 192.0.2.2 and the Zephyr side is 192.0.2.1; both addresses are in the sample's [`prj.conf`](zephyr/samples/embedded_ua/prj.conf). The MTU of 1400 is a workaround for a QEMU driver bug described under Known limitations; it does no harm to native_sim. Zephyr's own `tools/net-tools/net-setup.sh` creates the same interface, if you prefer it.

### Build

If Section 2 left you with a configured `build-native`, build it from inside; the board, the source directory and the settings saved there are reused:

```sh
cd ~/zephyrproject/mypjproject/build-native
west build
```

Otherwise, or to start over from `prj.conf`, give the board and the source directory:

```sh
cd ~/zephyrproject/mypjproject
west build -p always -b native_sim/native/64 -d build-native $PJ_DIR/zephyr/samples/embedded_ua
cd build-native
```

`-p always` empties the build directory first, so the board and source directory have to come with it; `-b` is the board, `-d` the build directory. `-d` is only needed while the directory does not exist: run inside an existing build directory, `west build` uses it.

The rest of this section runs inside `build-native`. The first build compiles all of pjproject and takes a few minutes; later builds are incremental.

The sample is set up to register with a SIP server at 192.0.2.2, the host side of the TAP interface. For a first run without any server, build it without registration instead:

```sh
west build -- -DCONFIG_EMBUA_USE_REGISTRATION=0
```

Everything after `--` goes to CMake; this is how any Kconfig option is set from the command line. A setting given this way stays in the build directory for later builds, until a pristine build.

### Run

```sh
zephyr/zephyr.exe
```

You will see Zephyr boot, the network interface come up, and pjsip start its UDP transport on 192.0.2.1:55060. Leave it running. The "not safe entropy source" warning at the top is expected: native_sim's test entropy source starts from a fixed seed, so every run would have the same Call-ID and CSeq, which a SIP registrar rejects as a replay; the `pjproject-sim` snippet (section 9) therefore bakes in the `-seed-random` argument through `CONFIG_NATIVE_EXTRA_CMDLINE_ARGS`, which seeds it from the host at every start. Stop it with Ctrl-C.

### Make a call to it

Any SIP client on the host can call the sample. The easiest is pjsua from a normal Linux build of pjproject (`./configure && make` in a second checkout, or the same one; the Zephyr build does not interfere with it). The sample insists on SRTP, so SRTP has to be allowed over plain UDP:

```sh
cd $PJ_DIR/pjsip-apps/bin
./pjsua-x86_64-pc-linux-gnu --no-tcp --local-port 5062 --use-srtp=2 --srtp-secure=0 --play-file ../../tests/pjsua/wavs/input.16.wav --auto-play
```

At the pjsua prompt type `m`, then the URI `sip:bob@192.0.2.1:55060`. The sample answers after one ring. The Zephyr side logs every SIP message, so you can watch the INVITE, the SDP negotiation and the SRTP setup go by. `--play-file` with `--auto-play` makes pjsua stream the WAV into the call as soon as it connects, and the sample loops whatever it receives straight back. To hear that loopback, connect the call to the sound device: type `cl` to list the conference ports, then `cc`, the call's port number as source and `0` (the sound device) as destination. The WAV then comes out of your speaker after a round trip through the Zephyr guest. `dq` in pjsua prints the call quality and `h` hangs up.

### With a SIP server

With registration enabled (the default), the sample registers as `bob` with password `secret` to the server in `CONFIG_EMBUA_SIP_SERVER_IP`. These are Kconfig options in the sample's "embedded_ua" menu, so they can be set with menuconfig (section 2) or from the command line. Point it at your server, and turn registration back on if the build directory still has the no-registration setting from above:

```sh
west build -- -DCONFIG_EMBUA_SIP_SERVER_IP=\"$SIP_SERVER\" -DCONFIG_EMBUA_USE_REGISTRATION=1
```

The server can be anywhere the host can reach: the host forwards packets from the TAP interface as it does for any other network, and a server on the host itself answers directly. The port was developed against Kamailio; any registrar that does digest authentication will do. Create an account `bob`/`secret` on it, or change `CONFIG_EMBUA_USERNAME` and `CONFIG_EMBUA_PASSWORD`. The log shows the REGISTER, the 401 challenge, the authenticated REGISTER and the 200 OK; after that the sample is reachable by calling `bob` through the server.

### With the media on its own thread

So far the sample has run single threaded: one loop polls the SIP endpoint and paces the media, and no pthread function is linked. Build with the threads overlay and pjsip keeps the main thread while the media clock and RTP move to a pthread of their own, so that neither waits for SIP work:

```sh
west build -- -DEXTRA_CONF_FILE=overlay-threads.conf
```

The server address stays from the previous step. The [`overlay-threads.conf`](zephyr/samples/embedded_ua/overlay-threads.conf) overlay sets `CONFIG_PJPROJECT_THREADS`, which builds pjlib with threads, and the sample's `CONFIG_EMBUA_USE_MEDIA_THREAD` then defaults to 1. 

### Over TLS

Build with the [`overlay-tls.conf`](zephyr/samples/embedded_ua/overlay-tls.conf) overlay and the sample registers over TLS instead, to port 5061 of the server, and listens for TLS on 55061. The server address is needed here too unless the build directory already has it from the previous step; the default is the TAP address, where if nothing listens on 5061, the REGISTER will fail with "Connection refused":

```sh
west build -- -DCONFIG_EMBUA_SIP_SERVER_IP=\"$SIP_SERVER\" -DEXTRA_CONF_FILE=overlay-tls.conf
```

For TLS with the media thread give both overlays, as new `-DEXTRA_CONF_FILE` replaces the list the build directory had rather than adding to it:


```sh
west build -- -DCONFIG_EMBUA_SIP_SERVER_IP=\"$SIP_SERVER\" -DEXTRA_CONF_FILE="overlay-threads.conf;overlay-tls.conf"
```

Like a `-DCONFIG_...` value, the overlay list stays with the build directory, so to go back to UDP build pristine with `-p always`, giving the board and source directory again (inside the directory, `-d` is still not needed). 

What [`overlay-tls.conf`](zephyr/samples/embedded_ua/overlay-tls.conf) sets, and why, is commented in the file itself. The server certificate is not verified, as in the sample everywhere else.

The sample has no certificate of its own, so it cannot accept TLS connections: the server has to send requests back down the connection the sample opened to register, which SIP servers do for TLS clients behind NAT. Kamailio does it with `set_contact_alias()` on the REGISTER and `handle_ruri_alias()` plus `set_forward_no_connect()` before relaying, and its `tls.cfg` needs `method = TLSv1.2+`. The caller can stay on UDP; the server bridges the two.

### Driving it with SIP MESSAGE

embedded_ua has no console input. Send it a SIP MESSAGE (in pjsua: add it as a buddy and use `i`) with one of these bodies: `call <uri>` makes it place a call, `mem` logs its pool usage (and the C heap, when built with `CONFIG_SYS_HEAP_RUNTIME_STATS=y`), `quit` ends every call, unregisters and exits.

## 4. Run on a Cortex-M33 under QEMU

The MPS2 AN521 is an Arm development board that QEMU emulates, with a Cortex-M33 and a LAN9118 Ethernet controller. The `pjproject-sim` snippet (section 9) limits it to 640 KB of RAM, the size of the STM32H563, so that an image that would not fit the real part fails to link here, and sets the clock to what QEMU's model runs.

Build, giving the address of your SIP server as in section 3; without it the sample registers to 192.0.2.2, the host side of the TAP, where nothing answers:

```sh
cd ~/zephyrproject/mypjproject
west build -p always -b mps2/an521/cpu0 -d build-an521 $PJ_DIR/zephyr/samples/embedded_ua -- -DCONFIG_EMBUA_SIP_SERVER_IP=\"$SIP_SERVER\"
```

For a run without any server give `-DCONFIG_EMBUA_USE_REGISTRATION=0` instead, and the overlays apply as in section 3. The end of the build prints the memory use, for example:

```
Memory region         Used Size  Region Size  %age Used
           FLASH:      351352 B         4 MB      8.38%
             RAM:      411376 B       640 KB     62.77%
```

That is the whole image, Zephyr and its network stack included; 320 KB of the RAM is the heap arena pjlib's pools are carved from. With `overlay-tls.conf` the image is 451 KB of flash and 522 KB of RAM, and 589 KB of RAM with the threads overlay too: Mbed TLS's code, and the 64 KB the TLS overlay adds to the heap arena for its contexts.

The overlays apply as in section 3. For the largest variant, SIP over TLS with the media on its own thread:

```sh
west build -p always -b mps2/an521/cpu0 -d build-an521 $PJ_DIR/zephyr/samples/embedded_ua -- -DCONFIG_EMBUA_SIP_SERVER_IP=\"$SIP_SERVER\" -DEXTRA_CONF_FILE="overlay-threads.conf;overlay-tls.conf"
```

It registers over TLS to port 5061 of the server and is run and called exactly like the UDP build below. The build prints a CMake warning from Zephyr's Mbed TLS module about `CONFIG_MBEDTLS_HAVE_TIME_DATE`; it concerns certificate expiry checks, which the sample does not make, and can be ignored.

Run it with the launcher that comes with the sample, giving it the build directory:

```sh
$PJ_DIR/zephyr/samples/embedded_ua/run-qemu.sh build-an521
```

The usual way to run a Zephyr image in QEMU, `west build -t run`, does not work for the MPS2 boards: their Ethernet controller is part of the machine, Zephyr nevertheless adds it as a pluggable `-device lan9118`, and QEMU refuses. Hence the script.

The script finds QEMU in the SDK, attaches the board's Ethernet to the TAP interface named in `CONFIG_EMBUA_HOST_TAP_IFACE` (`zeth` by default) and boots the image. Extra arguments are passed to QEMU. From the host's point of view the board is at 192.0.2.1 exactly like native_sim, so the calls in section 3 work unchanged.

To stop QEMU press Ctrl-A, release it, then X. The terminal is the board's serial console, so Ctrl-C goes to the guest and does nothing. If the sequence does not reach QEMU, `pkill qemu-system-arm` from another terminal ends it.


## 5. Run the pjlib test suite

pjlib-test, PJLIB's own test suite, builds as a Zephyr test application in `zephyr/tests/pjlib`. It exercises the OS abstraction that a port touches most: pools, threads and thread-local storage, mutexes, timers, sockets over loopback, select and the ioqueue. Its `testcase.yaml` lets twister build and run it on native_sim, single threaded and with threads, checking the console for the suite's final line, and build it for the AN521:

```sh
cd ~/zephyrproject/mypjproject
west twister -T $PJ_DIR/zephyr/tests/pjlib -p native_sim/native/64 -p mps2/an521/cpu0 -v
```

A run takes a few minutes per configuration. To build and run one configuration by hand:

```sh
west build -p always -b native_sim/native/64 -d build-test $PJ_DIR/zephyr/tests/pjlib -- -DEXTRA_CONF_FILE=overlay-threads.conf
build-test/zephyr/zephyr.exe
```

On the AN521 twister only builds the image, for the reason given in section 4: Zephyr puts the board's Ethernet controller on the QEMU command line in a form QEMU rejects, and that happens for every image with networking enabled. Run it with the sample's launcher, telling QEMU to attach no network since the tests only use loopback:

```sh
west build -p always -b mps2/an521/cpu0 -d build-test-an521 $PJ_DIR/zephyr/tests/pjlib
$PJ_DIR/zephyr/samples/embedded_ua/run-qemu.sh build-test-an521 -nic none
```

The test setup is in these places:

- In [`pjlib/src/pjlib-test/main.c`](pjlib/src/pjlib-test/main.c) directory:

  - [`main.c`](pjlib/src/pjlib-test/main.c) sets `--ci-mode` and `-w 0`, so the tests run one at a time.
  - [`test_profile.h`](pjlib/src/pjlib-test/test_profile.h) selects the small footprint profile on any Zephyr board with less than 2 MB of RAM and records that native_sim's clock stands still while the CPU is busy
  - [`test.h`](pjlib/src/pjlib-test/test.h) derives the test sizes and which tests are built from that profile, and says why each one is left out.

- In [`zephyr/tests/pjlib/`](zephyr/tests/pjlib/) directory:

  - [`prj.conf`](zephyr/tests/pjlib/prj.conf) and [`boards/`](zephyr/tests/pjlib/boards/) files size the network stack, heap and stacks for the tests, with the reason for each setting.

The AN521 configuration enables `CONFIG_HW_STACK_PROTECTION`, so a stack overflow faults instead of corrupting memory silently. It caught two: the activesock test keeps 100 operation keys on the main thread's stack, so the suite needs a 32 KB main stack, and Zephyr's `poll()` keeps one event per possible descriptor on the stack, which with `CONFIG_ZVFS_POLL_MAX` at 88 outgrows the 1200-byte default stack of the socket service thread the DNS resolver uses. Keep both in mind when sizing an application: pjlib itself needs neither.

Everything else runs unchanged and passes on both targets in both thread modes.

## 6. Run on real hardware

Build for your board and flash as with any Zephyr application:

```sh
west build -p always -b <your board> $PJ_DIR/zephyr/samples/embedded_ua
west flash
```

Things to adjust in the sample for a real board, in a `boards/<your board>.conf` file in the sample directory (Zephyr merges it after `prj.conf` when building for that board) or in your own copy of `prj.conf`:

- Addressing. The sample uses a static address on the TAP link. On a real network, drop the `CONFIG_NET_CONFIG_MY_IPV4_*` lines and enable `CONFIG_NET_DHCPV4=y`.
- Random numbers. Remove `CONFIG_TEST_RANDOM_GENERATOR` and let the board's hardware RNG serve `CONFIG_ENTROPY_GENERATOR`. SIP Call-IDs, tags and SRTP keys all come from it.
- Memory. `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE` is the heap pjlib allocates its pools from; at about 55 KB a call the sample's 320 KB holds four concurrent calls with room to spare, and 192 KB would hold two (see the footprint figures in "What the port provides"). `CONFIG_MAIN_STACK_SIZE` of 16 KB covers pjsip's message parsing on the main thread; the deepest use seen was 12.4 KB.
- The board needs an Ethernet or other IP-capable interface with a Zephyr driver, and `CONFIG_NET_L2_ETHERNET=y` (or the matching L2) for it.

A board with less than 640 KB of RAM needs a smaller arena and fewer network buffers; a 1 MB part such as the STM32H753 has room to spare.

## 7. Use pjproject in your own application

A Zephyr application is a directory with `CMakeLists.txt`, `prj.conf` and sources. To use pjproject in it:

`CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20.0)

list(APPEND ZEPHYR_EXTRA_MODULES /path/to/pjproject)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(my_sip_app)

target_sources(app PRIVATE src/main.c)
target_link_libraries(app PRIVATE pjproject)
```

`ZEPHYR_EXTRA_MODULES` tells Zephyr to treat the pjproject tree as a module, and must be set before `find_package(Zephyr)`. It can equally be given on the command line (`-DZEPHYR_EXTRA_MODULES=/path/to/pjproject`) or, to make pjproject a permanent part of a west workspace, listed as a project in the west manifest, after which the line is not needed at all. The `pjproject` target carries every pjproject library and the include paths; the application includes `<pjsip.h>`, `<pjmedia.h>`, `<pjlib.h>` and so on as usual.

`prj.conf` needs, besides whatever the application needs itself:

```
CONFIG_POSIX_API=y
CONFIG_PJPROJECT=y

CONFIG_NETWORKING=y
CONFIG_NET_IPV4=y
CONFIG_NET_UDP=y
CONFIG_NET_SOCKETS=y
CONFIG_NET_IPV4_FRAGMENT=y

CONFIG_MAIN_STACK_SIZE=16384
CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=327680
CONFIG_ENTROPY_GENERATOR=y
```

The sample's `zephyr/samples/embedded_ua/prj.conf` is a complete, commented example and the best starting point; copy it and remove what you do not need. Two of its settings deserve a word. `CONFIG_NET_IPV4_FRAGMENT=y` is there because a SIP INVITE with SDP is usually larger than the Ethernet MTU and arrives as IP fragments. `CONFIG_ZVFS_OPEN_MAX` bounds the file descriptors `select()` can watch and must be at least `PJ_IOQUEUE_MAX_HANDLES`; the build checks this with a static assertion.

To build your application for QEMU's AN521 or for native_sim as the stand-ins for the hardware, add the `pjproject-sim` snippet of section 9 with `set(SNIPPET pjproject-sim)` before `find_package(Zephyr)`, as the sample does, or with `-S pjproject-sim` on the command line. It limits the AN521 to 640 KB, corrects its clock and seeds native_sim's entropy source, and does nothing on other boards.

The pjproject tree needs the `config_site.h` of section 2. The module is compiled with that one file's settings, so two applications in a workspace that want different pjproject settings use two pjproject checkouts, as they would on any other platform.

For SIP over TLS add the options of the sample's `overlay-tls.conf`: `CONFIG_PJPROJECT_TLS`, TCP, and Mbed TLS with at least one ciphersuite, certificate parsing and record buffers; then start a TLS transport with `pjsip_tls_transport_start2()` as the sample does. A threaded application also needs the sample's `psa_user_config.h`, or its own, as `CONFIG_TF_PSA_CRYPTO_USER_CONFIG_FILE`.

The application code is ordinary pjsip code. `embedded_ua.c` in `pjsip-apps/src/samples` is a complete example: it initialises pjlib, creates the endpoint, transport, dialog and invite layers, registers, handles calls and media, all from a single thread that polls the SIP endpoint and the media clock. It is portable C that also builds on Linux, Windows and macOS, which is how to develop and debug application logic before moving it to the board.

## 8. Configuration

### Kconfig options

The Kconfig options cover what the build has to know: whether to build pjproject at all, with threads, with TLS and with floating point. Everything else about pjproject, which codecs, how much logging, how many sockets an I/O queue watches, is set in `config_site.h` as on every other platform, so that each setting has one place. The options live under "PJSIP (pjproject)" in `west build -t menuconfig`, or in `prj.conf`:

| Option | Default | What it does |
|---|---|---|
| `CONFIG_PJPROJECT` | n | Build pjproject. Requires `POSIX_API`, `NETWORKING`, `NET_SOCKETS`, `NET_IPV4`. |
| `CONFIG_PJPROJECT_THREADS` | n | Build PJLIB with threads over pthreads. Off, PJLIB is single threaded and no pthread function is linked. See the note below. |
| `CONFIG_PJPROJECT_TLS` | n | SIP over TLS with Zephyr's Mbed TLS. Requires `MBEDTLS` and `NET_TCP`; the sample's `overlay-tls.conf` has the Mbed TLS options a TLS client needs. |
| `CONFIG_PJPROJECT_FLOATING_POINT` | follows `FPU` | Floating point arithmetic in PJLIB. Off saves the soft-float helpers on a core without an FPU. |

The module always builds the bundled libsrtp and libresample, and G.711, G.722 and L16 need no library; they are static archives, so an application that does not use one of them does not link it. The other codecs (Opus, Speex, iLBC, GSM, G.722.1, AMR), video, the echo cancellers and the sound device backends are not built. The sample does not need a sound device: media comes from the network and goes back to it.

A note on threads. The sample runs single threaded by default, which needs nothing special and is the smallest build. Build it with `-DEXTRA_CONF_FILE=overlay-threads.conf` and pjsip runs in one pthread and the media clock and RTP in another. The overlay shows what a threaded build needs besides `CONFIG_PJPROJECT_THREADS`: `CONFIG_DYNAMIC_THREAD` with heap-allocated stacks, since pjlib creates threads at run time; `CONFIG_DYNAMIC_THREAD_STACK_SIZE` raised to 16 KB, because a thread pjlib creates with stack size 0 gets that default and Zephyr's 1 KB is far too small; and more mutexes and condition variables than Zephyr's default of 5 each. A thread created with an explicit stack size gets a stack of that size from the same heap; Zephyr frees it only when the thread's attributes are destroyed after it has ended, which pjlib does in `pj_thread_join()` or `pj_thread_destroy()`, so join or destroy threads you create to get their stacks back. Zephyr keeps pthread key values only for threads created with `pthread_create()`, not for its main thread, so on Zephyr pjlib keeps its per-thread data in compiler thread-local storage instead (`PJ_THREAD_LOCAL_USE_COMPILER_TLS` in `os_zephyr.h`, `CONFIG_THREAD_LOCAL_STORAGE` selected by `CONFIG_PJPROJECT_THREADS`). The application can therefore use pjlib from `main()` as on any other platform. Zephyr takes that storage off the top of every thread's stack, kernel threads included, so each stack loses pjlib's share of about 270 bytes: the overlay raises `CONFIG_IDLE_STACK_SIZE` from its default of 256 bytes, which is smaller than that, and the net stack's small kernel stacks were checked to still have room.

### config_site.h

pjproject's compile-time settings beyond Kconfig are in `pjlib/include/pj/config_site.h` in the pjproject tree, the file every pjproject build uses and section 2 had you create. Nothing is generated: the Zephyr build reads that file as the Linux, Windows and macOS builds do. On Zephyr it must select the Zephyr preset, at minimum:

```c
#define PJ_CONFIG_ZEPHYR 1
#include <pj/config_site_sample.h>
```

The `PJ_CONFIG_ZEPHYR` preset in `pjlib/include/pj/config_site_sample.h` holds the Zephyr settings: G.711 only, SDES-only SRTP, no sample rate conversion, full logging, 16 sockets per I/O queue, no socket buffer tuning (the buffers are Zephyr's `net_buf` pools), G.711 computed instead of table driven, no video, no echo canceller, and the null sound device. It also sizes pjproject for a few calls, which is where the per-call figure in "What the port provides" comes from: hash tables for 15 transactions and dialogs and a 32-entry timer heap (all of which grow as needed), SIP messages of up to 2500 bytes, RTP packets of up to 512 bytes sent and 1000 received, SDP with up to 16 formats and 4 media lines, pools whose first block is a size class of the caching pool (4 KB dialogs and messages, 8 KB streams and endpoint) with 1 KB increments, and the lighter WSOLA in the jitter buffer. Read it to see exactly what is set; the sizes that an application may need larger (`PJSIP_MAX_PKT_LEN`, `PJMEDIA_MAX_MTU`, `PJMEDIA_MAX_MRU`, the SDP limits) are behind `#ifndef`, so a definition before the include overrides them. What is a fact of the platform rather than a choice, such as pjlib keeping its per-thread data in compiler thread-local storage, is in `os_zephyr.h` and applies whatever `config_site.h` says.

To change any of this, add to the same file. The codec, logging, I/O queue, SRTP and resampling settings take whatever is defined before the preset is included; the rest is overridden after it:

```c
#define PJMEDIA_HAS_G722_CODEC 1    /* G.722 as well as G.711 */
#define PJMEDIA_RESAMPLE_IMP PJMEDIA_RESAMPLE_LIBRESAMPLE  /* needed to switch between the two mid-call */
#define PJMEDIA_HAS_SRTP 0          /* plain RTP; libsrtp stays out of the image */
#define PJ_LOG_MAX_LEVEL 3          /* info and below */
#define PJ_IOQUEUE_MAX_HANDLES 32   /* CONFIG_ZVFS_OPEN_MAX must be at least this */

#define PJ_CONFIG_ZEPHYR 1
#include <pj/config_site_sample.h>

#undef PJSIP_MAX_TSX_COUNT
#define PJSIP_MAX_TSX_COUNT 15
```

A tree that also serves host builds can keep the Zephyr lines behind `#ifdef __ZEPHYR__`, which the Zephyr toolchain defines, so that the same `config_site.h` configures both.

The pjlib test application in `zephyr/tests/pjlib` needs a 64-socket I/O queue and gets it from a compile definition in its `CMakeLists.txt`, so that it builds with the plain preset above and leaves the tree's `config_site.h` to the user.

### The sample's own options

The sample's build settings are `CONFIG_EMBUA_*` macros that `embedded_ua.c` reads on every platform; on Zephyr they are options in the sample's Kconfig, set in `prj.conf`, in menuconfig or with `-DCONFIG_...` on the west command line. `CONFIG_EMBUA_SIP_SERVER_IP` (default 192.0.2.2), `CONFIG_EMBUA_USE_REGISTRATION` (1), `CONFIG_EMBUA_USERNAME` (bob) and `CONFIG_EMBUA_PASSWORD` (secret) describe the account. `CONFIG_EMBUA_AUDIO_CODEC` (1 for G.711, 2 for G.722, which also needs `PJMEDIA_HAS_G722_CODEC` in `config_site.h`), `CONFIG_EMBUA_USE_SRTP`, `CONFIG_EMBUA_SIP_TRANSPORT` (1 for UDP, 2 for TLS) and `CONFIG_EMBUA_USE_MEDIA_THREAD` follow the `CONFIG_PJPROJECT_*` options by default. The on/off settings are ints rather than Kconfig bools so that the macro is always defined. `CONFIG_EMBUA_HOST_TAP_IFACE` (zeth) is only used by `run-qemu.sh`. Without registration the sample is reached at `sip:<address>:55060`.

## 9. How the port is put together

The Zephyr-specific parts are few, and this is where to look when something needs changing:

- `zephyr/module.yml`: tells Zephyr this tree is a module and where its Kconfig and CMake are.
- `zephyr/Kconfig`: the `CONFIG_PJPROJECT_*` options above.
- `zephyr/CMakeLists.txt`: when `CONFIG_PJPROJECT` is set, checks that `config_site.h` selects the Zephyr preset, drives pjproject's own per-library `CMakeLists.txt` files with options derived from Kconfig, gives every library Zephyr's include paths and flags, and defines the `pjproject` link target.
- `zephyr/cmake/MbedTLSConfig.cmake`: lets pjlib's Mbed TLS backend find Zephyr's Mbed TLS.
- `pjlib/include/pj/compat/os_zephyr.h`: the platform description (which headers and socket functions exist, threads, TCP and TLS from Kconfig, no IPv6). `pj/config.h` selects it when `__ZEPHYR__` is defined. One of its settings, `PJ_THREAD_LOCAL_USE_COMPILER_TLS`, makes pjlib's thread-local storage use the compiler's `__thread` rather than pthread keys, which Zephyr does not provide to its main thread.
- `pjlib/src/pj/ip_helper_zephyr.c`: network interface and route enumeration through Zephyr's `net_if` API, since Zephyr has no `getifaddrs()`. This is how pjsip learns the address to put in Via and Contact headers.
- The `PJ_CONFIG_ZEPHYR` preset in `pjlib/include/pj/config_site_sample.h`, which sizes pjsip and pjmedia for a small endpoint; `config_site.h` selects it.
- `zephyr/tests/pjlib`: the pjlib-test application of section 5, with the `testcase.yaml` that twister runs and, in `boards/`, the heap and stack sizes its tests need on each board. Its sources are pjlib's own in `pjlib/src/pjlib-test`.
- `zephyr/snippets/pjproject-sim`: a [snippet](https://docs.zephyrproject.org/latest/build/snippets/index.html) that makes the emulated boards stand in for the 640 KB Cortex-M33 target: a devicetree overlay that limits the AN521's RAM to 640 KB, the 20 MHz SysTick of QEMU's model, and the host-seeded entropy source on native_sim. The sample and the test select it in their `CMakeLists.txt`; a `-S` on the command line replaces that selection, so give `-S pjproject-sim` too when adding another snippet. It is registered through `snippet_root` in `module.yml` and so available to any application that uses the module.
- `zephyr/samples/embedded_ua`: the sample's `CMakeLists.txt`, `Kconfig`, `prj.conf` and `run-qemu.sh`. The source is `pjsip-apps/src/samples/embedded_ua.c`, the same file that builds on Linux, Windows and macOS; on Zephyr it reads its settings from the `CONFIG_EMBUA_*` options.

Random numbers come from Zephyr's `sys_rand32_get()` rather than the C library's `rand()`, which is unseeded on Zephyr. Zephyr's `sockaddr_in` is 8 bytes rather than 16, which pjlib accommodates. Zephyr's `sockaddr_in6` differs from the BSD layout in ways pjlib does not yet accommodate, which is why IPv6 is off.

## 10. Known limitations

- IPv6 is disabled on Zephyr.
- QEMU's MPS2 boards drop full-size Ethernet frames: Zephyr's `smsc911x` driver rounds the frame length up before subtracting the checksum, asks for a buffer two bytes larger than the network stack allows for a 1514-byte frame, and the frame is lost. The first fragment of a SIP INVITE is such a frame. Until the driver is fixed, keep the host side of the TAP at MTU 1400 so that every frame is smaller. Real hardware and native_sim are not affected.
- QEMU's LAN9118 model stops delivering usable frames once the driver has skipped one. When Zephyr's `smsc911x` driver cannot get a receive buffer it tells the controller to skip the frame, and the model's FIFO accounting does not follow (the `RX_DP_CTRL` handler in QEMU's `hw/net/lan9118.c` leaves the packet size and the used count stale), so every later frame is misframed and dropped as a processing error while the host keeps sending. On QEMU the receive pools must therefore never run dry. Four INVITEs arriving together while the sample logs every SIP message at level 5 keep the main thread busy long enough for the RTP of the first calls to exhaust the default pools; the four-call measurement in "What the port provides" was made with `PJ_LOG_MAX_LEVEL` 3, `CONFIG_NET_PKT_RX_COUNT=96` and `CONFIG_NET_BUF_RX_COUNT=384`. Real hardware drops the frame and carries on.
- The QEMU boards have no entropy device, so the sample uses Zephyr's test random generator, seeded from the timer so that each boot differs. It is not a secure source; a real board must use its hardware RNG.
- QEMU's AN521 model clocks SysTick at 20 MHz, while Zephyr's board files declare the real board's 25 MHz. Left alone, the guest's clock runs at 80% of real time: the sample sends 40 RTP packets a second instead of 50, the audio from it breaks up, and pjsua reports RTTs of half a second. The `pjproject-sim` snippet therefore sets `CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC=20000000` for that board; an application of your own gets it from the snippet, and must not have it on the hardware. The AN385 model and native_sim are not affected.
- Even with the SysTick rate corrected, the AN521 guest's clock runs slightly slow under QEMU and the guest logs "Frame lost, recovered!" about once a second during a call, with an occasional "Internal RTCP NTP clock skew detected". Nothing is lost: pjsua reports zero loss and the jitter buffer's summary at the end of the call shows `discard=0`. The guest reads frames a little slower than they arrive, so the jitter buffer discards one now and then to keep its latency down and conceals the gap with PLC, which is what the message reports. The cause is QEMU's timer model losing time when the host deschedules the emulator, and it goes away with periodic kernel ticks (`CONFIG_TICKLESS_KERNEL=n` and `CONFIG_SYS_CLOCK_TICKS_PER_SEC=1000`); the sample does not set these because real hardware runs tickless, and the messages are harmless.
- Logging at a `PJ_LOG_MAX_LEVEL` below 5 produces a few unused-variable warnings in pjproject's own code. They are harmless.
- The sample's TLS transport has no certificate, so it cannot take inbound TLS connections; requests to it must arrive on the connection it registered over (section 3). That is the sample's choice, not the port's: with a certificate and key in `pjsip_tls_setting` the listener works as on any platform. The listener is nevertheless created: built with `PJSIP_TLS_TRANSPORT_DONT_CREATE_LISTENER` the sample registers, but Kamailio then opens a new connection to the Contact for every request instead of reusing the registration's, and that connection fails; with the listener present the requests arrive over the registration's connection.
- `exit()` on Zephyr does not end the program; after `quit`, the sample's final log line is followed by an idle loop. Stop the emulator by hand.

## 11. Troubleshooting

- "no Zephyr build in ..." from `run-qemu.sh`: give it the build directory you passed to `west build -d`.
- The guest never gets a response: check `ip addr show zeth` shows 192.0.2.2 and the interface is UP, and that no firewall drops traffic on it. On QEMU, check the MTU is 1400.
- A registrar answers the second REGISTER with 400 or "invalid CSeq": the random numbers repeat from one run to the next. On native_sim `CONFIG_NATIVE_EXTRA_CMDLINE_ARGS="-seed-random"` is missing from the build, which the `pjproject-sim` snippet provides; on QEMU, `CONFIG_TEST_RANDOM_GENERATOR` is missing from `prj.conf`.
- Audio from the QEMU guest breaks up, pjsua shows RTTs of hundreds of milliseconds and the guest logs "Frame lost, recovered!" throughout a call: the guest's clock is slow; see the SysTick entry in section 10. One "Frame lost, recovered!" a second with good audio and zero loss in pjsua is the harmless case described there.
- The build stops with "config_site.h is missing or does not select the Zephyr preset": create `pjlib/include/pj/config_site.h` in the pjproject tree as in section 2.
- The build stops with `PJ_IOQUEUE_MAX_HANDLES_must_not_exceed_FD_SETSIZE`: `FD_SETSIZE` on Zephyr is `CONFIG_ZVFS_OPEN_MAX`; raise it to at least `PJ_IOQUEUE_MAX_HANDLES` (16 in the preset).
- The image does not fit: in `config_site.h` lower `PJ_LOG_MAX_LEVEL`; in `prj.conf` shrink `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE`.
- A SIP INVITE arrives in the host log but never in the guest: `CONFIG_NET_IPV4_FRAGMENT` is off, or the fragment buffers (`CONFIG_NET_IPV4_FRAGMENT_MAX_PKT`, `CONFIG_NET_BUF_RX_COUNT`) are too few.
