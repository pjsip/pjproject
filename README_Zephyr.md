# PJSIP on Zephyr RTOS

This guide takes you from a clean machine to a SIP call running on Zephyr: first on `native_sim`, which runs Zephyr as an ordinary Linux process, then on a Cortex-M33 emulated by QEMU, and from there to your own application and board. It is the reference for the Zephyr port; docs.pjsip.org links to or copies this file.

## What the port provides

- pjproject is a Zephyr module. Add this source tree to a Zephyr application and PJLIB, PJLIB-UTIL, PJNATH, PJMEDIA and PJSIP are built with the application, configured through Kconfig like any other part of Zephyr.
- The port runs on Zephyr's POSIX layer: BSD sockets, `select()`, `clock_gettime()` and, optionally, pthreads. There is one Zephyr-specific source file in PJLIB (network interface enumeration) and one platform header. Everything else is the same code that runs on Linux.
- `embedded_ua`, a complete headless SIP user agent on pjsip and pjmedia without pjsua-lib, builds as a Zephyr sample. It registers to a SIP server (or not), supports TLS via Mbed TLS, answers calls with audio loopback, speaks SDES-SRTP, and is driven by SIP MESSAGE requests. It is the bring-up and reference application of the port.
- Tested targets: `native_sim/native/64` and `mps2/an521/cpu0` (Cortex-M33 under QEMU, limited to 640 KB of RAM to match an STM32H563). Both register and complete calls with zero packet loss.
- pjlib-test, PJLIB's own test suite, runs on both targets through twister, single threaded and with threads.

Current status: SIP over UDP, G.711, G.722 and SDES-SRTP are working, single threaded and with pjsip and the media on two pthreads. SIP over TLS with Zephyr's Mbed TLS is the next step and is described below as such.

## 1. Install Zephyr

If you already have a working Zephyr workspace and SDK, skip to section 2. Otherwise follow the [Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html); the steps below are its Ubuntu version, condensed.

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

## 2. Get pjproject

Clone pjproject anywhere; it does not need to be inside the Zephyr workspace:

```sh
git clone https://github.com/pjsip/pjproject.git ~/pjproject
```

The rest of this guide writes `~/pjproject` for that location. No `./configure` or `make` is needed: the Zephyr build compiles pjproject from source as part of the application.

## 3. Build and run the sample on native_sim

`native_sim` is a Zephyr board that is a Linux executable. It runs the real Zephyr kernel, network stack and POSIX layer, talks to the host through a TAP interface, and can be debugged with gdb. It is the fastest way to see the port working.

### Create the TAP interface

The emulated board needs a network link to the host. Create it once (it disappears on reboot):

```sh
sudo ip tuntap add dev zeth mode tap user $USER
sudo ip addr add 192.0.2.2/24 dev zeth
sudo ip link set zeth mtu 1400 up
```

The host is 192.0.2.2 and the Zephyr side is 192.0.2.1; both addresses are in the sample's `prj.conf`. The MTU of 1400 is a workaround for a QEMU driver bug described under Known limitations; it does no harm to native_sim. Zephyr's own `tools/net-tools/net-setup.sh` creates the same interface, if you prefer it.

### Build

```sh
cd ~/zephyrproject
west build -p always -b native_sim/native/64 -d build-native ~/pjproject/zephyr/samples/embedded_ua
```

`-p always` asks for a clean build directory, `-b` is the board, `-d` the build directory. The first build compiles all of pjproject and takes a few minutes; later builds are incremental.

The sample is set up to register with a SIP server at 192.0.2.2, the host side of the TAP interface. For a first run without any server, build it without registration instead:

```sh
west build -p always -b native_sim/native/64 -d build-native ~/pjproject/zephyr/samples/embedded_ua -- -DCONFIG_EMBUA_USE_REGISTRATION=0
```

Everything after `--` goes to CMake; this is how any Kconfig option is set from the command line.

### Run

```sh
build-native/zephyr/zephyr.exe -seed-random
```

