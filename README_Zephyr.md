# PJSIP on Zephyr RTOS

This guide takes you from a clean machine to a SIP call running on [Zephyr RTOS](https://www.zephyrproject.org/): first on [`native_sim`](https://docs.zephyrproject.org/latest/boards/native/native_sim/doc/index.html), which runs Zephyr as an ordinary Linux process, then on a [Cortex-M33](https://developer.arm.com/Processors/Cortex-M33) emulated by QEMU, then on the [NUCLEO-H563ZI](https://www.st.com/en/evaluation-tools/nucleo-h563zi.html) board, and from there to your own application and board. It is the reference for the Zephyr port; [docs.pjsip.org](https://docs.pjsip.org) links to or copies this file.

## What the port provides

- pjproject is a [Zephyr module](https://docs.zephyrproject.org/latest/develop/modules.html). Add this source tree to a Zephyr application and PJLIB, PJLIB-UTIL, PJNATH, PJMEDIA, PJSIP and pjsua-lib are built with the application, configured through Kconfig like any other part of Zephyr.
- The port runs on Zephyr's POSIX layer: BSD sockets, `select()`, `clock_gettime()` and, optionally, pthreads.
- [`embedded_ua.c`](pjsip-apps/src/samples/embedded_ua.c) as a Zephyr sample, based on pjsip and pjmedia without pjsua-lib. It supports four concurrent calls,  registration, TLS via [Mbed TLS](https://www.trustedfirmware.org/projects/mbed-tls/), audio loopback, and SDES-SRTP.
- [`simple_pjsua.c`](pjsip-apps/src/samples/simple_pjsua.c), the pjsua-lib sample. It registers, answers calls on the null sound device and makes one when told to, sized for one call at a time.
- Tested targets:

  - `native_sim/native/64`, Zephyr as a Linux process
  - `mps2/an521/cpu0`, a Cortex-M33 under QEMU, limited to 640 KB of RAM to stand in for the STM32H563
  - `nucleo_h563zi/stm32h563xx`, the NUCLEO-H563ZI board (STM32H563ZI, Cortex-M33 at 250 MHz, 640 KB of RAM, 2 MB of flash)
- The five test suites, pjlib-test, pjlib-util-test, pjnath-test, pjmedia-test and pjsip-test, build as Zephyr test applications with twister entries. All five pass on the three targets, single threaded and with threads, with one exception: the threaded pjsip-test does not fit 640 KB (section 5).
- A CI workflow builds every suite and sample for native_sim and the AN521 and runs pjlib-test on every pull request (section 5).

### Current status

- Registration and four concurrent calls
- SIP transport: UDP or TLS
- Audio codec: G.711 or G.722
- SDES-SRTP
- Single threaded or one worker thread for media


The standard pjsip and pjmedia feature set are working: RFC 3261, registration refresh, digest authentication (RFC 2617), rport (RFC 3581), SDP offer/answer (RFC 3264) with the RTCP (RFC 3605), re-INVITE and UPDATE (RFC 3311), PRACK (RFC 3262), call hold and resume, mid-call codec change, SIP MESSAGE (RFC 3428), RTCP reports and quality statistics, DTMF (RFC 4733), adaptive jitter buffer (200 ms by default, `CONFIG_EMBUA_JB_SIZE`), packet loss concealment, VAD with silence suppression, symmetric RTP and ptime negotiation.

The rest of the libraries compiles too, among them SUBSCRIBE/NOTIFY and PUBLISH, session timers, REFER, the conference bridge and the WAV ports, but *has not been exercised on Zephyr* beyond what the test suites of section 5 cover (they run STUN, TURN, ICE and the conference bridge over loopback on native_sim), and there is no Zephyr sound device backend.

The preset builds G.711 only; G.722 and the resampler are a `config_site.h` setting away.


## Footprint

The sizes of the whole embedded_ua image on the AN521, Zephyr and its network stack included, for the four ways the sample builds, G.711 with SDES-SRTP, at `-Os`, Zephyr's default optimisation level ("Optimisation level" below). How to read them:

- The figures are the FLASH and RAM lines that `west build` prints at the end of the link, rounded to KB.
- RAM is the static allocation: data and BSS, including the heap arena, the kernel heap and the thread stacks. pjproject's pools come out of the arena, so they are inside this number, not on top of it.
- The four-call column is the sample as it ships: the heap arena at the measured peak of four calls arriving at once plus 5% for the heap's own bookkeeping, and a receive pool that holds four INVITEs at once.
- The one-call column is the same build sized for a single call, the arena at the one-call peak plus 5% and the receive pool at 40 packets and 80 buffers; each was verified with a call on the emulator.

| Build | Flash | RAM, one call | RAM, four calls |
|---|---|---|---|
| SIP over UDP, single thread | 342 KB | 178 KB | 371 KB |
| SIP over UDP, threads | 351 KB | 216 KB | 411 KB |
| SIP over TLS, single thread | 450 KB | 290 KB | 492 KB |
| SIP over TLS, threads | 460 KB | 329 KB | 531 KB |

Most of the RAM is what the configuration sets aside rather than pjproject's own data: the heap arena for pjlib's pools, the kernel heap the media thread's stack comes from in the threaded build, and about 80 KB of network buffers, most of it the receive pool for four INVITEs at once (the LAN9118 note under Known limitations). The image is the same size however many calls are active, since calls draw on the arena at run time.

What the arena holds, measured on the AN521 in the single-threaded UDP build with the sample's `mem` command (section 3) and `CONFIG_SYS_HEAP_RUNTIME_STATS=y`:

| State | Heap in use |
|---|---|
| Registered, idle | 26 KB |
| Each established call | about 46 KB |
| Four concurrent calls | 218 KB, peak 226 KB while they were set up |

On the NUCLEO-H563ZI the heap behaves as on the emulator: the same pools from the same arena, and the four-call peak is the same number of bytes in the UDP builds and within 400 bytes in the TLS builds. The images are larger by Zephyr's drivers for the board and the DHCP client. The board's four images as they ship, each with the heap's peak while **four** INVITEs arrived at once and the **four** calls ran, the AN521's peak in the same test, and the arena it has to fit in:

| Build | Flash | RAM | Heap peak, four calls | Same on AN521 | Arena |
|---|---|---|---|---|---|
| SIP over UDP, single thread | 360 KB | 382 KB | 226 KB | 226 KB | 236 KB |
| SIP over UDP, threads | 368 KB | 422 KB | 244 KB | 244 KB | 257 KB |
| SIP over TLS, single thread | 465 KB | 503 KB | 278 KB | 279 KB | 293 KB |
| SIP over TLS, threads | 474 KB | 542 KB | 297 KB | 297 KB | 312 KB |

So the arena has 10 to 15 KB to spare at the four-call peak; an application with less margin than the sample should measure on its own board, with `mem`, before trimming it. In the threaded builds the kernel heap holds the media thread's stack and 1 KB is left, as intended: nothing else is allocated from it after start-up.

> [!NOTE]
> **G.722 needs a larger arena.** These arenas are sized for G.711. With G.722 (section 8) each call's jitter buffer and frames are for 16 kHz, and the same four-call burst on the board peaked at 240 KB, and at 243 KB when a call had ended a few seconds before (its pools linger until the transaction timers run out), past the UDP arena: with it the sample aborted on "No memory" at the fourth call. For G.722 set `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE` to 256 KB over UDP without threads, verified, and add the same 16 KB to the other three.

Each reading includes a few KB for the SIP transactions still timing out, among them the MESSAGE that asked for it, so the steady idle figure is about 20 KB: the SIP endpoint, the transport, the media endpoint and the registration. What a call is made of, nearly all of it pool memory:

- the media stream with its jitter buffer, 8 KB
- the dialog and INVITE session, 12 KB
- the 200 OK kept for retransmission, 7 KB
- the UDP media transport, 6 KB, and SRTP, 4 KB, plus libsrtp's own allocations, 3.5 KB
- the call's own pool, codec, locks and INVITE transaction, 5 KB

These figures are with the preset's sizing (section 8): with pjproject's stock pool sizes, packet buffers and SDP limits a call took 82 KB and four calls peaked at 436 KB.

### pjsua-lib

pjsua-lib is supported, with [simple_pjsua](pjsip-apps/src/samples/simple_pjsua.c) as its sample (section 3): a user agent on pjsua-lib's null sound device, with threads, sized and tested for one call at a time. The build can take the same features as embedded_ua's, TLS, SRTP and G.722 among them, through the same Kconfig and `config_site.h` settings (section 8); the figures here are the baseline with G.711 over UDP and no SRTP. Its images as they ship, and embedded_ua's over UDP with threads on the same targets, the nearest build above:

| Image | Flash | RAM |
|---|---|---|
| simple_pjsua, AN521 | 609 KB | 357 KB |
| simple_pjsua, NUCLEO-H563ZI | 626 KB | 368 KB |
| embedded_ua over UDP with threads, AN521 | 351 KB | 411 KB |
| embedded_ua over UDP with threads, NUCLEO-H563ZI | 368 KB | 422 KB |

The extra flash is pjsua-lib itself: presence, REFER, instant messaging, the conference bridge, the players and recorders and the rest of its API. The image has less RAM only because it is sized for one call where embedded_ua's is sized for four: a 144 KB heap arena, a 64 KB kernel heap for three 20 KB thread stacks, and 9 KB of static pjsua-lib state with the preset's limits (section 3). Where the RAM goes at run time, from the heap statistics the sample logs every ten seconds when built with `CONFIG_SYS_HEAP_RUNTIME_STATS=y`, the same bytes on the AN521 and on the board:

| State | Heap arena in use | Arena peak so far | Kernel heap in use |
|---|---|---|---|
| Registered, idle | 54 KB | 66 KB, during start-up and registration | 41 KB |
| One call up | 119 KB | 135 KB, while the call was set up | 61 KB |
| After the hangup | 68 KB, 10 s later | 138 KB, at the call's teardown | 41 KB |

So a call takes 65 KB of the arena, against embedded_ua's 46 KB: pjsua-lib's call and media state, the conference bridge port and the null sound device's master port come on top of the dialog, INVITE session, stream and transport that both have. The kernel heap holds the stacks of pjsua's worker thread and pjmedia's event thread while idle, and the null sound device's clock thread while a call is up. A second concurrent call needs the arena grown by that 65 KB plus the 5% margin, and has not been tested.

### Optimisation level

The footprint figures above are at `-Os`, Zephyr's default (`CONFIG_SIZE_OPTIMIZATIONS`); the CPU benchmarks below are at `-O2` (`CONFIG_SPEED_OPTIMIZATIONS=y`), which is what an application that runs G.722 or resamples should use. What changes between the two, measured on the board:

| | `-Os` | `-O2` |
|---|---|---|
| Flash, pjsip-test benchmark image | 327 KB | 373 KB, 14% more |
| Flash, pjmedia-test benchmark image | | 57 KB more |
| RAM | 610,032 B | 610,056 B |
| G.722 encode and decode | 16.9% of the core | 11.9%, a third less |

- RAM is the static allocation, which the optimiser does not touch; the 24 bytes are alignment.
- Stack depth changes with inlining, so a stack sized tightly at one level deserves a look with the thread analyzer at the other.
- Heap use is the same bytes either way.

## Processing cost

### Media

What the media components cost on the NUCLEO-H563ZI, a Cortex-M33 at 240 MHz, measured with pjmedia-test's `mips_test` (section 5): the share of the core that one instance of each takes per second of audio. Both optimisation levels, where measured:

| Component | `-O2` | `-Os` |
|---|---|---|
| G.711 encode and decode | 0.37% | 0.49% |
| G.711 stream, plain RTP | 0.48% | 0.72% |
| G.711 stream, SRTP AES-CM-128 with 80-bit HMAC-SHA1 | 1.94% | 2.12% |
| G.722 encode and decode | 11.9% | 16.9% |
| G.722 stream, plain RTP | 12.0% | |
| Jitter buffer WSOLA at 16 kHz, no loss / 5% loss | 0.07% / 0.22% | 0.29% / 0.48% |
| Resampling 8 to 16 kHz and back, linear | 0.31% | |
| Resampling 8 to 16 kHz and back, libresample small filter | 4.9% | |
| Resampling 8 to 16 kHz and back, libresample large filter | 29% | |
| Resampling 16 to 32 kHz and back, libresample small filter | 9.8% | |
| Conference bridge, 16 ports at 16 kHz | 5.4% | 10.5% |
| Echo suppressor, 100 ms tail, 16 kHz | 0.47% | 0.96% |
| Tone generator, two frequencies, 8 kHz | 1.3% | 1.56% |

Note: a stream row already includes its codec, the RTP framing and, where named, the SRTP, so the cost of one call is the figure in the matching stream row, with nothing to add.

What this means for a call:

- A G.711 call with SRTP costs about 2% of the core; a G.722 call about 13% at `-O2` and 19% at `-Os`. G.722 is the ITU reference-style integer code with a function call per sample, which is where `-O2` gains.
- SRTP is a flat 1.4 points per call: libsrtp's AES and SHA-1 are table driven.
- Four concurrent calls on the sample, by the thread analyzer, came to 12% of the core for G.711 with SRTP and 57% for G.722 at `-O2`; 15% and 80% at `-Os` (section 6).
- The resampler is the component to keep out of the path: libresample's large filter alone takes a third of the core, where linear interpolation costs under 1%. The preset builds none, and the sample runs everything at the codec's rate.

The whole table, for reference. The MIPS column is the CPU column times the 1.5 DMIPS/MHz of a Cortex-M33, so the CPU column is the one to read. The "echo canceller" rows equal the "echo suppressor" rows because the preset builds no acoustic echo canceller, so both fall back to the suppressor, whose cost does not depend on the tail length.

<details>
<summary>mips_test on the NUCLEO-H563ZI at -O2</summary>

```
MIPS test, with CPU=240Mhz,  360.0 MIPS
Clock  Item                                                        Time       CPU    MIPS
 Rate                                                             (usec)      (%)
-----------------------------------------------------------------------------------------
 8KHz get from memplayer                                             160    0.016    0.06
 8KHz conference bridge with 1 port                                 2940    0.294    1.06
 8KHz conference bridge with 2 ports                                5040    0.504    1.81
 8KHz conference bridge with 4 ports                                8460    0.846    3.05
 8KHz conference bridge with 8 ports                               15280    1.528    5.50
 8KHz conference bridge with 16 ports                              28970    2.897   10.43
 8KHz upsample+downsample - linear                                  3120    0.312    1.12
 8KHz upsample+downsample - small filter                           49180    4.918   17.70
 8KHz upsample+downsample - large filter                          291410   29.141  104.91
 8KHz WSOLA PLC - 0% loss                                            580    0.058    0.21
 8KHz WSOLA PLC - 2% loss                                            770    0.077    0.28
 8KHz WSOLA PLC - 5% loss                                           1270    0.127    0.46
 8KHz WSOLA PLC - 10% loss                                          1590    0.159    0.57
 8KHz WSOLA PLC - 20% loss                                          2610    0.261    0.94
 8KHz WSOLA PLC - 50% loss                                          5010    0.501    1.80
 8KHz WSOLA discard 2% excess                                        460    0.046    0.17
 8KHz WSOLA discard 5% excess                                        640    0.064    0.23
 8KHz WSOLA discard 10% excess                                      1100    0.110    0.40
 8KHz WSOLA discard 20% excess                                      1650    0.165    0.59
 8KHz WSOLA discard 50% excess                                      3460    0.346    1.25
 8KHz Delay buffer                                                   500    0.050    0.18
 8KHz Delay buffer - drift -2%                                       950    0.095    0.34
 8KHz Delay buffer - drift -5%                                      1040    0.104    0.37
 8KHz Delay buffer - drift -10%                                     1380    0.138    0.50
 8KHz Delay buffer - drift -20%                                     2340    0.234    0.84
 8KHz Delay buffer - drift +2%                                       810    0.081    0.29
 8KHz Delay buffer - drift +5%                                       980    0.098    0.35
 8KHz Delay buffer - drift +10%                                     1300    0.130    0.47
 8KHz Delay buffer - drift +20%                                     2000    0.200    0.72
 8KHz echo canceller 100ms tail len                                 4280    0.428    1.54
 8KHz echo canceller 128ms tail len                                 4560    0.456    1.64
 8KHz echo canceller 200ms tail len                                 6240    0.624    2.25
 8KHz echo canceller 256ms tail len                                 6940    0.694    2.50
 8KHz echo canceller 400ms tail len                                 2720    0.272    0.98
 8KHz echo canceller 500ms tail len                                 4040    0.404    1.45
 8KHz echo canceller 512ms tail len                                 4220    0.422    1.52
 8KHz echo canceller 600ms tail len                                 5720    0.572    2.06
 8KHz echo canceller 800ms tail len                                 3060    0.306    1.10
 8KHz echo suppressor 100ms tail len                                4280    0.428    1.54
 8KHz echo suppressor 128ms tail len                                4560    0.456    1.64
 8KHz echo suppressor 200ms tail len                                6240    0.624    2.25
 8KHz echo suppressor 256ms tail len                                6940    0.694    2.50
 8KHz echo suppressor 400ms tail len                                2720    0.272    0.98
 8KHz echo suppressor 500ms tail len                                4040    0.404    1.45
 8KHz echo suppressor 512ms tail len                                4220    0.422    1.52
 8KHz echo suppressor 600ms tail len                                5720    0.572    2.06
 8KHz echo suppressor 800ms tail len                                3060    0.306    1.10
 8KHz tone generator with single freq                               6380    0.638    2.30
 8KHz tone generator with dual freq                                12980    1.298    4.67
 8KHz codec encode/decode - G.711                                   3660    0.366    1.32
 8KHz stream TX/RX - G.711                                          4800    0.480    1.73
 8KHz stream TX/RX - G.711 SRTP 32bit                              13170    1.317    4.74
 8KHz stream TX/RX - G.711 SRTP 32bit +auth                        19370    1.937    6.97
 8KHz stream TX/RX - G.711 SRTP 80bit                              13150    1.315    4.73
 8KHz stream TX/RX - G.711 SRTP 80bit +auth                        19360    1.936    6.97
16KHz get from memplayer                                             240    0.024    0.09
16KHz conference bridge with 1 port                                 5360    0.536    1.93
16KHz conference bridge with 2 ports                                9330    0.933    3.36
16KHz conference bridge with 4 ports                               15680    1.568    5.64
16KHz conference bridge with 8 ports                               28380    2.838   10.22
16KHz conference bridge with 16 ports                              53780    5.378   19.36
16KHz upsample+downsample - linear                                  5820    0.582    2.10
16KHz upsample+downsample - small filter                           97680    9.768   35.16
16KHz upsample+downsample - large filter                          582070   58.207  209.55
16KHz WSOLA PLC - 0% loss                                            740    0.074    0.27
16KHz WSOLA PLC - 2% loss                                           1220    0.122    0.44
16KHz WSOLA PLC - 5% loss                                           2170    0.217    0.78
16KHz WSOLA PLC - 10% loss                                          3400    0.340    1.22
16KHz WSOLA PLC - 20% loss                                          6720    0.672    2.42
16KHz WSOLA PLC - 50% loss                                         13920    1.392    5.01
16KHz WSOLA discard 2% excess                                        450    0.045    0.16
16KHz WSOLA discard 5% excess                                       1480    0.148    0.53
16KHz WSOLA discard 10% excess                                      2020    0.202    0.73
16KHz WSOLA discard 20% excess                                      3820    0.382    1.38
16KHz WSOLA discard 50% excess                                     10310    1.031    3.71
16KHz Delay buffer                                                   800    0.080    0.29
16KHz Delay buffer - drift -2%                                      1440    0.144    0.52
16KHz Delay buffer - drift -5%                                      1690    0.169    0.61
16KHz Delay buffer - drift -10%                                     2600    0.260    0.94
16KHz Delay buffer - drift -20%                                     4490    0.449    1.62
16KHz Delay buffer - drift +2%                                      1240    0.124    0.45
16KHz Delay buffer - drift +5%                                      1850    0.185    0.67
16KHz Delay buffer - drift +10%                                     2660    0.266    0.96
16KHz Delay buffer - drift +20%                                     3870    0.387    1.39
16KHz echo canceller 100ms tail len                                 4700    0.470    1.69
16KHz echo canceller 128ms tail len                                 5100    0.510    1.84
16KHz echo canceller 200ms tail len                                 3520    0.352    1.27
16KHz echo canceller 256ms tail len                                 3640    0.364    1.31
16KHz echo canceller 400ms tail len                                 3840    0.384    1.38
16KHz echo canceller 500ms tail len                                 4240    0.424    1.53
16KHz echo canceller 512ms tail len                                 3880    0.388    1.40
16KHz echo canceller 600ms tail len                                 3680    0.368    1.32
16KHz echo canceller 800ms tail len                                 3460    0.346    1.25
16KHz echo suppressor 100ms tail len                                4700    0.470    1.69
16KHz echo suppressor 128ms tail len                                5100    0.510    1.84
16KHz echo suppressor 200ms tail len                                3520    0.352    1.27
16KHz echo suppressor 256ms tail len                                3640    0.364    1.31
16KHz echo suppressor 400ms tail len                                3840    0.384    1.38
16KHz echo suppressor 500ms tail len                                4240    0.424    1.53
16KHz echo suppressor 512ms tail len                                3880    0.388    1.40
16KHz echo suppressor 600ms tail len                                3680    0.368    1.32
16KHz echo suppressor 800ms tail len                                3460    0.346    1.25
16KHz tone generator with single freq                              12680    1.268    4.56
16KHz tone generator with dual freq                                25860    2.586    9.31
16KHz codec encode/decode - G.722                                 118820   11.882   42.78
16KHz stream TX/RX - G.722                                        119880   11.988   43.16
```

</details>

### SIP

What the SIP stack costs on the same board at `-O2`, measured with pjsip-test's URI, message and transmit-buffer benchmarks (section 5) over the suite's 41 URIs, 35 bytes long on average, and its 8 messages, 560 bytes on average. The suite logs rates normalised to 80-byte URIs and 800-byte messages; the table divides the elapsed times by the number of operations instead.

| Operation | Time | Rate |
|---|---|---|
| Parse a URI (`pjsip_parse_uri`) | 11 µs | 91,000 URIs/s |
| Print a URI (`pjsip_uri_print`) | 4.3 µs | 230,000 URIs/s |
| Compare two URIs (`pjsip_uri_cmp`) | 4.7 µs | 211,000 pairs/s |
| Find a message's end in a buffer (`pjsip_find_msg`) | 31 µs | 32,000 msg/s |
| Parse a message (`pjsip_parse_msg`) | 115 µs | 8,700 msg/s |
| Print a message (`pjsip_msg_print`) | 41 µs | 25,000 msg/s |
| Create a request (`pjsip_endpt_create_request`) | 183 µs | 5,500 msg/s |
| Create a response (`pjsip_endpt_create_response`) | 43 µs | 23,000 msg/s |

So a message arriving from the network costs the parser about 150 µs before the transaction layer sees it, and building a request about 180 µs, with the URI parser accounting for most of it: a message holds a dozen URIs. For scale, the desktop build of the same suite on an x86-64 host parses a URI in 0.2 µs and a message in 2.6 µs.


## 1. Install Zephyr and get pjproject

If you already have a working Zephyr workspace and SDK, skip to "Get pjproject" at the end of this section. Otherwise follow the [Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html); the steps below are its Ubuntu version, condensed, with one addition: the workspace can be created from pjproject's own manifest instead of Zephyr's.

### Host tools

Install the host tools (Ubuntu 24.04 or later):

```sh
sudo apt install --no-install-recommends git cmake ninja-build gperf \
  ccache dfu-util device-tree-compiler wget python3-dev python3-venv python3-tk \
  xz-utils file make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1
```

Zephyr needs CMake 3.28 or later and Python 3.12. Check with `cmake --version` and `python3 --version`.

### Create the workspace

The workspace holds Zephyr, its modules and `west`, Zephyr's tool, which lives in a Python virtual environment. This guide uses `~/zephyrproject`; substitute your own path. Create the environment first:

```sh
python3 -m venv ~/zephyrproject/.venv
source ~/zephyrproject/.venv/bin/activate
pip install west
```

Then fill the workspace in one of two ways. Pick one:

- A standard Zephyr workspace, as in Zephyr's guide: Zephyr at the release you choose, with all its modules, and pjproject cloned separately under "Get pjproject" below. The choice when the workspace serves other applications too.
- A workspace from pjproject's manifest: Zephyr pinned to 4.5.0 with only the five modules the port's targets need, and pjproject inside the workspace as its manifest repository. Smaller, and what CI builds from.

#### A standard Zephyr workspace

```sh
west init -m https://github.com/zephyrproject-rtos/zephyr ~/zephyrproject
cd ~/zephyrproject
west update
west packages pip --install
west zephyr-export
```

`west update` downloads Zephyr's modules and takes a while. Which Zephyr release:

- Zephyr 4.5 is the release the port is tested on, in CI on every change (section 5). To check it out, add `--mr v4.5.0` to the `west init` command.
- Zephyr 4.4.2 was checked by hand: everything builds and pjlib-test passes, except the TLS overlay, which sizes Mbed TLS's record buffers with `CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN` and `_OUT_CONTENT_LEN`, names 4.4 does not have. There set `CONFIG_MBEDTLS_SSL_MAX_CONTENT_LEN=16384` instead, at 12 KB more RAM.
- Zephyr 4.3 and older ship Mbed TLS 3.6 rather than 4.x. pjproject's TLS backend works with both, so they may well work, but they are not tested.

#### A workspace from pjproject's manifest

The manifest is [`west.yml`](west.yml) at the root of pjproject. `west init` clones pjproject to `~/zephyrproject/pjproject` and `west update` fetches Zephyr 4.5.0 and the five modules next to it:

```sh
west init -m https://github.com/pjsip/pjproject ~/zephyrproject
cd ~/zephyrproject
west update
west packages pip --install
west zephyr-export
```

With this workspace there is nothing to clone under "Get pjproject"; only set `PJ_DIR` there to `~/zephyrproject/pjproject`.

### Install the Zephyr SDK

The SDK has the cross compilers and a QEMU that knows the MPS2 boards:

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

With a standard Zephyr workspace, clone pjproject anywhere; it does not need to be inside the workspace:

```sh
git clone https://github.com/pjsip/pjproject.git ~/pjproject
export PJ_DIR=~/pjproject
```

With a workspace from pjproject's manifest, pjproject is already there: `export PJ_DIR=~/zephyrproject/pjproject`.

The rest of this guide refers to that location as `$PJ_DIR`; set the variable in every shell you run the commands from. No `./configure` or `make` is needed: the Zephyr build compiles pjproject from source as part of the application.

## 2. Configure

Two kinds of settings shape a build. pjproject's own compile-time settings, which codecs, how much logging, pool and queue sizes, are in `config_site.h` as on every other platform. Zephyr, the sample and the few things the build has to know about pjproject (threads, TLS, floating point) are [Kconfig](https://docs.zephyrproject.org/latest/build/kconfig/index.html) options. Section 8 is the reference for both; this section is what you need before the first build.

### config_site.h

pjproject's compile-time settings are in `pjlib/include/pj/config_site.h` in the pjproject tree, the same file every pjproject build reads. The file is not part of the repository: create it, and for Zephyr it must select the Zephyr preset:

```sh
cat > $PJ_DIR/pjlib/include/pj/config_site.h <<'EOF'
#ifdef __ZEPHYR__
#   define PJ_CONFIG_ZEPHYR 1
#   include <pj/config_site_sample.h>
#endif
EOF
```

This is enough for the sample. The `#ifdef` keeps the preset out of a host build of the same tree, which the next section needs for the pjsua that calls the sample: the Zephyr toolchain defines `__ZEPHYR__`, the host compiler does not. The preset in [`config_site_sample.h`](pjlib/include/pj/config_site_sample.h) sizes pjproject for a small endpoint and says why for each setting; section 8 says how to change any of it in the same file. The build stops with a message if `config_site.h` is missing or does not define `PJ_CONFIG_ZEPHYR`.

### Kconfig

[Kconfig](https://docs.zephyrproject.org/latest/build/kconfig/index.html) configures Zephyr, the sample and the pjproject module, like any Zephyr application. The sample's [`prj.conf`](zephyr/samples/embedded_ua/prj.conf) is a complete, commented configuration that works as is; the overlays [`overlay-threads.conf`](zephyr/samples/embedded_ua/overlay-threads.conf) and [`overlay-tls.conf`](zephyr/samples/embedded_ua/overlay-tls.conf) in the same directory add threads and TLS, applied with `-DEXTRA_CONF_FILE=`. Any option can also be given on the `west build` command line after `--`, as the examples in the next section do; everything after `--` goes to CMake.

The examples that register with a SIP server take its address from a shell variable. Set it once, to the address of your server:

```sh
export SIP_SERVER=192.168.1.19
```

The interactive menu needs a build directory. The first `west build` creates one from the sample's source directory and the board; the directory remembers both, and `west build` run inside it reuses them together with whatever the menu saved. Keep the build directories in a folder of their own inside the workspace, apart from Zephyr's; `west` works from any directory under the workspace:

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

The host is 192.0.2.2 and the Zephyr side is 192.0.2.1; both addresses are in the sample's [`boards/`](zephyr/samples/embedded_ua/boards/) files for the two emulated boards, since there is no DHCP server on this link. `prj.conf` itself asks for DHCP, as a real board would. The MTU of 1400 is a workaround for a QEMU driver bug described under Known limitations; it does no harm to native_sim. Zephyr's own `tools/net-tools/net-setup.sh` creates the same interface, if you prefer it.

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

You will see Zephyr boot, the network interface come up, and pjsip start its UDP transport on 192.0.2.1:55060. Leave it running; Ctrl-C stops it. The "not safe entropy source" warning at the top is expected: native_sim's entropy source is a test generator, which the `pjproject-target` snippet (section 9) seeds from the host at every start so that no two runs repeat a Call-ID.

### Make a call to it

Any SIP client on the host can call the sample. The easiest is pjsua from a normal Linux build of pjproject (`./configure && make` in the same checkout; the `#ifdef __ZEPHYR__` in `config_site.h` keeps the Zephyr preset out of it). The sample insists on SRTP, so SRTP has to be allowed over plain UDP:

```sh
cd $PJ_DIR/pjsip-apps/bin
./pjsua-x86_64-pc-linux-gnu --no-tcp --local-port 5062 --use-srtp=2 --srtp-secure=0 --play-file ../../tests/pjsua/wavs/input.16.wav --auto-play --add-buddy sip:bob@192.0.2.1:55060
```

At the pjsua prompt:

- Enter `m 1` to initiate call to the sample: the sample answers after one ring. pjsua streams the WAV into the call as soon as it connects, and the sample loops whatever it receives straight back.
- `cl` lists the conference ports
- `cc` with the call's port as source and `0`, the sound device, as destination (e.g. `cc 4 0`), to play the loopback on your speaker, after a round trip through the Zephyr guest.
- `dq` prints the call quality
- `h` hangs up.

With `CONFIG_EMBUA_LOG_LEVEL=4` (section 8; the default 3 keeps the console quiet enough not to disturb the media) the Zephyr side logs every SIP message, so you can watch the INVITE, the SDP negotiation and the SRTP setup go by.

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

embedded_ua has no console input. Send it a SIP MESSAGE (in pjsua: use `i`) with one of these bodies:

- **`call <uri>`** makes it place a call,
- **`mem`** logs its pool usage (and the C heap, when built with `CONFIG_SYS_HEAP_RUNTIME_STATS=y`),
- **`quit`** ends every call, unregisters and exits.

### pjsua-lib: simple_pjsua

[`simple_pjsua.c`](pjsip-apps/src/samples/simple_pjsua.c), pjsua-lib's minimal user agent, builds as the Zephyr sample in [`zephyr/samples/simple_pjsua`](zephyr/samples/simple_pjsua/):

- Its settings follow embedded_ua's pattern: `CONFIG_SIMPLE_PJSUA_*` options in the sample's "simple_pjsua" menu (domain, user name, password, port, calls at once, log level, the null sound device, and a URL to call at start since there is no command line), read by the sample on every platform.
- It builds with threads: pjsua-lib runs its SIP endpoint from a worker thread and its media from the sound device's clock.
- It runs on pjsua's null sound device, as the port has no sound device driver. The device clocks the conference bridge so that a call's audio flows: in, decoded into the bridge, and out as silence.
- RTP stays on pjsua's worker thread rather than a media thread of its own, so three threads come out of the kernel heap: the worker, pjmedia's event thread and, while a call is up, the clock; the main thread only sleeps and logs the heap, on its own static stack. The worker takes an INVITE from the socket to the answered call in one nest of calls and uses 17.4 KB of stack at its deepest, which sizes the stacks; the `boards/` files turn on `CONFIG_HW_STACK_PROTECTION`, so an overflow faults instead of corrupting memory.

```sh
cd ~/zephyrproject/mypjproject
west build -p always -b native_sim/native/64 -d build-pjsua $PJ_DIR/zephyr/samples/simple_pjsua -- -DCONFIG_SIMPLE_PJSUA_SIP_DOMAIN=\"$SIP_SERVER\"
build-pjsua/zephyr/zephyr.exe
```

It registers as `bob`/`secret` to the server, and pjsua on the host (above, without the SRTP options) can call `sip:bob@$SIP_SERVER` or the sample directly at `sip:bob@192.0.2.1:5060`. Verified on native_sim, on the AN521 under QEMU and on the NUCLEO-H563ZI against Kamailio: registration, a G.711 call from pjsua with no packet loss either way, and the hangup. For the board build as embedded_ua does (section 6), with `-b nucleo_h563zi/stm32h563xx` and the same `-DCONFIG_SIMPLE_PJSUA_SIP_DOMAIN`.

What pjsua-lib needed, beyond what an application on pjsip needs:

- `PJSIP_MAX_MODULE` 32, set by the sample's CMakeLists for its build: pjsua-lib registers 20 modules where the preset allows 16, enough for an application on pjsip, the recommended way on a small target.
- In the preset, `PJMEDIA_CONF_THREADS` 1, as the parallel conference bridge would start three more threads on a single core, and `PJSUA_DEFAULT_CLOCK_RATE` 8000, as the bridge runs at G.711's rate since the preset builds no resampler (16000 for G.722).
- The preset's limits on pjsua-lib's static state, one object with an array per limit: at the desktop defaults (8 accounts, 256 buddies, 32 players and recorders) it is 195 KB on a Cortex-M; with the preset's 1 account, 2 buddies, 2 players and 2 recorders it is 9 KB. The echo canceller is off, as the preset builds none and the null sound device has nothing to cancel.
- Heaps sized from measurements with one G.711 call, as embedded_ua's: a 144 KB arena and a 64 KB kernel heap for the three stacks. What they hold at each stage of a call, and the image sizes, are in "Footprint". Built with `CONFIG_SYS_HEAP_RUNTIME_STATS=y`, the sample logs both heaps' use and peak every ten seconds.

## 4. Run on a Cortex-M33 under QEMU

The MPS2 AN521 is an Arm development board that QEMU emulates, with a Cortex-M33 and a LAN9118 Ethernet controller. The `pjproject-target` snippet (section 9) limits it to 640 KB of RAM, the size of the STM32H563, so that an image that would not fit the real part fails to link here, and sets the clock to what QEMU's model runs.

Build, giving the address of your SIP server as in section 3; without it the sample registers to 192.0.2.2, the host side of the TAP, where nothing answers:

```sh
cd ~/zephyrproject/mypjproject
west build -p always -b mps2/an521/cpu0 -d build-an521 $PJ_DIR/zephyr/samples/embedded_ua -- -DCONFIG_EMBUA_SIP_SERVER_IP=\"$SIP_SERVER\"
```

For a run without any server give `-DCONFIG_EMBUA_USE_REGISTRATION=0` instead. The end of the build prints the memory use:

```
Memory region         Used Size  Region Size  %age Used
           FLASH:      350260 B         4 MB      8.35%
             RAM:      379600 B       640 KB     57.92%
```

That is the whole image, Zephyr and its network stack included; 236 KB of the RAM is the heap arena pjlib's pools are carved from. "Footprint" has the four builds side by side.

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


## 5. Run the test suites

Each library's own test suite builds as a Zephyr test application under `zephyr/tests/`: [`pjlib`](zephyr/tests/pjlib/), [`pjlib-util`](zephyr/tests/pjlib-util/), [`pjnath`](zephyr/tests/pjnath/), [`pjmedia`](zephyr/tests/pjmedia/) and [`pjsip`](zephyr/tests/pjsip/). The applications share one layout:

- a `CMakeLists.txt` that links the suite's sources through an INTERFACE target in the library's own `CMakeLists.txt`, so the desktop and Zephyr builds compile the same files
- a `Kconfig` with `CONFIG_<SUITE>_TEST_ARGS`, the test names and options to pass to the suite (empty runs every test), and the memory sizes that differ between the thread modes
- a `prj.conf` sizing the network stack, heap and stacks for the tests, and `boards/` files for the three targets; the board's leaves the Ethernet driver out, as the tests only use loopback
- a `testcase.yaml` for twister: the suite runs on native_sim, single threaded and with threads, watching the console for its closing line, and builds for the AN521

The tests run one at a time (`-w 0`) in `--ci-mode`. The threaded build of any suite adds `-DEXTRA_CONF_FILE=../overlay-threads.conf`, the overlay all five share. To run everything twister can:

```sh
cd ~/zephyrproject/mypjproject
west twister -T $PJ_DIR/zephyr/tests -p native_sim/native/64 -p mps2/an521/cpu0 -v
```

### Continuous integration

The "CI Zephyr" workflow ([`.github/workflows/ci-zephyr.yml`](.github/workflows/ci-zephyr.yml)) runs on every pull request that touches the libraries, the samples or the Zephyr files, in the workspace that `west.yml` describes (section 1), with the SDK's Arm toolchain cached between runs. Four jobs in parallel:

- every test suite and sample built for native_sim
- the same built for the AN521, where the link fails when an image outgrows 640 KB of RAM
- pjlib-test run on native_sim, single threaded
- pjlib-test run on native_sim, with threads

The other suites are built but not run: what Zephyr does differently they found once, and those things are fixed in the tests. What a library change breaks on Zephyr afterwards is the build, a call into something its libc or POSIX layer lacks, or the footprint, and the builds catch both in a fraction of the time the suites take.

### pjlib-test

pjlib-test, in `zephyr/tests/pjlib`, exercises the OS abstraction that a port touches most: pools, threads and thread-local storage, mutexes, timers, sockets over loopback, select and the ioqueue. A run takes a few minutes per configuration. To build and run one configuration by hand:

```sh
west build -p always -b native_sim/native/64 -d build-test $PJ_DIR/zephyr/tests/pjlib -- -DEXTRA_CONF_FILE=../overlay-threads.conf
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
  - [`test_profile.h`](pjlib/src/pjlib-test/test_profile.h) scales the test sizes to the target's RAM, `PJTEST_TARGET_RAM_KB` (Zephyr's `CONFIG_SRAM_SIZE`, 1 GB elsewhere): a thread count is one thread per 160 KB, so the ioqueue stress test, the parallel receive test and the runner's own scheduling test run 4 each on a 640 KB board and 16 on a desktop, and the timer test's size is one unit per 256 KB. It also records that native_sim's clock stands still while the CPU is busy, and that a target with less than 2 MB has a slow CPU, which sets the benchmarks' loop counts.
  - [`test.h`](pjlib/src/pjlib-test/test.h) derives the test sizes and which tests are built from that profile, and says why each one is left out.

- In [`zephyr/tests/pjlib/`](zephyr/tests/pjlib/) directory:

  - [`prj.conf`](zephyr/tests/pjlib/prj.conf), [`Kconfig`](zephyr/tests/pjlib/Kconfig) and the [`boards/`](zephyr/tests/pjlib/boards/) files size the network stack, heap and stacks for the tests, with the reason for each setting; the kernel heap the threaded build takes its stacks from follows the thread counts above.

The AN521 and NUCLEO-H563ZI configurations enable `CONFIG_HW_STACK_PROTECTION`, so a stack overflow faults instead of corrupting memory silently. It caught two, worth knowing when sizing an application, though pjlib itself needs neither:

- the activesock test keeps 100 operation keys on the main thread's stack, so the suite needs a 32 KB main stack
- Zephyr's `poll()` keeps one event per possible descriptor on the stack, which with `CONFIG_ZVFS_POLL_MAX` at 88 outgrows the 1200-byte default stack of the socket service thread the DNS resolver uses

On the NUCLEO-H563ZI the suite builds as above with `-b nucleo_h563zi/stm32h563xx` and `west flash`. Three things the board's 640 KB turned up are fixed on every platform: the runner's error path, which destroyed worker threads that were still waiting; the parallel receive test, which left its threads polling a destroyed ioqueue when one could not be created; and the thread counts, which used to assume a desktop.

### pjmedia-test and the MIPS benchmark

pjmedia-test builds the same way in [`zephyr/tests/pjmedia`](zephyr/tests/pjmedia/), with G.722 and libresample enabled on top of the preset. Its Kconfig adds `CONFIG_PJMEDIA_TEST_BENCHMARK`, which brings in `mips_test`, the benchmark `test_profile.h` otherwise leaves out of a slow CPU and of native_sim, where the simulated clock stands still while the CPU is busy. The benchmark pushes five seconds of audio through each media component as fast as it can and reports the share of the CPU that component takes per second of audio. Build it for the board with the options that print the table as it goes, since the test runner shows a passing test's log only when asked:

```sh
west build -p always -b nucleo_h563zi/stm32h563xx -d build-mips $PJ_DIR/zephyr/tests/pjmedia -- -DCONFIG_PJMEDIA_TEST_BENCHMARK=y -DCONFIG_PJMEDIA_TEST_ARGS=\"--log-no-cache -l 3 mips_test\"
west flash
```

The application's `prj.conf` builds at `-O2` and sizes the heap for the 16-port conference bridge at 16 kHz, the benchmark's largest user; the entries that need a hundred streams are left out below 2 MB of RAM. The figures measured on the NUCLEO-H563ZI are in "Processing cost" at the top. Two tests stay out on Zephyr, in the suite's `test.h`: the jitter buffer test and the codec test vectors read their data from files.

### pjlib-util-test, pjnath-test and pjsip-test

The three remaining suites build the same way, in [`zephyr/tests/pjlib-util`](zephyr/tests/pjlib-util/), [`zephyr/tests/pjnath`](zephyr/tests/pjnath/) and [`zephyr/tests/pjsip`](zephyr/tests/pjsip/):

```sh
west build -p always -b native_sim/native/64 -d build-pjnath $PJ_DIR/zephyr/tests/pjnath -- -DCONFIG_PJNATH_TEST_ARGS=\"ice_test turn_sock_test\"
build-pjnath/zephyr/zephyr.exe
```

Each talks to itself over loopback: pjlib-util-test runs its DNS resolver, HTTP and WebSocket clients against servers in its sources, pjnath-test runs STUN, TURN and ICE against its test server, and pjsip-test runs its transports and its own DNS server. What the applications set beyond the preset, each with the reason in its `CMakeLists.txt`:

- 64 ioqueue handles (`PJ_IOQUEUE_MAX_HANDLES`), and the sockets and buffers the busiest tests open
- for pjsip-test, 32 modules (`PJSIP_MAX_MODULE`; the preset's 16 is for an application) and 6 resolved addresses per destination (`PJSIP_MAX_RESOLVED_ADDRESSES`; the SRV failover test resolves to three servers, with a TCP copy of each)

pjsip-test's Kconfig also has `CONFIG_PJSIP_TEST_BENCHMARK`, the counterpart of pjmedia-test's, which brings in the URI and message parsing benchmarks that `test_profile.h` leaves out of a slow CPU and of native_sim. On a board they run 2,000 rounds over the suite's URIs and messages, ten seconds in all. The figures measured on the NUCLEO-H563ZI are in "Processing cost" at the top; to reproduce them, with the options that print the figures as they come:

```sh
west build -p always -b nucleo_h563zi/stm32h563xx -d build-bench $PJ_DIR/zephyr/tests/pjsip -- -DCONFIG_PJSIP_TEST_BENCHMARK=y -DCONFIG_PJSIP_TEST_ARGS=\"--log-no-cache -l 3 uri_test msg_test txdata_test\" -DCONFIG_SPEED_OPTIMIZATIONS=y
west flash
```

What passes where, with the time a run takes:

| Suite | native_sim | AN521 under QEMU | NUCLEO-H563ZI |
|---|---|---|---|
| pjlib-test | 15 tests, 8 min; threads 20 + self-test | 15; threads 20 | 20 in 3 min; threads 25 in 6 min |
| pjlib-util-test | 5 in 1 s; threads 7 in 5 min | 5; threads 7 | 5 in 2 s; threads 7 in 4 min |
| pjmedia-test | 5 in 1 s; threads 5 | 5; threads 5 | 5 in 1 s; threads 5 |
| pjnath-test | 20 in 9 min; threads 22 in 10 min | 19 in 9 min; threads 21 in 10 min | 19 in 9 min; threads 21 in 10 min |
| pjsip-test | 12 in 1 min; threads 30 in 28 min | 12 in 1 min | 12 in 1 min |

The counts differ by design, and each suite's `test.h` says why a test is out:

- Without threads a suite leaves out what needs a worker thread: pjlib-util-test's HTTP client and WebSocket tests, whose servers are threads, and it runs the resolver test in its single-threaded form; pjnath-test's concurrency and session authentication tests; and pjsip-test's loop transport, which runs on a worker thread, and with it the transaction, invite and registration tests (`PJSIP_TEST_HAS_LOOP`).
- The pjsua tests stay out of the Zephyr build of pjsip-test, since pjsua is not part of the port; leaving their sources out of the test target also keeps pjsua's 195 KB of static state out of the image.
- Below 1 MB of RAM pjnath-test leaves out its trickle ICE test, which needs 448 KB of heap where the other ICE tests run in 320 (`PJTEST_TARGET_RAM_KB` in `test_profile.h`).
- On Zephyr pjnath-test leaves out the fifty rounds of its concurrency stress: eight threads polling one select ioqueue make Zephyr's `select()` return at once.
- The threaded pjsip-test does not fit a 640 KB board: it needs the 384 KB heap of the single-threaded suite plus some 90 KB for its threads and their locks.

The suites found a few things a loopback-only host does differently from a desktop, each fixed in the test rather than in the library:

- Zephyr drops a datagram whose source and destination endpoints are the same, so pjsip-test's UDP round trips, transaction tests and registration test send from one UDP transport to another.
- A host with only loopback refuses a connection to any outside address at once, so pjsip-test's TCP selector test connects to one of its own listeners, and the transaction-destroy cases that need a connection still in progress run only where `PJTEST_HAS_OUTSIDE_ROUTE` says there is one.
- The ICE test enabled loopback host candidates only on the legacy STUN setting, not on the per-server one that takes effect once a STUN server is configured, which left its server-reflexive candidates without a base.
- The TURN test server kept reading a connection the client had closed: on a real OS a busy wait that still ends, on native_sim, where the simulated clock stands still while the CPU is busy, one that never did. pjsip-test's multithreaded round-trip benchmark stops there for the same reason and runs only where `PJTEST_TIME_PASSES_WHEN_BUSY`.
- The rest is memory sized for a desktop: a million-byte SHA1 block, an 8 MB HTTP body, 64 KB of WebSocket receive buffers and 24 KB of echo-server buffers on an 8 KB thread stack, eight hundred DNS queries kept in one pool, a hundred messages alive at once in a benchmark, a 128 KB block meant only to defeat the pool cache, and a STUN authentication test that released neither its client pools nor the timers its sessions still held. Each now fits a few hundred KB.

## 6. Run on real hardware

The steps below are for the NUCLEO-H563ZI, the board the port was developed on; another board takes its own name in the build command and the adjustments listed after the steps.

1. Connect the board: Ethernet to a network with a DHCP server from which the SIP server is reachable, and the ST-LINK USB port (CN1) to the host. The ST-LINK is the programmer and the serial console in one.

2. Build as for QEMU in section 4, with the board's name and a build directory of its own. SIP over UDP, single threaded:

   ```sh
   cd ~/zephyrproject/mypjproject
   west build -p always -b nucleo_h563zi/stm32h563xx -d build-h563 $PJ_DIR/zephyr/samples/embedded_ua -- -DCONFIG_EMBUA_SIP_SERVER_IP=\"$SIP_SERVER\"
   ```

   The overlays of section 3 apply the same way. SIP over TLS with the media on its own thread:

   ```sh
   west build -p always -b nucleo_h563zi/stm32h563xx -d build-h563 $PJ_DIR/zephyr/samples/embedded_ua -- -DCONFIG_EMBUA_SIP_SERVER_IP=\"$SIP_SERVER\" -DEXTRA_CONF_FILE="overlay-threads.conf;overlay-tls.conf"
   ```

3. Flash, giving the same build directory. The board's default runner is STM32CubeProgrammer; without it, use pyocd, which Zephyr's Python requirements install, after `pyocd pack install stm32h5` once for the STM32H5 family:

   ```sh
   west flash -d build-h563 -r pyocd
   ```

   The board resets and runs the image when flashing ends.

4. Watch the console on the ST-LINK's serial port, `/dev/ttyACM0` on Linux, at 115200 baud, with any terminal program (`screen /dev/ttyACM0 115200`, `tio /dev/ttyACM0`). Open it before flashing to see the boot. The log shows the DHCP lease with the board's address, the REGISTER, the 401 challenge and the 200 OK.

5. Call it as in section 3, through the server, since the board's address comes from DHCP: in pjsua, `m sip:bob@$SIP_SERVER`. The sample answers after one ring and loops the audio back.

The sample is written for real hardware and the emulated boards are the exception:

- `prj.conf` asks for an address from DHCP and waits for the lease; the emulated boards' static TAP address is in the sample's `boards/` files, which Zephyr merges after `prj.conf`.
- Random numbers, which SIP Call-IDs, tags and SRTP keys all come from, come from the board's hardware generator through `CONFIG_ENTROPY_GENERATOR`; the emulated boards get a software one from the `pjproject-target` snippet (section 9).
- Zephyr's board files give the NUCLEO-H563ZI only 256 KB of its 640 KB; the snippet carries the devicetree overlay that gives it the rest, so nothing has to be added for it.

Things to adjust for another board, in a `boards/<your board>.conf` file in the sample directory or in your own copy of `prj.conf`:

- Addressing. For a fixed address instead of DHCP, set `CONFIG_NET_DHCPV4=n` and the `CONFIG_NET_CONFIG_MY_IPV4_*` lines, as the sample's `boards/` files do for the emulated boards.
- Memory. `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE` is the heap pjlib allocates its pools from. The sample's Kconfig sets it per feature set to the measured peak of four concurrent calls plus 5% ("Footprint"); an application that needs fewer calls, or wants more headroom, sets its own, at about 46 KB a call. `CONFIG_MAIN_STACK_SIZE` of 16 KB covers pjsip's message parsing on the main thread; the deepest use seen was 12.4 KB.
- The network interface. The board needs an Ethernet or other IP-capable interface with a Zephyr driver, and `CONFIG_NET_L2_ETHERNET=y` (or the matching L2) for it.
- The receive pool. `prj.conf` sizes it for QEMU, whose Ethernet model stops delivering once a buffer runs out (section 10); a real controller drops the frame and carries on. On the NUCLEO-H563ZI the four-INVITE burst below passed with `CONFIG_NET_PKT_RX_COUNT=40` and `CONFIG_NET_BUF_RX_COUNT=80` too, 51 KB less RAM than the shipped pool.

A board with less than 640 KB of RAM needs a smaller arena and fewer network buffers; a 1 MB part such as the STM32H753 has room to spare.

### Results on the NUCLEO-H563ZI

Measured on a NUCLEO-H563ZI on a wired link to the host that runs Kamailio and pjsua, with each of the four builds of the sample. In every build the board took its address from DHCP, registered within a few seconds of the lease, and answered four INVITEs sent within 100 ms of each other at once: four G.711 calls with SDES-SRTP both ways, zero packets lost or discarded in either direction, round trips of 0.5 to 2 ms, no jitter buffer messages, and clean hangups. The image sizes and the heap peaks are in "Footprint"; the other things worth knowing before trusting a board:

- Clock. The board runs tickless and its clock keeps time: over three two-minute calls the interval between call setup and hangup by the board's clock was within 1 to 6 ms of the host's, under 50 ppm, which is the crystals' tolerance, with jitter under 0.5 ms. None of the "Frame lost, recovered!" messages of the QEMU guest.
- CPU. Four G.711 calls with SRTP take about 12% of the core at `-O2` and 15% at `-Os`; four G.722 calls about 57% at `-O2` and 80% at `-Os`, by the thread analyzer's cycle counts per 10 s interval during the calls (`CONFIG_THREAD_ANALYZER=y` with `CONFIG_THREAD_ANALYZER_AUTO=y`; the percentages it prints itself are cumulative since boot). Where it goes, component by component, is in "Processing cost" at the top.
- Stacks, from the same analyzer in the threaded builds: the media thread peaked at 7.6 KB of the 16 KB `CONFIG_DYNAMIC_THREAD_STACK_SIZE` in the threads overlay and the main thread at 4.7 KB of its 16 KB; Zephyr's network RX and TX threads and the system work queue, which the sample leaves at Zephyr's defaults, stayed under 80% of theirs. In the single-threaded builds the main thread parses SIP and runs the media too; the 12.4 KB above is its deepest use seen.
- Memory. The heap peaks of the four-call bursts are those of the emulator to within a few hundred bytes ("Footprint"); nothing on the board allocates differently.
- pjsua-lib. simple_pjsua on the board (section 3) took its address from DHCP, registered 12 s after reset and answered a G.711 call from pjsua: no packet lost or discarded either way, round trips under 1 ms, a clean hangup, and heap readings identical to the AN521's to the byte at every stage ("Footprint").
- Reset and re-registration. A board reset while registered leaves the old binding in the registrar until it expires, which with Kamailio means an INVITE forks to the old Contact too; it does no harm over UDP, but see section 11 for what it does when the old and the new image use different transports. Send `quit` before reflashing when the registrar matters.

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

The `pjproject` target carries every library, pjsua-lib included, and the include paths; the libraries are static archives, so an application links only what it calls, and includes `<pjsip.h>`, `<pjmedia.h>`, `<pjlib.h>` and so on as usual. `ZEPHYR_EXTRA_MODULES` tells Zephyr to treat the pjproject tree as a module and must be set before `find_package(Zephyr)`. It can equally be given on the command line (`-DZEPHYR_EXTRA_MODULES=/path/to/pjproject`) or, to make pjproject a permanent part of a west workspace, listed as a project in the west manifest, after which the line is not needed at all; a workspace created from pjproject's own `west.yml` (section 1) has it already.

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
CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=241664
CONFIG_ENTROPY_GENERATOR=y
```

The sample's `zephyr/samples/embedded_ua/prj.conf` is a complete, commented example and the best starting point; copy it and remove what you do not need. Three of its settings deserve a word:

- `CONFIG_NET_IPV4_FRAGMENT=y`: a SIP INVITE with SDP is usually larger than the Ethernet MTU and arrives as IP fragments.
- The receive pool, `CONFIG_NET_PKT_RX_COUNT` and `CONFIG_NET_BUF_RX_COUNT`, is sized for four such INVITEs arriving at once, more than a UA that takes calls one at a time needs (section 6 has the smaller figure).
- `CONFIG_ZVFS_OPEN_MAX` bounds the file descriptors `select()` can watch and must be at least `PJ_IOQUEUE_MAX_HANDLES`; the build checks this with a static assertion.

To build your application for the NUCLEO-H563ZI, or for QEMU's AN521 or native_sim as its stand-ins, add the `pjproject-target` snippet of section 9 with `set(SNIPPET pjproject-target)` before `find_package(Zephyr)`, as the sample does, or with `-S pjproject-target` on the command line. It holds what each of those targets needs beyond Zephyr's board files, with the reason for each setting in its files, and does nothing on other boards.

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

What the module builds:

- The bundled libsrtp and libresample always, and G.711, G.722 and L16 need no library. They are static archives, so an application that does not use one does not link it.
- Not the other codecs (Opus, Speex, iLBC, GSM, G.722.1, AMR), video, the echo cancellers or the sound device backends. Media comes from the network and goes back to it.
- Video has not been tried at all: pjmedia-videodev is declared and gets Zephyr's flags like every other library, so that an application linking pjsua-lib, which depends on it, compiles; with `PJMEDIA_HAS_VIDEO` off in the preset every one of its files is empty.

A note on threads. The sample runs single threaded by default, which needs nothing special and is the smallest build. Built with `-DEXTRA_CONF_FILE=overlay-threads.conf`, pjsip runs in one pthread and the media clock and RTP in another. The overlay shows what a threaded build needs besides `CONFIG_PJPROJECT_THREADS`, with the reason for each setting; what to know when writing one:

- pjlib creates its threads at run time, from heap-allocated stacks. A thread created with an explicit stack size gets one of that size from the same heap; Zephyr frees it only when the thread's attributes are destroyed after it has ended, which pjlib does in `pj_thread_join()` or `pj_thread_destroy()`. Join or destroy the threads you create to get their stacks back.
- Zephyr keeps pthread key values only for threads created with `pthread_create()`, not for its main thread, so on Zephyr pjlib keeps its per-thread data in compiler thread-local storage instead (`PJ_THREAD_LOCAL_USE_COMPILER_TLS` in `os_zephyr.h`). The application can use pjlib from `main()` as on any other platform.
- Zephyr takes that storage off the top of every thread's stack, kernel threads included, so each stack loses pjlib's share of about 270 bytes. The overlay raises the idle thread's stack, whose default is smaller than that; the net stack's small kernel stacks were checked to still have room.

### config_site.h

pjproject's compile-time settings beyond Kconfig are in `pjlib/include/pj/config_site.h` in the pjproject tree, the file every pjproject build uses and section 2 had you create. Nothing is generated: the Zephyr build reads that file as the Linux, Windows and macOS builds do. On Zephyr it must select the Zephyr preset, at minimum:

```c
#define PJ_CONFIG_ZEPHYR 1
#include <pj/config_site_sample.h>
```

The `PJ_CONFIG_ZEPHYR` preset in `pjlib/include/pj/config_site_sample.h` holds the Zephyr settings, each with the reason next to it: which codecs and media features are built, and the sizes of pjsip's and pjmedia's tables, pools, messages and packets, chosen for a few calls on a 640 KB board. Read it to see exactly what is set. The sizes an application may need larger, such as `PJSIP_MAX_PKT_LEN`, `PJMEDIA_MAX_MTU`, `PJMEDIA_MAX_MRU` and the SDP limits, are behind `#ifndef`, so a definition before the include overrides them. What is a fact of the platform rather than a choice, such as pjlib keeping its per-thread data in compiler thread-local storage, is in `os_zephyr.h` and applies whatever `config_site.h` says.

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

Keep the Zephyr lines behind `#ifdef __ZEPHYR__`, as section 2 did, when the tree also serves host builds; the Zephyr toolchain defines the macro, so the same `config_site.h` configures both.

The test applications in `zephyr/tests` need larger limits than the preset's, a 64-socket I/O queue among them, and get them from compile definitions in their `CMakeLists.txt`, so that they build with the plain preset above and leave the tree's `config_site.h` to the user.

### The sample's own options

The sample's build settings are `CONFIG_EMBUA_*` macros that `embedded_ua.c` reads on every platform; on Zephyr they are options in the sample's Kconfig, set in `prj.conf`, in menuconfig or with `-DCONFIG_...` on the west command line, each described there. Three things the help text cannot say:

- `CONFIG_EMBUA_AUDIO_CODEC` set to G.722 also needs `PJMEDIA_HAS_G722_CODEC` in `config_site.h`; the codec, SRTP, transport and media thread options follow the `CONFIG_PJPROJECT_*` options by default.
- `CONFIG_EMBUA_LOG_LEVEL` above 3 stalls the media while a call is set up on a slow UART: each line is written to the console by the thread that logs it.
- The on/off settings are ints rather than Kconfig bools, so that the macro is always defined. Without registration the sample is reached at `sip:<address>:55060`.

## 9. How the port is put together

The Zephyr-specific parts are few, and this is where to look when something needs changing:

- `zephyr/module.yml`: tells Zephyr this tree is a module and where its Kconfig and CMake are, and that it is configured after Zephyr's Mbed TLS module, whose targets the TLS backend links (a module listed before it, as the manifest repository of a workspace is, would otherwise be configured first).
- `zephyr/Kconfig`: the `CONFIG_PJPROJECT_*` options above.
- `zephyr/CMakeLists.txt`: when `CONFIG_PJPROJECT` is set, checks that `config_site.h` selects the Zephyr preset, drives pjproject's own per-library `CMakeLists.txt` files with options derived from Kconfig, gives every library Zephyr's include paths and flags, and defines the `pjproject` link target.
- `zephyr/cmake/MbedTLSConfig.cmake`: lets pjlib's Mbed TLS backend find Zephyr's Mbed TLS.
- `pjlib/include/pj/compat/os_zephyr.h`: the platform description (which headers and socket functions exist, threads, TCP and TLS from Kconfig, no IPv6). `pj/config.h` selects it when `__ZEPHYR__` is defined. One of its settings, `PJ_THREAD_LOCAL_USE_COMPILER_TLS`, makes pjlib's thread-local storage use the compiler's `__thread` rather than pthread keys, which Zephyr does not provide to its main thread.
- `pjlib/src/pj/ip_helper_zephyr.c`: network interface and route enumeration through Zephyr's `net_if` API, since Zephyr has no `getifaddrs()`. This is how pjsip learns the address to put in Via and Contact headers.
- The `PJ_CONFIG_ZEPHYR` preset in `pjlib/include/pj/config_site_sample.h`, which sizes pjsip and pjmedia for a small endpoint; `config_site.h` selects it.
- `zephyr/tests/<lib>` and `zephyr/tests/overlay-threads.conf`: the five test applications of section 5, each with the `testcase.yaml` that twister runs and, in `boards/`, the heap and stack sizes its tests need on each board. Their sources are the libraries' own, in `<lib>/src/<lib>-test` (`pjmedia/src/test`, `pjsip/src/test`).
- `zephyr/snippets/pjproject-target`: a [snippet](https://docs.zephyrproject.org/latest/build/snippets/index.html) with what pjproject's applications need on each target beyond Zephyr's board files: the NUCLEO-H563ZI, and QEMU's AN521 and native_sim as its stand-ins. Each file under its `boards/` says what it sets and why. The sample and the test select it in their `CMakeLists.txt`; a `-S` on the command line replaces that selection, so give `-S pjproject-target` too when adding another snippet. It is registered through `snippet_root` in `module.yml` and so available to any application that uses the module.
- `zephyr/samples/embedded_ua`: the sample's `CMakeLists.txt`, `Kconfig`, `prj.conf`, `run-qemu.sh` and, in `boards/`, the static TAP address of the two emulated boards. The source is `pjsip-apps/src/samples/embedded_ua.c`, the same file that builds on Linux, Windows and macOS; on Zephyr it reads its settings from the `CONFIG_EMBUA_*` options.

Random numbers come from Zephyr's `sys_rand32_get()` rather than the C library's `rand()`, which is unseeded on Zephyr. Zephyr's `sockaddr_in` is 8 bytes rather than 16, which pjlib accommodates. Zephyr's `sockaddr_in6` differs from the BSD layout in ways pjlib does not yet accommodate, which is why IPv6 is off.

## 10. Known limitations

- IPv6 is disabled on Zephyr.
- QEMU's MPS2 boards drop full-size Ethernet frames: Zephyr's `smsc911x` driver rounds the frame length up before subtracting the checksum, asks for a buffer two bytes larger than the network stack allows for a 1514-byte frame, and the frame is lost. The first fragment of a SIP INVITE is such a frame. Until the driver is fixed, keep the host side of the TAP at MTU 1400 so that every frame is smaller; the `pjproject-target` snippet caps the guest's own IPv4 MTU at 1400 as well, so that nothing it sends is larger either, while `prj.conf` keeps 1500 for real hardware. Real hardware and native_sim are not affected.
- QEMU's LAN9118 model stops delivering usable frames once the driver has skipped one: when Zephyr's `smsc911x` driver cannot get a receive buffer it tells the controller to skip the frame, and the model's FIFO accounting does not follow (the `RX_DP_CTRL` handler in QEMU's `hw/net/lan9118.c` leaves the packet size and the used count stale). On QEMU the receive pools must therefore never run dry, and several INVITEs arriving within a second can drain them, each waiting in the pool, whole, while the main thread sets up the call before it. The sample's pool takes four at once with the help of `CONFIG_NET_IPV4_FRAGMENT_MAX_COUNT=2`, which serialises the burst two calls at a time. Logging every SIP message makes it worse. Real hardware drops the frame and carries on, and a pool of 40 packets and 80 buffers took the same burst on the NUCLEO-H563ZI (section 6).
- The QEMU boards have no entropy device, so the sample uses Zephyr's test random generator, seeded from the timer so that each boot differs. It is not a secure source; a real board must use its hardware RNG.
- QEMU's AN521 model clocks SysTick at 20 MHz, while Zephyr's board files declare the real board's 25 MHz. Left alone, the guest's clock runs at 80% of real time: the sample sends 40 RTP packets a second instead of 50, the audio from it breaks up, and pjsua reports RTTs of half a second. The `pjproject-target` snippet therefore sets `CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC=20000000` for that board; an application of your own gets it from the snippet, and must not have it on the hardware. The AN385 model and native_sim are not affected.
- Even with the SysTick rate corrected, the AN521 guest's clock runs slightly slow under QEMU and the guest logs "Frame lost, recovered!" about once a second during a call, with an occasional "Internal RTCP NTP clock skew detected". Nothing is lost: pjsua reports zero loss and the jitter buffer's summary shows `discard=0`. The guest reads frames a little slower than they arrive, so the jitter buffer discards one now and then and conceals the gap, which is what the message reports. The cause is QEMU's timer model losing time when the host deschedules the emulator; it goes away with periodic kernel ticks (`CONFIG_TICKLESS_KERNEL=n` and `CONFIG_SYS_CLOCK_TICKS_PER_SEC=1000`), which the sample does not set because real hardware runs tickless.
- Logging at a `PJ_LOG_MAX_LEVEL` below 5 produces a few unused-variable warnings in pjproject's own code. They are harmless.
- The sample's TLS transport has no certificate, so it cannot take inbound TLS connections; requests to it must arrive on the connection it registered over (section 3). That is the sample's choice, not the port's: with a certificate and key in `pjsip_tls_setting` the listener works as on any platform. The listener is nevertheless created: built with `PJSIP_TLS_TRANSPORT_DONT_CREATE_LISTENER` the sample registers, but Kamailio then opens a new connection to the Contact for every request instead of reusing the registration's, and that connection fails; with the listener present the requests arrive over the registration's connection.
- `exit()` on Zephyr does not end the program; after `quit`, the sample's final log line is followed by an idle loop. Stop the emulator by hand.
- Zephyr drops a datagram whose source and destination addresses and ports are the same, so a socket cannot send to itself; the test suites send between two sockets instead (section 5). An application never needs to.
- The threaded pjsip-test needs about 690 KB of RAM and does not fit the 640 KB boards; pjnath-test's trickle ICE test is out below 1 MB and its concurrency stress on Zephyr (section 5).

## 11. Troubleshooting

- "no Zephyr build in ..." from `run-qemu.sh`: give it the build directory you passed to `west build -d`.
- The guest never gets a response: check `ip addr show zeth` shows 192.0.2.2 and the interface is UP, and that no firewall drops traffic on it. On QEMU, check the MTU is 1400.
- A registrar answers the second REGISTER with 400 or "invalid CSeq": the random numbers repeat from one run to the next. The `pjproject-target` snippet is not applied: it gives native_sim `CONFIG_NATIVE_EXTRA_CMDLINE_ARGS="-seed-random"` and both emulated boards `CONFIG_TEST_RANDOM_GENERATOR`.
- Audio from the QEMU guest breaks up, pjsua shows RTTs of hundreds of milliseconds and the guest logs "Frame lost, recovered!" throughout a call: the guest's clock is slow; see the SysTick entry in section 10. One "Frame lost, recovered!" a second with good audio and zero loss in pjsua is the harmless case described there.
- The build stops with "config_site.h is missing or does not select the Zephyr preset": create `pjlib/include/pj/config_site.h` in the pjproject tree as in section 2.
- The build stops with `PJ_IOQUEUE_MAX_HANDLES_must_not_exceed_FD_SETSIZE`: `FD_SETSIZE` on Zephyr is `CONFIG_ZVFS_OPEN_MAX`; raise it to at least `PJ_IOQUEUE_MAX_HANDLES` (16 in the preset).
- The image does not fit: in `config_site.h` lower `PJ_LOG_MAX_LEVEL`; in `prj.conf` shrink `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE`.
- A SIP INVITE arrives in the host log but never in the guest: `CONFIG_NET_IPV4_FRAGMENT` is off, or the fragment buffers (`CONFIG_NET_IPV4_FRAGMENT_MAX_PKT`, `CONFIG_NET_BUF_RX_COUNT`) are too few.
- Over TLS, the first REGISTER after boot gets a 503 with "Failed to mbedtls_ssl_handshake, ret -0x7780" and Kamailio logs "no shared cipher", then the retry ten seconds later registers: seen with Kamailio 5.7 on the first TLS connection a fresh worker process accepts, and not on later ones. A server-side matter; the sample's retry covers it.
- Over TLS, calls and MESSAGEs to the sample time out with 408 and it logs "Handshake failed on ...:55061 in accepting": the server is opening a new connection to the sample's listener, which has no certificate (section 10), instead of reusing the registration's. One cause is a binding left by a previous run with a different transport, for instance a board reflashed from the UDP build without `quit`: Kamailio forks the request to both Contacts, and `handle_ruri_alias()` rewrites only the Request-URI, so the TLS branch is sent to the Contact's listener port. Wait for the old binding to expire (five minutes with the sample's settings), or restart the registrar.
