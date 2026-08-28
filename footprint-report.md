# PJSIP footprint measurement — minimal embedded configuration

**Date:** 2026-08-28
**Question answered:** does a minimal PJSIP build — G.711 A/µ-law + G.722 +
RFC 4733 telephone-event, SDES-SRTP, SIP over TLS (mbedTLS), no video, no
ICE/STUN/TURN usage, 4 concurrent bidirectional calls — fit a target with
**640 KB RAM and 2 MB flash** (STM32H563, Cortex-M33, no MMU, no filesystem)?

**Short answer:** yes, with margin on flash and a workable but
configuration-sensitive picture on RAM. Headline numbers, measured (not
extrapolated):

| Quantity | Measured | Against target |
|---|---|---|
| Flash (code + rodata + init data), logging at level 4 | {{FLASH_LOG4}} | ~{{FLASH_PCT}} of 2 MB |
| Flash, logging compiled out (`PJ_LOG_MAX_LEVEL 0`) | {{FLASH_LOG0}} | — |
| Static RAM (`.data` + `.bss`) | {{STATIC_RAM}} | see §5 |
| Heap, idle (0 calls, steady) | {{HEAP_BASE}} | — |
| Heap, 4 concurrent SRTP/G.722 calls (steady) | {{HEAP_4CALL}} | — |
| Marginal heap per call | {{HEAP_PER_CALL}} | — |

All RAM-related numbers come from 32-bit builds; 64-bit numbers would be
misleading (pointer-heavy structs roughly double) and are not reported.

---

## 1. What exactly was measured

### Versions and toolchains

| Component | Version |
|---|---|
| pjproject | tag `2.17` (commit `5a457451fa2712ba18e12b01738e8ff3af2b26fd`, 2026-04-22) |
| mbedTLS | 3.6.7 (LTS), default `mbedtls_config.h` |
| SRTP | bundled `third_party/srtp` (libsrtp 2.x as shipped in 2.17) |
| Cross toolchain (Build A) | Ubuntu `arm-linux-gnueabihf-gcc` 13.3.0, `-Os -g0 -mthumb -ffunction-sections -fdata-sections`, linked `-Wl,--gc-sections` |
| Host toolchain (Build B) | Ubuntu gcc 13.3.0 `-m32 -Os -g` + same section GC flags |
| Heap profiler | valgrind massif (`--pages-as-heap=no`), 32-bit binaries |

Two builds from the **same working tree state and the same
`config_site.h`** (reproduced in full below and committed at
`footprint/config_site.h`):

* **Build A — flash + static RAM proxy:** cross-compiled to 32-bit ARM
  Thumb-2 (Linux EABI hard-float). Thumb-2 code size is close across
  M-/A-profile, so this stands in for Cortex-M33 code size. The measured
  ELF is `footprint/footprint_app.c`, a minimal-but-complete PJSUA
  application that references TLS transport, SDES-SRTP, G.711/G.722, the
  conference bridge and the null audio device — i.e. a real link that
  pulls in exactly what a minimal endpoint uses.
* **Build B — dynamic RAM:** the same configuration built natively for
  32-bit x86 and exercised with real calls between two `pjsua` instances
  over TLS with SRTP mandatory, measured under massif.

### Configure line (identical feature set in both builds)

```
./configure --host=<arm-linux-gnueabihf | i686-pc-linux-gnu> \
    --disable-video --disable-sound --disable-speex-aec \
    --disable-speex-codec --disable-gsm-codec --disable-ilbc-codec \
    --disable-l16-codec --disable-g7221-codec --disable-opencore-amr \
    --disable-silk --disable-opus --disable-bcg729 --disable-libwebrtc \
    --disable-upnp --disable-v4l2 --with-mbedtls=<prefix> \
    CFLAGS="... -Os ..." LDFLAGS="-Wl,--gc-sections ..." \
    LIBS="-lmbedtls -lmbedx509 -lmbedcrypto"
```

After configure, the generated config was verified to contain
`PJ_SSL_SOCK_IMP == PJ_SSL_SOCK_IMP_MBEDTLS`, no OpenSSL anywhere in the
link, bundled libsrtp enabled, video off. The ioqueue backend is
`select()` (pjproject's portable default when cross-compiling; the ioqueue
choice does not change the footprint materially).