You will see Zephyr boot, the network interface come up, and pjsip start its UDP transport on 192.0.2.1:55060. Leave it running. `-seed-random` matters: native_sim's fake entropy source otherwise starts from the same seed each run, so every run would have the same Call-ID and CSeq, which a SIP registrar rejects as a replay. Stop it with Ctrl-C.

### Make a call to it

Any SIP client on the host can call the sample. The easiest is pjsua from a normal Linux build of pjproject (`./configure && make` in a second checkout, or the same one; the Zephyr build does not interfere with it). The sample insists on SRTP, so SRTP has to be allowed over plain UDP:

```sh
cd ~/pjproject/pjsip-apps/bin
./pjsua-x86_64-pc-linux-gnu --no-tcp --local-port 5062 --use-srtp=2 --srtp-secure=0 --play-file ../../tests/pjsua/wavs/input.16.wav --auto-play
```

At the pjsua prompt type `m`, then the URI `sip:bob@192.0.2.1:55060`. The sample answers after one ring. The Zephyr side logs every SIP message, so you can watch the INVITE, the SDP negotiation and the SRTP setup go by. `--play-file` with `--auto-play` makes pjsua stream the WAV into the call as soon as it connects, and the sample loops whatever it receives straight back. To hear that loopback, connect the call to the sound device: type `cl` to list the conference ports, then `cc`, the call's port number as source and `0` (the sound device) as destination. The WAV then comes out of your speaker after a round trip through the Zephyr guest. `dq` in pjsua prints the call quality and `h` hangs up.

### With a SIP server

With registration enabled (the default), the sample registers as `bob` with password `secret` to the server in `CONFIG_EMBUA_SIP_SERVER_IP`. Point it at your server:

```sh
west build -p always -b native_sim/native/64 -d build-native ~/pjproject/zephyr/samples/embedded_ua -- -DCONFIG_EMBUA_SIP_SERVER_IP=\"192.168.1.19\"
```

The server can be anywhere the host can reach: the host forwards packets from the TAP interface as it does for any other network, and a server on the host itself answers directly. The port was developed against Kamailio; any registrar that does digest authentication will do. Create an account `bob`/`secret` on it, or change `CONFIG_EMBUA_USERNAME` and `CONFIG_EMBUA_PASSWORD`. The log shows the REGISTER, the 401 challenge, the authenticated REGISTER and the 200 OK; after that the sample is reachable by calling `bob` through the server.

### Driving it with SIP MESSAGE

embedded_ua has no console input. Send it a SIP MESSAGE (in pjsua: add it as a buddy and use `i`) with one of these bodies: `call <uri>` makes it place a call, `mem` logs its pool usage, `quit` ends every call, unregisters and exits.

## 4. Run on a Cortex-M33 under QEMU

The MPS2 AN521 is an Arm development board that QEMU emulates, with a Cortex-M33 and a LAN9118 Ethernet controller. The sample limits it to 640 KB of RAM, the size of the STM32H563, so that an image that would not fit the real part fails to link here.

Build:

```sh
cd ~/zephyrproject
west build -p always -b mps2/an521/cpu0 -d build-an521 ~/pjproject/zephyr/samples/embedded_ua
```

The same `-DCONFIG_...` options as above apply. The end of the build prints the memory use, for example:

```
Memory region         Used Size  Region Size  %age Used
           FLASH:      352272 B         4 MB      8.40%
             RAM:      347384 B       640 KB     53.01%
```

That is the whole image, Zephyr and its network stack included; 256 KB of the RAM is the heap arena pjlib's pools are carved from.

Run it with the launcher that comes with the sample, giving it the build directory:

```sh
~/pjproject/zephyr/samples/embedded_ua/run-qemu.sh build-an521
```

The script finds QEMU in the SDK, attaches the board's Ethernet to the TAP interface named in `CONFIG_EMBUA_HOST_TAP_IFACE` (`zeth` by default) and boots the image. Extra arguments are passed to QEMU. From the host's point of view the board is at 192.0.2.1 exactly like native_sim, so the calls in section 3 work unchanged. Stop QEMU with Ctrl-A then X.

`west build -t run`, the usual way to run a Zephyr image in QEMU, does not work for the MPS2 boards: their Ethernet controller is part of the machine, Zephyr nevertheless adds it as a pluggable `-device lan9118`, and QEMU refuses. Hence the script.

## 5. Run the pjlib test suite

pjlib-test, PJLIB's own test suite, builds as a Zephyr test application in `zephyr/tests/pjlib`. It exercises the OS abstraction that a port touches most: pools, threads and thread-local storage, mutexes, timers, sockets over loopback, select and the ioqueue. Its `testcase.yaml` lets twister build and run it on native_sim, single threaded and with threads, checking the console for the suite's final line, and build it for the AN521:

```sh
cd ~/zephyrproject
west twister -T ~/pjproject/zephyr/tests/pjlib -p native_sim/native/64 -p mps2/an521/cpu0 -v
```

A run takes a few minutes per configuration. To build and run one configuration by hand:

```sh
west build -p always -b native_sim/native/64 -d build-test ~/pjproject/zephyr/tests/pjlib -- -DEXTRA_CONF_FILE=overlay-threads.conf
build-test/zephyr/zephyr.exe -seed-random
```

On the AN521 twister only builds the image, for the reason given in section 4: Zephyr puts the board's Ethernet controller on the QEMU command line in a form QEMU rejects, and that happens for every image with networking enabled. Run it with the sample's launcher, telling QEMU to attach no network since the tests only use loopback:

```sh
west build -p always -b mps2/an521/cpu0 -d build-test-an521 ~/pjproject/zephyr/tests/pjlib
~/pjproject/zephyr/samples/embedded_ua/run-qemu.sh build-test-an521 -nic none
```

The suite runs with `--ci-mode`, which widens its timing tolerances for emulators, and with `-w 0`, so that the tests run one at a time and a board only needs memory for one test's threads. What differs from a desktop run follows from the test profile in `pjlib/src/pjlib-test/test_profile.h`, which selects `PJTEST_SMALL_FOOTPRINT` on any Zephyr board with less than 2 MB of RAM, and from the test application's `prj.conf`:

- The benchmarks (pool, socket and ioqueue performance) are excluded in the small profile, where they would need far more memory than 640 KB, and on native_sim, where they would measure a clock that stands still while the CPU is busy.
- In the small profile the timer test runs at an eighth of its size (`PJTEST_TIMER_LOOP`): 500 entries and 2 stress threads rather than 4000 and 20, so that its pools fit in the heap arena. native_sim runs it in full.
- The file test is skipped, since the application mounts no file system.
- The SSL socket tests are not built, as the test application does not enable `CONFIG_PJPROJECT_TLS`.
- Zephyr's `socketpair()` only supports AF_UNIX stream sockets, so the datagram variant of that test reports itself as skipped.
- On native_sim only, the timestamp test, the ioqueue unregistration test and the parts of the thread test that keep threads spinning are skipped: native_sim's simulated clock stands still while the CPU is busy, so a test that busy-waits on the clock, or polls a socket that is never idle, never reaches its end time. They run on the AN521 and on every real board.

The AN521 configuration enables `CONFIG_HW_STACK_PROTECTION`, so a stack overflow faults instead of corrupting memory silently. It caught two: the activesock test keeps 100 operation keys on the main thread's stack, so the suite needs a 32 KB main stack, and Zephyr's `poll()` keeps one event per possible descriptor on the stack, which with `CONFIG_ZVFS_POLL_MAX` at 88 outgrows the 1200-byte default stack of the socket service thread the DNS resolver uses. Keep both in mind when sizing an application: pjlib itself needs neither.

Everything else runs unchanged and passes on both targets in both thread modes.

## 6. Run on real hardware

Build for your board and flash as with any Zephyr application:

```sh
west build -p always -b <your board> ~/pjproject/zephyr/samples/embedded_ua
west flash
```

Things to adjust in the sample for a real board, in a `boards/<your board>.conf` file next to the sample's or in your own copy of `prj.conf`:

- Addressing. The sample uses a static address on the TAP link. On a real network, drop the `CONFIG_NET_CONFIG_MY_IPV4_*` lines and enable `CONFIG_NET_DHCPV4=y`.
- Random numbers. Remove `CONFIG_TEST_RANDOM_GENERATOR` and let the board's hardware RNG serve `CONFIG_ENTROPY_GENERATOR`. SIP Call-IDs, tags and SRTP keys all come from it.
- Memory. `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE` is the heap pjlib allocates its pools from; 256 KB is comfortable for a few calls with logging on. `CONFIG_MAIN_STACK_SIZE` of 16 KB covers pjsip's message parsing on the main thread.
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
CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=262144
CONFIG_ENTROPY_GENERATOR=y
```

The sample's `zephyr/samples/embedded_ua/prj.conf` is a complete, commented example and the best starting point; copy it and remove what you do not need. Two of its settings deserve a word. `CONFIG_NET_IPV4_FRAGMENT=y` is there because a SIP INVITE with SDP is usually larger than the Ethernet MTU and arrives as IP fragments. `CONFIG_ZVFS_OPEN_MAX` bounds the file descriptors `select()` can watch and must be at least `CONFIG_PJPROJECT_IOQUEUE_MAX_HANDLES`; the build checks this with a static assertion.

The application code is ordinary pjsip code. `embedded_ua.c` in `pjsip-apps/src/samples` is a complete example: it initialises pjlib, creates the endpoint, transport, dialog and invite layers, registers, handles calls and media, all from a single thread that polls the SIP endpoint and the media clock. It is portable C that also builds on Linux, Windows and macOS, which is how to develop and debug application logic before moving it to the board.

## 8. Configuration

### Kconfig options

All pjproject options live under "PJSIP (pjproject)" in `west build -t menuconfig`, or in `prj.conf`:

| Option | Default | What it does |
|---|---|---|
| `CONFIG_PJPROJECT` | n | Build pjproject. Requires `POSIX_API`, `NETWORKING`, `NET_SOCKETS`, `NET_IPV4`. |
| `CONFIG_PJPROJECT_THREADS` | n | Build PJLIB with threads over pthreads. Off, PJLIB is single threaded and no pthread function is linked. See the note below. |
| `CONFIG_PJPROJECT_TLS` | n | SIP over TLS with Zephyr's Mbed TLS. Requires `MBEDTLS` and `NET_TCP`. In progress. |
| `CONFIG_PJPROJECT_SRTP` | y | SDES-SRTP with the bundled libsrtp. |
| `CONFIG_PJPROJECT_CODEC_G711` | y | G.711 PCMU and PCMA. |
| `CONFIG_PJPROJECT_CODEC_G722` | y | G.722 wideband. |
| `CONFIG_PJPROJECT_CODEC_L16` | n | L16 linear PCM. |
| `CONFIG_PJPROJECT_RESAMPLE` | y | The bundled resampler, needed when codecs of different clock rates are both enabled. |
| `CONFIG_PJPROJECT_FLOATING_POINT` | follows `FPU` | Floating point arithmetic in PJLIB. Off saves the soft-float helpers on a core without an FPU. |
| `CONFIG_PJPROJECT_LOG_MAX_LEVEL` | 5 | `PJ_LOG_MAX_LEVEL`. 0 compiles all logging out; 3 is info, 4 debug, 5 and 6 trace. Lower values save flash. |
| `CONFIG_PJPROJECT_IOQUEUE_MAX_HANDLES` | 16 | `PJ_IOQUEUE_MAX_HANDLES`, the sockets one I/O queue can watch. |
| `CONFIG_PJPROJECT_CONFIG_SITE` | empty | Path to a custom `config_site.h`, see below. |

The other codecs (Opus, Speex, iLBC, GSM, G.722.1, AMR), video, the echo cancellers and the sound device backends are not built. The sample does not need a sound device: media comes from the network and goes back to it.

A note on threads. The sample runs single threaded by default, which needs nothing special and is the smallest build. Build it with `-DEXTRA_CONF_FILE=overlay-threads.conf` and pjsip runs in one pthread and the media clock and RTP in another. The overlay shows what a threaded build needs besides `CONFIG_PJPROJECT_THREADS`: `CONFIG_DYNAMIC_THREAD` with heap-allocated stacks, since pjlib creates threads at run time; `CONFIG_DYNAMIC_THREAD_STACK_SIZE` raised to 16 KB, because a thread pjlib creates with stack size 0 gets that default and Zephyr's 1 KB is far too small; and more mutexes and condition variables than Zephyr's default of 5 each. A thread created with an explicit stack size gets a stack of that size from the same heap; Zephyr frees it only when the thread's attributes are destroyed after it has ended, which pjlib does in `pj_thread_join()` or `pj_thread_destroy()`, so join or destroy threads you create to get their stacks back. Zephyr keeps pthread key values only for threads created with `pthread_create()`, not for its main thread, so on Zephyr pjlib keeps its per-thread data in compiler thread-local storage instead (`PJ_THREAD_LOCAL_USE_COMPILER_TLS` in the preset, `CONFIG_THREAD_LOCAL_STORAGE` selected by `CONFIG_PJPROJECT_THREADS`). The application can therefore use pjlib from `main()` as on any other platform. Zephyr takes that storage off the top of every thread's stack, kernel threads included, so each stack loses pjlib's share of about 270 bytes: the overlay raises `CONFIG_IDLE_STACK_SIZE` from its default of 256 bytes, which is smaller than that, and the net stack's small kernel stacks were checked to still have room.

### config_site.h

pjproject's compile-time settings beyond Kconfig are in `config_site.h`. The Zephyr module generates one that reads:

```c
#define PJ_CONFIG_ZEPHYR 1
#include <pj/config_site_sample.h>
```

The `PJ_CONFIG_ZEPHYR` preset in `pjlib/include/pj/config_site_sample.h` holds the Zephyr settings: logging and I/O queue sizes from Kconfig, 31 transactions and dialogs, no socket buffer tuning (Zephyr has no `SO_SNDBUF`), the codec selection from Kconfig, G.711 computed instead of table driven, no video, no echo canceller, the null sound device, and SDES-only SRTP. Read it to see exactly what is set.

To change any of this, write your own `config_site.h` and point `CONFIG_PJPROJECT_CONFIG_SITE` at it (an absolute path, or relative to the application directory). Start it with the same two lines so the preset stays in effect, then override:

```c
#define PJ_CONFIG_ZEPHYR 1
#include <pj/config_site_sample.h>