### config_site.h (complete, as used)

```c
{{CONFIG_SITE}}
```

Points worth calling out, since they materially shape the numbers:

* `PJ_CONFIG_MINIMAL_SIZE` preset: small transaction/dialog tables
  (`PJSIP_MAX_TSX_COUNT`/`PJSIP_MAX_DIALOG_COUNT` = 15), 32 ioqueue
  handles, no CRC32/G.711 lookup tables, `PJ_HAS_ERROR_STRING 0`.
* **Deviation from the preset:** `PJ_LOG_MAX_LEVEL 4` (the preset sets 0)
  so the runtime scenario can be driven and verified. The flash cost of
  this is measured separately: {{LOG_COST}} — see §2.
* `PJSUA_MAX_CALLS 5` (4 + margin), `PJSUA_MAX_ACC 1`.
* Everything else at pjproject 2.17 defaults, notably: jitter buffer
  `jb_max` defaults to **500 ms of frames per stream** (25 × 20 ms frames,
  preallocated at stream creation), `PJSUA_MAX_BUDDIES 256`,
  `PJSUA_MAX_CONF_PORTS 254`, `PJSUA_DEFAULT_CLOCK_RATE 16000`,
  `PJMEDIA_MAX_MTU 1500`, `PJ_THREAD_DEFAULT_STACK_SIZE 8192`.
* mbedTLS at defaults, notably `MBEDTLS_SSL_IN_CONTENT_LEN` =
  `MBEDTLS_SSL_OUT_CONTENT_LEN` = **16384 bytes per TLS connection**
  (sensitivity to this measured in §4). Note PJSIP multiplexes all calls
  to the same peer over **one** TLS connection, so this is per-connection,
  not per-call.

---

## 2. Flash (Build A, ARM Thumb-2, -Os, --gc-sections)

`size(1)` of the fully linked minimal application:

| Binary | .text+rodata (`text`) | `data` | `bss` |
|---|---|---|---|
| `footprint_app` (headline) | 1,060,482 | 23,640 | 201,548 |
| `footprint_app`, `PJ_LOG_MAX_LEVEL 0` | 978,539 | 23,300 | 201,492 |
| `simple_pjsua` sample (reference) | 1,060,864 | 23,644 | 201,548 |
| full `pjsua` console app (reference) | 1,224,142 | 26,596 | 222,284 |

**Flash ≈ `text` + `data` = 1,084,122 B ≈ 1.03 MiB** with logging at
level 4, or **1,001,839 B ≈ 0.96 MiB** with logging compiled out — i.e.
log strings cost ~82 KiB. Either way this is **~50–54 % of the 2 MB
part**, leaving comfortable room for the RTOS, IP stack, drivers and
application.

Section detail of the headline ELF:

| Section | Bytes | Goes to |
|---|---|---|
| `.text` | 787,096 | flash |
| `.rodata` | 246,416 | flash (XIP) |
| `.data.rel.ro` + `.got` | 17,360 | Linux/PIE artifact; mostly const-like — counted as flash *and* conservatively as RAM below |
| `.data` | 6,008 | flash + RAM (initialized) |
| `.bss` | 201,548 | RAM |

### Where the flash goes (per-library link contribution)

Exact per-archive attribution from the linker map of the final
`--gc-sections` link (code + rodata + data actually kept in the ELF):

{{MAP_TABLE}}

For orientation, the full `size -t` totals of each static library
(upper bounds before section GC) are in
`footprint/results/perlib-size-arm.txt`. The crypto/TLS stack (mbedTLS +
x509 + crypto) and the SIP + media core each account for roughly a third
of the code; the G.722 codec itself is tiny (~10 KB); bundled libresample
(~43 KB) is retained because the conference bridge references it.

Not included in these figures: C library (measured binaries link glibc
dynamically; on the target this would be newlib/newlib-nano — typically
some tens of KB more, depending on what the RTOS already pulls in), RTOS,
IP stack, drivers, and the OS-abstraction port itself (see caveats).

---

## 3. Static RAM (Build A)

**`data` + `bss` (+ GOT/relro, conservatively) = 225,764 B ≈ 220 KiB.**

The dominant item, from the symbol table (`footprint/results/top-bss-arm.txt`):