#undef PJSIP_MAX_TSX_COUNT
#define PJSIP_MAX_TSX_COUNT 15
```

### The sample's own options

The sample's build settings are `CONFIG_EMBUA_*` macros that `embedded_ua.c` reads on every platform; on Zephyr they are options in the sample's Kconfig, set in `prj.conf`, in menuconfig or with `-DCONFIG_...` on the west command line. `CONFIG_EMBUA_SIP_SERVER_IP` (default 192.0.2.2), `CONFIG_EMBUA_USE_REGISTRATION` (1), `CONFIG_EMBUA_USERNAME` (bob) and `CONFIG_EMBUA_PASSWORD` (secret) describe the account. `CONFIG_EMBUA_AUDIO_CODEC` (1 for G.711, 2 for G.722), `CONFIG_EMBUA_USE_SRTP`, `CONFIG_EMBUA_SIP_TRANSPORT` (1 for UDP, 2 for TLS) and `CONFIG_EMBUA_USE_MEDIA_THREAD` follow the `CONFIG_PJPROJECT_*` options by default. The on/off settings are ints rather than Kconfig bools so that the macro is always defined. `CONFIG_EMBUA_HOST_TAP_IFACE` (zeth) is only used by `run-qemu.sh`. Without registration the sample is reached at `sip:<address>:55060`.

## 9. How the port is put together

The Zephyr-specific parts are few, and this is where to look when something needs changing:

- `zephyr/module.yml`: tells Zephyr this tree is a module and where its Kconfig and CMake are.
- `zephyr/Kconfig`: the `CONFIG_PJPROJECT_*` options above.
- `zephyr/CMakeLists.txt`: when `CONFIG_PJPROJECT` is set, drives pjproject's own per-library `CMakeLists.txt` files with options derived from Kconfig, generates `config_site.h`, gives every library Zephyr's include paths and flags, and defines the `pjproject` link target.
- `zephyr/cmake/MbedTLSConfig.cmake`: lets pjlib's Mbed TLS backend find Zephyr's Mbed TLS.
- `pjlib/include/pj/compat/os_zephyr.h`: the platform description (which headers and socket functions exist, threads, TCP and TLS from Kconfig, no IPv6). `pj/config.h` selects it when `__ZEPHYR__` is defined.
- `pjlib/src/pj/ip_helper_zephyr.c`: network interface and route enumeration through Zephyr's `net_if` API, since Zephyr has no `getifaddrs()`. This is how pjsip learns the address to put in Via and Contact headers.
- The `PJ_CONFIG_ZEPHYR` preset in `pjlib/include/pj/config_site_sample.h`. One of its settings, `PJ_THREAD_LOCAL_USE_COMPILER_TLS`, makes pjlib's thread-local storage use the compiler's `__thread` rather than pthread keys, which Zephyr does not provide to its main thread.
- `zephyr/tests/pjlib`: the pjlib-test application of section 5, with the `testcase.yaml` that twister runs. Its sources are pjlib's own in `pjlib/src/pjlib-test`.
- `zephyr/samples/embedded_ua`: the sample's `CMakeLists.txt`, `Kconfig`, `prj.conf`, board overlay and `run-qemu.sh`. The source is `pjsip-apps/src/samples/embedded_ua.c`, the same file that builds on Linux, Windows and macOS; on Zephyr it reads its settings from the `CONFIG_EMBUA_*` options.

Random numbers come from Zephyr's `sys_rand32_get()` rather than the C library's `rand()`, which is unseeded on Zephyr. Zephyr's `sockaddr_in` is 8 bytes rather than 16, which pjlib accommodates. Zephyr's `sockaddr_in6` differs from the BSD layout in ways pjlib does not yet accommodate, which is why IPv6 is off.

## 10. Known limitations

- IPv6 is disabled on Zephyr.
- QEMU's MPS2 boards drop full-size Ethernet frames: Zephyr's `smsc911x` driver rounds the frame length up before subtracting the checksum, asks for a buffer two bytes larger than the network stack allows for a 1514-byte frame, and the frame is lost. The first fragment of a SIP INVITE is such a frame. Until the driver is fixed, keep the host side of the TAP at MTU 1400 so that every frame is smaller. Real hardware and native_sim are not affected.
- The QEMU boards have no entropy device, so the sample uses Zephyr's test random generator, seeded from the timer so that each boot differs. It is not a secure source; a real board must use its hardware RNG.
- Logging at `CONFIG_PJPROJECT_LOG_MAX_LEVEL` below 5 produces a few unused-variable warnings in pjproject's own code. They are harmless.
- `exit()` on Zephyr does not end the program; after `quit`, the sample's final log line is followed by an idle loop. Stop the emulator by hand.

## 11. Troubleshooting

- "no Zephyr build in ..." from `run-qemu.sh`: give it the build directory you passed to `west build -d`.
- The guest never gets a response: check `ip addr show zeth` shows 192.0.2.2 and the interface is UP, and that no firewall drops traffic on it. On QEMU, check the MTU is 1400.
- A registrar answers the second REGISTER with 400 or "invalid CSeq": on native_sim, you forgot `-seed-random`; on QEMU, `CONFIG_TIMER_RANDOM_GENERATOR` is missing from the build.
- The build stops with `PJ_IOQUEUE_MAX_HANDLES_must_not_exceed_FD_SETSIZE`: `FD_SETSIZE` on Zephyr is `CONFIG_ZVFS_OPEN_MAX`; raise it to at least `CONFIG_PJPROJECT_IOQUEUE_MAX_HANDLES`.
- The image does not fit: lower `CONFIG_PJPROJECT_LOG_MAX_LEVEL`, shrink `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE`, turn off G.722 and the resampler if only G.711 is needed.
- A SIP INVITE arrives in the host log but never in the guest: `CONFIG_NET_IPV4_FRAGMENT` is off, or the fragment buffers (`CONFIG_NET_IPV4_FRAGMENT_MAX_PKT`, `CONFIG_NET_BUF_RX_COUNT`) are too few.