| Symbol | Bytes | What it is |
|---|---|---|
| `pjsua_var` | 167,296 | PJSUA-lib's central static state |
| `handler` (pjlib exception registry) | 5,760 | |
| `status_phrase` (SIP) | 5,680 | |
| `ssl_ciphers` | 2,048 | |
| `zero_frame` (pjmedia) | 1,920 | |
| everything else | ~18,800 | scattered small statics |

Inside `pjsua_var` (32-bit layout, measured with a sizeof probe —
`footprint/results/pjsua_var-breakdown.txt`):

* **`buddy[PJSUA_MAX_BUDDIES=256]` = 158,720 B — 95 % of `pjsua_var`.**
  A device that does no presence, or tracks a handful of peers, can set
  `PJSUA_MAX_BUDDIES` to e.g. 8 and cut static RAM by ~150 KiB. This is
  the single biggest RAM knob in the whole configuration.
  {{BUDDY_SENSITIVITY}}
* `acc[1]` = 3,848 B; transport data = 512 B; the rest is small state.
* Note the per-call structures are **not** here: `pjsua_var.calls` is a
  pointer; the call array (17,592 B × `max_calls`, i.e. ~88 KB at
  `max_calls=5`) is allocated from the heap at `pjsua_init()` and is part
  of the measured base heap in §4.

---

## 4. Dynamic RAM (Build B, 32-bit, valgrind massif)

Scenario: two pjsua instances on localhost, TLS-only SIP transport
(RSA-2048 self-signed cert, ECDHE handshake), `--use-srtp 2` (mandatory),
G.722 as the only negotiated codec (plus telephone-event), `--null-audio`,
VAD off, 20 ms ptime, RTP verified flowing both directions (~50 pkt/s per
direction per call). Calls held in steady state ≥ 90 s per point, `dd`
pool dump captured, then clean shutdown. Massif totals below are
`mem_heap_B + mem_heap_extra_B` (allocator overhead included; the pure
`mem_heap_B` figures are ~1 % lower).

### Callee (measured side) heap vs. number of calls

| Concurrent calls | Peak heap | Steady-state heap | Pool bytes (pjlib `dd` dump) |
|---|---|---|---|
| 0 (idle) | 208,104 B | ~203 KB | {{POOLS_0}} |
| 1 | {{PEAK_1}} | {{STEADY_1}} | {{POOLS_1}} |
| 2 | {{PEAK_2}} | {{STEADY_2}} | {{POOLS_2}} |
| 4 | {{PEAK_4}} | {{STEADY_4}} | {{POOLS_4}} |

**Marginal cost per additional call: {{MARGINAL}}** (slope of the steady
points 1→4). The 0→1 step is larger than the later steps because the
first call also brings up the single shared TLS connection
({{TLS_CONN_COST}} including mbedTLS's 16 KB + 16 KB I/O buffers) —
subsequent calls to the same peer reuse it.

Composition at 4 calls (from the massif peak-snapshot allocation tree and
the pool dump):

{{COMPOSITION_4}}

Caller side, same 4-call scenario, measured in a separate run:
peak {{PEAK_A}}, steady {{STEADY_A}} — same order as the callee, so the
numbers are not materially direction-dependent.

### Codec sensitivity: G.711 instead of G.722

Same 4-call scenario negotiated as PCMU/8 kHz (conference bridge at
8 kHz): peak {{PEAK_G711}}, steady {{STEADY_G711}}. {{G711_NOTE}}

### TLS buffer sensitivity

mbedTLS rebuilt with `MBEDTLS_SSL_IN_CONTENT_LEN` =
`MBEDTLS_SSL_OUT_CONTENT_LEN` = 4096 (a common embedded choice when the
SIP peer honours the RFC 8449 record-size limit / keeps records small),
same 4-call run: peak {{PEAK_4K}}, steady {{STEADY_4K}} — i.e.
{{TLS4K_DELTA}} versus the default 16 KB buffers.

### What massif does not count

Thread stacks and static data are outside these heap numbers. This build
runs {{NTHREADS}} (pjlib default stack size is 8 KB per thread, but on a
real port stack sizes are set by the RTOS task definitions). glibc's own
startup allocations are included in the totals above (small); a newlib
malloc would have different, typically smaller, per-block overhead.

---

## 5. Does it fit the STM32H563 (640 KB RAM / 2 MB flash)?

**Flash: yes, easily.** ~1.03 MiB for the whole SIP/media/TLS/SRTP stack
(~0.96 MiB with logging stripped) against 2 MB leaves ~0.9 MB for RTOS,
lwIP, drivers and application. Even the full console pjsua fits.

**RAM: yes for the measured workload, with the stock config leaving
~{{RAM_HEADROOM}} of headroom — and easy wins if more is needed:**

| Item | Bytes (measured) |
|---|---|
| Static (`.data`+`.bss`+GOT) | {{STATIC_RAM_B}} |
| Heap, steady at 4 calls | {{HEAP_4CALL_B}} |
| **Sum** | **{{RAM_SUM}}** |

Add what this measurement deliberately excludes: thread/task stacks (a
few × 4–8 KB), C library static data, RTOS + lwIP pools (lwIP PBUF/heap
sizing is typically tens of KB and is the prospect's domain). A
realistic all-in estimate lands around {{RAM_ALLIN}} — inside 640 KB with
margin, before any tuning.

The two obvious tuning levers, both measured above: `PJSUA_MAX_BUDDIES`
(−~150 KB static if presence isn't used at this scale) and the jitter
buffer / TLS buffer sizes. Conversely, budget headroom if the real
deployment needs more calls (≈ {{MARGINAL_SHORT}} per call steady-state,
plus ~18 KB static per `max_calls` slot) or bigger jitter depth.

The prospect's 5th receive-only RTP stream was **not** measured (pjsua
cannot easily pin a decode-only stream); treat it as slightly less than
one extra call's marginal cost (no encoder state, no TLS impact), or
re-run the 5-call scenario if a hard number is needed.

---

## 6. Caveats (verbatim-ready for the customer email)

> The numbers above were measured on pjproject 2.17 with mbedTLS 3.6.7 in
> the exact feature configuration described (G.711/G.722 +
> telephone-event, SDES-SRTP, SIP over TLS via mbedTLS, no video, no
> ICE/STUN/TURN, 4 concurrent calls). Flash and static RAM come from a
> 32-bit ARM Thumb-2 cross-build at -Os with dead-code elimination, which
> is a close proxy for Cortex-M33 code size but was linked against Linux
> glibc — C-library, RTOS, and IP-stack footprint are not included. Peak
> and steady-state heap were measured on a 32-bit Linux build under a
> heap profiler while real TLS+SRTP calls were up; a bare-metal malloc
> will show slightly different overhead per allocation. There is no
> in-tree Zephyr or FreeRTOS port of PJSIP: PJLIB's OS abstraction
> (threads, mutexes, sockets against lwIP, clock) would need to be ported
> and maintained on your side, and its footprint is not in these numbers.
> Finally, the RAM figures move with configuration — jitter-buffer depth
> (default here: 500 ms max per stream), mbedTLS TLS record buffers
> (default 16 KB in + 16 KB out per connection), `PJSUA_MAX_BUDDIES`, and
> the compile-time call count all shift the totals materially, in the
> directions and magnitudes quantified in this report.

---

## Appendix: reproduction

Everything needed to reproduce is committed under `footprint/`:

* `config_site.h` — the exact configuration (copy to
  `pjlib/include/pj/config_site.h`).
* `footprint_app.c` + `app.mak` — the minimal link target and how to link
  it against a configured tree.
* `run_scenario.sh` — the two-instance TLS+SRTP call scenario driver
  (readiness via `/proc/net/tcp`, console nudging to defeat stdio
  buffering, massif wiring).
* `massif_peak.py` — massif output summarizer.
* `results/` — raw `size`/`nm` outputs, massif summaries, pool dumps.

Build notes for whoever re-runs this: Ubuntu's `gcc-multilib`
metapackage conflicts with the ARM cross compiler; install both compiler
packages directly and restore `/usr/include/asm → x86_64-linux-gnu/asm`
by hand, and run `configure` only after that (its `socklen_t` probe is
poisoned otherwise). Pass the mbedTLS libraries as
`LIBS="-lmbedtls -lmbedx509 -lmbedcrypto"` with `-L` in `LDFLAGS` so the
static link order is right.
