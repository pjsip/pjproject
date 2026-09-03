# PJSIP footprint measurement — minimal embedded configuration

**Date:** 2026-08-28
**Question answered:** does a minimal PJSIP build — G.711 A/µ-law + G.722 +
RFC 4733 telephone-event, SDES-SRTP, SIP over TLS (mbedTLS), no video, no
ICE/STUN/TURN usage, 4 concurrent bidirectional calls — fit a target with
**640 KB RAM and 2 MB flash** (STM32H563, Cortex-M33, no MMU, no filesystem)?

**Short answer:** flash fits easily (~52 % of 2 MB used). For the
prospect's exact 4-calls-plus-one-receive-only, mixed G.711/G.722 scenario,
RAM does **not** fit 640 KB even after tuning — static + heap totals
~700 KB tuned and ~0.9–1.1 MB at the call-setup peak (see §0, added
2026-09-03). The original 4-call single-codec analysis in §§1–6 is lighter
and fits after tuning; §0 explains the difference.

The figures in §§1–6 are measured; where §5 combines them into tuned or
all-in totals those are arithmetic on the measured parts and are labelled as
estimates there:

| Quantity | Measured |
|---|---|
| Flash (code + rodata + init data), logging at level 4 | 1,084,122 B (1.03 MiB) |
| Flash with logging compiled out (`PJ_LOG_MAX_LEVEL 0`) | 1,001,839 B (0.96 MiB) |
| Static RAM (`.data`+`.bss`), stock defaults | 225,188 B (220 KiB) |
| Static RAM with `PJSUA_MAX_BUDDIES 8` | 71,428 B (70 KiB) |
| Heap, idle (0 calls, steady) | 208,104 B (203 KiB) |
| Heap, 4 concurrent SRTP/G.722 calls, steady | 561,520 B (548 KiB) |
| Heap peak during 4-call setup burst | 739,560 B (722 KiB) |
| Marginal heap per call (steady, slope 1→4) | ≈ 86 KB/call |

All RAM-related numbers come from 32-bit builds; 64-bit numbers would
roughly double pointer-heavy structures and are deliberately not reported.

---

## 0. Direct answers to SIP Spectrum's follow-up (added 2026-09-03)

Scott Godin sharpened the question after reading the older public pages.
This section answers his three points with new measurements; sections 1–6
are the original 4-call single-codec analysis and still stand, but §5's fit
verdict is superseded by the corrected math here (the prospect's scenario is
heavier than §5 modelled).

### 0.1 Peak heap high-water mark (not pool-fill %), his exact scenario

Scenario measured (`run_scenario.sh mixed5`): **4 concurrent bidirectional
TLS+SRTP calls — 2 negotiated as G.722, 2 as G.711/PCMA — plus a 5th call
placed on hold so the callee carries one receive-only (decode-only) RTP
stream.** No video, no ICE, no AEC. This is his configuration, not a proxy.

The number he asked for is the **peak malloc high-water mark** and the
settled steady level, captured two independent ways that agree to within
0.2 %: valgrind massif, and an `LD_PRELOAD` allocator shim
(`footprint/hwm_shim.c`) that records live and peak malloc'd bytes exactly
as his instrumented allocator does. Both count **every** library's heap
(pjlib pools, mbedTLS, libsrtp, glibc), not a pool-fill percentage.

| Build | Peak heap HWM | Settled steady heap |
|---|---|---|
| x86-32, stock config | 891 KB | 653 KB |
| **ARM32 (real ARM build under qemu), stock** | **891 KB** | **653 KB** |
| x86-32, embedded-tuned | 843 KB | 633 KB |

The ARM32 and x86-32 numbers are identical to within 0.2 % — both are
32-bit, and the heap is dominated by buffers whose size is the same on
either ISA. (This is the "modern ARM figure" he asked for; a Cortex-M
part was not run because there is no in-tree RTOS/bare-metal port — see the
caveats. ARM32 `pjsua_call` is 136 B/call larger than x86 from 8-byte
alignment, which is in the heap figure but lost in rounding.)

"Embedded-tuned" = `PJSUA_MAX_BUDDIES 8`, mbedTLS 4 KB record buffers,
`--jb-max-size 200` ms. Note it moves the **heap** only ~20 KB steady:
the buddy table is *static* RAM, and the heap is dominated by the 88 KB
preallocated call array and per-call INVITE/dialog/stream pools that these
knobs don't touch.

The peak is a call-**setup** transient: all five calls were set up within
~20 s, so their SIP transaction buffers stacked; they age out ~32 s after
setup (`PJSIP_TRANSPORT_IDLE_TIME`), leaving the settled steady ~210–240 KB
lower. Staggered call arrival lowers the peak; four *simultaneous* incoming
INVITEs would sit near it.

### 0.2 Corrected fit verdict for the STM32H563 (640 KB / 2 MB)

Static (`.data`+`.bss`) and heap are **separate, additive** SRAM regions
(the 88 KB call array is on the heap; the 158 KB buddy table is static —
no overlap). Sizing RAM for his exact scenario:

| Config | Static | Heap steady | **Total steady** | Heap peak | Total peak |
|---|---|---|---|---|---|
| Stock defaults | 225 KB | 653 KB | **878 KB** | 891 KB | 1,116 KB |
| Embedded-tuned | 71 KB | 633 KB | **704 KB** | 843 KB | 914 KB |

Plus ~40 KB of thread stacks (5 threads × 8 KB default) and lwIP pools on
his side. **Even the tuned steady total (704 KB) exceeds the 640 KB budget
by ~49 KB, and sizing for the setup peak needs ~0.9–1.1 MB.** So, plainly:
**at his exact scenario PJSIP does not fit 640 KB, even after tuning.** A
realistic RAM budget for this stack and workload is ~1 MB.

This corrects §5, which modelled 4 *single-codec* G.722 calls (heap
561 KB) and concluded "fits with 22 KB spare" after `buddies=8`. His 4+1
*mixed* scenario carries ~92 KB more heap — the 5th stream plus per-stream
resampler state for the 8 kHz G.711 ports on the 16 kHz bridge — so the
same tuned config lands at 704 KB, not 633 KB, and no longer fits.

Paths to fit 640 KB, in rough order of leverage: drop below `pjsua-lib` to
raw `pjsip`+`pjmedia` (removes the 88 KB preallocated call array and the
conference-bridge/resampler ports — the biggest single heap item); a much
shallower jitter buffer; fewer preallocated call slots; or a part with
~1 MB SRAM. The receive-only 5th stream here is a *held SIP call* (upper
bound); a raw RTP listener would cost less.

### 0.3 Calibration against his libre figure (~25 KB steady)

His libre measurement (~25 KB steady, high-30s KB peak) and this PJSIP
number (~653 KB steady) differ ~26×. That gap is real and mostly
architectural, but part of it is **denominator**, and it's worth being
explicit so the comparison is fair:

- **What the PJSIP bytes buy** (mixed5 peak attribution): pjlib pools
  813 KB (of which the 88 KB preallocated call array, ~16 KB/stream jitter
  buffers at the 500 ms default, INVITE/dialog/transaction pools), mbedTLS
  record buffers 33.5 KB, libsrtp ~16 KB, glibc stdio 12 KB.
- **Likely not apples-to-apples:** an allocator that instruments only
  libre's own `mem_alloc`/`mem_deref` will not see OpenSSL/mbedTLS record
  buffers, libsrtp contexts, or static arrays at all. If his 25 KB is
  libre-core-only, the honest comparison to *our* all-in 653 KB is our
  pjlib-pools-only figure (~460–530 KB) — still ~20×, but not 26×. Two
  questions worth asking him: did the libre run include TLS + SRTP, and at
  what jitter-buffer depth; and did his allocator count heap only or
  heap + static.
- **The gap is design, not measurement error:** PJSIP via `pjsua-lib` is a
  batteries-included high-level stack (conference bridge, presence, account
  management, media-transport abstraction) that preallocates and pools;
  libre is a lean core. If 640 KB is a hard constraint, that difference is
  exactly what decides the part — and it points away from `pjsua-lib`.

### 0.4 G.711-vs-G.722 CPU ratio (his DSP-vs-MCU question)

Measured on PJSIP's own codecs with a microbenchmark
(`footprint/codec_bench.c`): encode + decode of one 10 ms frame, counted
two ways.

| Codec | Instructions/frame (callgrind) | µs/frame (native x86-32) |
|---|---|---|
| G.722 @ 16 kHz | 219,481 | 28.2 |
| G.711 computed (minimal-size default) | 9,613 | 1.02 |
| G.711 table-based | 2,096 | 0.35 |

- **Ratio G.722 / G.711 = 23× with computed G.711, 105× with table-based
  G.711.** The old "~55× on ARM9" page sits between these, consistent with
  a table-based G.711 build.
- The reason the ratio swings so much: **G.722 is the fixed cost** — plain
  C fixed-point, a 24-tap QMF analysis + synthesis filter and full sub-band
  ADPCM, run at twice G.711's sample rate, with no DSP intrinsics. G.711 is
  near-free, so the ratio is governed entirely by whether G.711 is a table
  lookup or computed. `PJ_CONFIG_MINIMAL_SIZE` sets
  `PJMEDIA_HAS_ALAW_ULAW_TABLE 0` (computed) to save flash/RAM, which is why
  our default ratio is ~23× rather than ~100×.
- **For the DSP-vs-MCU decision:** one bidirectional G.722 call is ~21.9 M
  instructions/s (enc+dec); four G.722 calls ≈ 88 M inst/s, roughly a third
  to half of a 200–250 MHz Cortex-M core spent purely on the G.722 codec,
  before RTP/SRTP/jitter/mixing. Four G.711 calls (computed) ≈ 3.8 M
  inst/s — negligible. That is the gap that justifies a DSP/hardware codec
  path: **G.722 in software is a real load on the MCU; G.711 is not.** Two
  related notes: a mixed device also pays software resampling (G.711's
  8 kHz ↔ the 16 kHz bridge) and packet-loss concealment; and on the H563
  the SRTP AES/HMAC per packet is itself a candidate for the on-chip
  AES/HASH accelerator, which libsrtp is not wired to by default.


---

## 1. What exactly was measured

### Versions and toolchains

| Component | Version |
|---|---|
| pjproject | tag `2.17` (commit `5a457451fa2712ba18e12b01738e8ff3af2b26fd`, 2026-04-22) |
| mbedTLS | 3.6.7 (LTS), default `mbedtls_config.h` |
| SRTP | bundled `third_party/srtp` as shipped in 2.17 |
| Cross toolchain (Build A) | Ubuntu `arm-linux-gnueabihf-gcc` 13.3.0, `-Os -g0 -mthumb -ffunction-sections -fdata-sections`, linked `-Wl,--gc-sections` |
| Host toolchain (Build B) | Ubuntu gcc 13.3.0, `-m32 -Os -g` + the same section-GC flags |
| Heap profiler | valgrind massif (`--pages-as-heap=no`), on the 32-bit binaries |

Two builds from the same tree state and the **same `config_site.h`**
(committed at `footprint/config_site.h`, reproduced below):

* **Build A — flash + static RAM proxy:** cross-compiled to 32-bit ARM
  Thumb-2 (Linux EABI hard-float, ARMv7-A + double-precision VFP).
  Thumb-2 code size is within a few percent across M-/A-profile, so this
  stands in for Cortex-M33 code size, with two known deviations: the
  Cortex-M33 has single-precision (FPv5-SP) FPU only, so `double` math
  becomes libgcc soft-float (a little more code and more cycles), and this
  PIE binary carries ~17 KB of `.data.rel.ro`/`.got` that a bare-metal
  non-PIE image would not. The measured
  ELF is `footprint/footprint_app.c`, a minimal-but-complete PJSUA
  application that references the TLS transport, SDES-SRTP, G.711/G.722,
  the conference bridge and the null audio device — a real
  `--gc-sections` link that keeps exactly what a minimal endpoint uses.
  The build reproduces byte-identically across clean rebuilds.
* **Build B — dynamic RAM:** the same configuration built natively for
  32-bit x86 and exercised with real calls between two `pjsua` instances
  (TLS-only signaling, SRTP mandatory), measured under massif.

### Configure line (identical feature set in both builds)

```
./configure --host=<arm-linux-gnueabihf | i686-pc-linux-gnu> \
    --disable-video --disable-sound --disable-speex-aec \
    --disable-speex-codec --disable-gsm-codec --disable-ilbc-codec \
    --disable-l16-codec --disable-g7221-codec --disable-opencore-amr \
    --disable-silk --disable-opus --disable-bcg729 --disable-libwebrtc \
    --disable-upnp --disable-v4l2 --with-mbedtls=<prefix> \
    CFLAGS="-Os ..." LDFLAGS="-Wl,--gc-sections ..." \
    LIBS="-lmbedtls -lmbedx509 -lmbedcrypto"
```

After configure, the generated config was verified to contain
`PJ_SSL_SOCK_IMP == PJ_SSL_SOCK_IMP_MBEDTLS`, no OpenSSL anywhere in the
link, bundled libsrtp enabled, video off. The ioqueue backend is
`select()` (pjproject's portable default when cross-compiling); the
ioqueue choice does not move the footprint materially.

### config_site.h (complete, as used)

```c
/* Start from the smallest-footprint preset. */
#define PJ_CONFIG_MINIMAL_SIZE
#include <pj/config_site_sample.h>

/*
 * Deviation from the preset (which sets log level 0): keep runtime
 * logging compiled in at level 4.  Needed to drive and verify the
 * runtime measurement, and closer to a shippable embedded config than
 * a fully silent build.  Flash cost measured separately (§2).
 */
#undef PJ_LOG_MAX_LEVEL
#define PJ_LOG_MAX_LEVEL                    4

/* Media features. */
#ifndef PJMEDIA_HAS_VIDEO
#   define PJMEDIA_HAS_VIDEO                0
#endif
#define PJMEDIA_HAS_SRTP                    1

/* Codecs: G.711 (PCMA/PCMU) + G.722 only.  RFC 4733 telephone-event
 * support is built into pjmedia_stream and needs no codec macro. */
#ifndef PJMEDIA_HAS_G711_CODEC
#   define PJMEDIA_HAS_G711_CODEC           1
#endif
#ifndef PJMEDIA_HAS_G722_CODEC
#   define PJMEDIA_HAS_G722_CODEC           1
#endif
/* ...speex/gsm/ilbc/l16/g7221/amr/opus/silk all forced to 0
   (see footprint/config_site.h for the full guarded list)... */

/* Application sizing: 4 concurrent calls + 1 margin, single account. */
#define PJSUA_MAX_CALLS                     5
#define PJSUA_MAX_ACC                       1
```

Defaults left in effect that materially shape the numbers (all
pjproject 2.17 / mbedTLS 3.6.7 defaults):

* `PJ_CONFIG_MINIMAL_SIZE` preset: 15-entry transaction/dialog hash
  tables, 32 ioqueue handles, no CRC32/G.711 lookup tables,
  `PJ_HAS_ERROR_STRING 0`.
* Jitter buffer: pjsua leaves `jb_max` at −1 → stream default of
  **500 ms of frames per stream** (25 × 20 ms frames, preallocated at
  stream creation inside the per-call `strm` pool).
* `PJSUA_MAX_BUDDIES 256` (presence buddy table — see §3, this is the
  big one), `PJSUA_MAX_CONF_PORTS 254`, `PJSUA_DEFAULT_CLOCK_RATE
  16000`, `PJMEDIA_MAX_MTU 1500`, `PJ_THREAD_DEFAULT_STACK_SIZE 8192`.
* mbedTLS `MBEDTLS_SSL_IN_CONTENT_LEN` = `MBEDTLS_SSL_OUT_CONTENT_LEN` =
  **16384 per TLS connection** (sensitivity measured in §4). PJSIP
  multiplexes all calls to the same peer over **one** TLS connection, so
  this is per-connection, not per-call.

---

## 2. Flash (Build A — ARM Thumb-2, -Os, --gc-sections)

`size(1)` of the fully linked minimal application:

| Binary | text (code+rodata) | data | bss |
|---|---|---|---|
| **`footprint_app` (headline)** | **1,060,482** | **23,640** | **201,548** |
| `footprint_app`, `PJ_LOG_MAX_LEVEL 0` | 978,539 | 23,300 | 201,492 |
| `footprint_app`, `PJSUA_MAX_BUDDIES 8` | 1,060,358 | 23,640 | 47,788 |
| `simple_pjsua` sample (reference) | 1,060,864 | 23,644 | 201,548 |
| full `pjsua` console app (reference) | 1,224,142 | 26,596 | 222,284 |

**Flash ≈ text + data = 1,084,122 B ≈ 1.03 MiB** with logging at level 4,
or **1,001,839 B ≈ 0.96 MiB** with logging compiled out (log strings and
call sites cost ~80 KiB / 82 KB). Either way **~48–52 % of the 2 MiB part**,
leaving ~1 MB for RTOS, IP stack, drivers and application. Linking
`simple_pjsua` instead gives the same number (±0.4 KB), so the result is
not an artifact of the app chosen; the full interactive pjsua console
costs ~163 KB more in code (167 KB including its extra data).

Section detail of the headline ELF: `.text` 787,096, `.rodata` 246,416,
`.data.rel.ro`+`.got` 17,360 (Linux/PIE artifact, counted in both flash
and RAM to stay conservative), `.data` 6,008, `.bss` 201,548.

### Where the flash goes

Exact per-archive bytes kept in the final ELF, from the linker map
(`footprint/mapsum.py`; string constants are merged across objects by the
linker, so they are shown as one line, which also absorbs ~27 KB of
non-string text-class metadata (`.plt`/`.rel.dyn`/`.eh_frame`/`.init`);
the columns reconcile to the ELF `text`+`data`):

| Archive | code+const (B) | share |
|---|---|---|
| mbedTLS total (`libmbedcrypto` 156,432 + `libmbedtls` 102,400 + `libmbedx509` 13,188) | 272,020 | 26 % |
| string constants (merged) + link/EH metadata (residual) | 152,972 | 14 % |
| `libpjmedia` | 118,766 | 11 % |
| `libpjsip` | 107,024 | 10 % |
| `libpjsua` (pjsua-lib) | 102,067 | 10 % |
| `libpjnath` (linked by pjsua-lib; unused at runtime here) | 69,232 | 7 % |
| `libpj` (pjlib) | 58,088 | 5 % |
| `libpjsip-ua` | 44,136 | 4 % |
| `libresample` (bundled, referenced by conf bridge) | 43,323 | 4 % |
| `libpjsip-simple` | 27,810 | 3 % |
| `libsrtp` | 27,002 | 3 % |
| `libpjlib-util` | 25,413 | 2 % |
| `libpjmedia-codec` (G.722 + G.711 glue) | 7,747 | <1 % |
| libgcc + app + audiodev | 4,882 | <1 % |
| **Total (= ELF text exactly)** | **1,060,482** | |

Reading of that table: the TLS/crypto stack is the single largest block
(~26 %); SIP core+UA+simple+pjsua-lib together ~27 %; media ~12 %. If
flash ever became tight: pjnath (69 KB) is linked because pjsua-lib
references ICE/TURN entry points even when unused; libresample (43 KB)
could be dropped with a resample-less configuration; and stripping
logging saves the ~80 KiB noted above.

Not included: C library (these binaries link glibc dynamically; on the
target this is newlib/newlib-nano — typically tens of KB extra depending
on what the RTOS already carries), RTOS, IP stack, drivers, and the
PJLIB OS-abstraction port itself.

---

## 3. Static RAM (Build A)

**`data` + `bss` = 225,188 B ≈ 220 KiB** with stock defaults.

The symbol table says where it lives: `pjsua_var` (pjsua-lib's central
state) is **167,296 B of the 201,548 B bss**. A 32-bit sizeof probe
(`footprint/results/pjsua_var-breakdown.txt`) splits `pjsua_var`:

* **`buddy[PJSUA_MAX_BUDDIES=256]` = 158,720 B — 95 % of `pjsua_var`.**
  This is the presence buddy table, sized by default for a desktop
  softphone. **Measured:** rebuilding with `PJSUA_MAX_BUDDIES 8` drops
  bss to 47,788 B → **static RAM 71,428 B (70 KiB), a 154 KB saving**,
  with essentially unchanged code size (−124 B). For a 4-call embedded
  endpoint this setting
  is the single biggest RAM decision in the whole stack.
* The per-call structures are *not* static: `pjsua_var.calls` is a
  pointer; the call array (17,592 B × `max_calls`, ≈ 88 KB at
  `max_calls=5`) is pool-allocated at `pjsua_init()` and shows up in the
  base heap of §4. Dropping `max_calls` to 4 saves 17.6 KB of heap.
* Everything else is small: the SIP header-parser dispatch table
  (`handler[PJSIP_MAX_HEADER_TYPES]` in `sip_parser.c`) 5,760, SIP status
  phrases 5,680, TLS cipher list 2,048, etc.

---

## 4. Dynamic RAM (Build B — 32-bit, valgrind massif)

Scenario: two pjsua instances on localhost; TLS-only SIP transport
(RSA-2048 self-signed certificate; negotiated cipher
`TLS-ECDHE-RSA-WITH-CHACHA20-POLY1305-SHA256`); `--use-srtp 2`
(mandatory); G.722 the only negotiated codec (plus telephone-event);
`--null-audio`, VAD off, 20 ms ptime. RTP was verified flowing both
directions (~50 pkt/s per direction per call). Calls held in steady
state ≥ 45 s per data point (≥ 90 s for the 4-call runs); `dd` pool dump
captured before shutdown.
Massif totals are `mem_heap_B + mem_heap_extra_B` (allocator bookkeeping
included; pure `mem_heap_B` is ~0.4–0.7 % lower).

### Callee heap vs. number of calls

| Concurrent calls | Peak | Steady | pjlib pools (capacity / used) |
|---|---|---|---|
| 0 (idle) | 208,104 | 208,104 | 187,428 / 169,820 |
| 1 | 418,576 | 302,776 | 275,600 / 243,727 |
| 2 | 525,736 | 391,240 | 360,164 / 316,446 |
| 4 | 739,560 | 561,520 | 528,792 / 461,920 |

* **Base (idle) heap: 208 KB**, of which ~88 KB is the preallocated
  call array (`max_calls=5` × 17,592 B) inside the 91 KB `pjsua` pool,
  plus endpoint/media-endpoint/conference pools.
* **Marginal cost per call: ≈ 86 KB steady** (slope 1→4 = 86,248 B;
  individual steps 85.1–88.5 KB; the first call costs 94.7 KB because it
  also brings up one-time singletons). Per-call pools, from the dump:
  INVITE session 32 KB, stream+jitter buffer 16 KB (at the default
  500 ms `jb_max`), RTP media transport 10.8 KB, dialog 8 KB, SRTP
  transport 6 KB, other pools plus non-pool malloc growth ~13 KB.
* **Peaks happen at call setup**, not steady state: the 4-call setup
  burst peaks 178 KB above steady. Attribution at the peak snapshot:
  ~136 KB is SIP transaction/`tx_data` transients that age out ~32 s
  after setup (they scale with how many calls are set up within a ~32 s
  window), ~33.5 KB is the live TLS connection, the rest x509/bignum
  handshake transients. libsrtp itself is small: 12.7 KB total at 4
  calls (~3.2 KB/call). glibc stdio contributes 12.3 KB.

### The TLS connection — lifecycle and cost

Measured behaviour worth knowing: pjsip destroyed the (single, shared)
TLS connection ~32 s after the last INVITE transaction finished
("`destroyed due to timeout in idle timer`" in the log) — media continues
on SRTP regardless. The steady figures above therefore contain **no**
live TLS connection. A deployment that keeps the connection up (e.g.
registrations with keep-alive, which is what the prospect's device will
do) should budget on top of steady:

* **33,530 B** — mbedTLS's per-connection record buffers (2 × 16,765 B
  at the default 16 KB `MBEDTLS_SSL_IN/OUT_CONTENT_LEN`), plus ~6 KB
  pjsip transport pool.
* Rebuilt with 4 KB in/out record buffers, those become 2 × 4,477 B =
  8,954 B, and the measured 4-call **peak drops 24.6 KB** (739,560 →
  714,984); steady is unchanged (the connection was closed by then).
  4 KB buffers are viable when the peer keeps TLS records small
  (RFC 8449 record_size_limit or operator control of the SIP server);
  SIP-over-TLS messages themselves are far below 4 KB here.

Caller side (same 4-call scenario, measured separately): peak 835,072,
steady 649,792 — ~88 KB above the callee, consistent with its
client-side TLS connection staying open through the hold plus larger
UAC-side dialog/transaction state. Use the caller numbers if the device
originates the calls.

### Codec sensitivity

G.711 (PCMU, 8 kHz conference clock) instead of G.722 at 4 calls:
steady 554,232 vs 561,520 (−7.3 KB), peak 733,312 vs 739,560. Codec
choice between two *single-codec* runs is roughly RAM-neutral at this
scale; the jitter buffer stores encoded frames (160 B per 20 ms for both).
This is **not** the same as running both codecs at once: a mixed device
puts the 8 kHz G.711 ports and the 16 kHz bridge on opposite clocks, which
adds per-port resampler state (and CPU). The prospect's mixed scenario is
measured directly in §0.1, not inferred from these single-codec runs.

### What the heap numbers do not include

Thread stacks and static data. The measured process runs **5 threads**
(main/console plus pjsua's worker, conference clock and internal
threads); pjlib's
default stack request is 8 KB per thread, but on an RTOS port task
stacks are set by the port. glibc startup overhead is included in the
totals (small); newlib's per-allocation overhead differs slightly.

---

## 5. Does it fit the STM32H563 (640 KB RAM / 2 MB flash)?

**Flash: yes, comfortably.** 1.03 MiB (or 0.96 MiB without logging) of
2 MB, leaving ~1 MB for RTOS, lwIP, drivers, application.

**RAM: not with stock defaults — yes with tuning, and the margin then
depends on peak behaviour.** Against the 655,360 B budget:

> **Note (2026-09-03):** this table is for **4 single-codec G.722 calls**.
> The prospect's actual **4 + 1 mixed** scenario carries ~92 KB more heap;
> its corrected fit table is in §0.2, where even the tuned config exceeds
> 640 KB. Read §0.2 as the binding answer for his workload.

| Configuration | Static | Heap | Total | Verdict |
|---|---|---|---|---|
| Stock defaults, steady 4 calls | 225,188 | 561,520 | 786,708 | **over budget by ~131 KB** |
| Stock defaults, worst case (setup burst peak) | 225,188 | 739,560 | 964,748 | over by ~309 KB |
| `PJSUA_MAX_BUDDIES 8` (measured), steady | 71,428 | 561,520 | 632,948 | fits, ~22 KB spare |
| `PJSUA_MAX_BUDDIES 8`, setup-burst peak | 71,428 | 739,560 | 810,988 | over by ~156 KB |

…and the total must still leave room for task stacks (5 threads here;
budget 32–64 KB), newlib static data, and lwIP pools (their domain,
typically 30–80 KB). So the honest engineering answer for the prospect:

* The **feature set fits**, but RAM needs a deliberate configuration
  pass, not default settings. The levers, all quantified above:
  buddy table (−154 KB static, measured), TLS record buffers (−24.6 KB
  peak, measured), `max_calls` 5→4 (−17.6 KB, measured struct size),
  jitter-buffer depth (default is a generous 500 ms; the 16 KB/call
  stream pool shrinks with it — not separately measured), and
  peak shaving: the ~136 KB setup transient scales with how many calls
  are *set up* inside a ~32 s window — 4 calls arriving staggered, or
  admission control on simultaneous setups, keeps peak near steady.
* With buddy table fixed and either 4 KB TLS buffers + staggered setup
  or a modest jitter-buffer reduction, steady-state sits ≈ 630 KB
  worst-case-peak ≈ 660–700 KB — i.e. the last ~10 % has to come from
  the tuning pass and stack/lwIP budgeting on their side. It is close
  enough that we should say "yes, with a documented configuration, and
  here is the measured breakdown", not an unconditional yes.

The prospect's 5th receive-only RTP stream was **not** measured (pjsua
cannot easily pin a decode-only stream); budget it at up to one call's
marginal cost (≈ 86 KB) as an upper bound — it has no TLS impact and no
encoder state, so the true cost is lower.

---

## 6. Caveats (verbatim-ready for the customer email)

> The numbers above were measured on pjproject 2.17 with mbedTLS 3.6.7
> in the exact feature configuration described (G.711/G.722 +
> telephone-event, SDES-SRTP, SIP over TLS via mbedTLS, no video, no
> ICE/STUN/TURN, 4 concurrent calls). Flash and static RAM come from a
> 32-bit ARM Thumb-2 cross-build at -Os with dead-code elimination,
> which is a close proxy for Cortex-M33 code size but was linked against
> Linux glibc — C-library, RTOS, and IP-stack footprint are not
> included. Peak and steady-state heap were measured on a 32-bit Linux
> build under a heap profiler while real TLS+SRTP calls were up; a
> bare-metal malloc will show slightly different per-allocation
> overhead. There is no in-tree Zephyr or FreeRTOS port of PJSIP:
> PJLIB's OS abstraction (threads, mutexes, sockets against lwIP,
> clock/timers) would need to be ported and maintained on your side, and
> neither its footprint nor lwIP's is in these numbers. Finally, the RAM
> figures move materially with configuration — the presence buddy table
> (158 KB at defaults), jitter-buffer depth (500 ms max per stream by
> default), mbedTLS TLS record buffers (16 KB + 16 KB per connection by
> default), and the compile-time call count all shift the totals in the
> directions and magnitudes quantified in this report — so treat the
> defaults-as-shipped numbers and the tuned numbers as the bounds of the
> envelope rather than a single figure.

---

## Appendix: reproduction

Everything needed to reproduce is committed under `footprint/`:

* `config_site.h` — the exact configuration (copy to
  `pjlib/include/pj/config_site.h` before configure).
* `footprint_app.c` + `app.mak` — the minimal link target and how to
  link it against a configured tree (`EXTRA_LDFLAGS=-Wl,-Map=…` for the
  attribution map).
* `run_scenario.sh` — the two-instance TLS+SRTP scenario driver
  (readiness polled from `/proc/net/tcp`; pjsua buffers both its console
  and its log file, so the script "nudges" the console with blank lines
  to force flushes; log files are complete only after a clean quit).
* `massif_peak.py` — massif summarizer (peak, steady-across-the-hold,
  top allocation subtrees). `mapsum.py` — linker-map per-archive sums.
* `results/` — raw size/nm outputs, per-run massif summaries, the 4-call
  pool dump, and `summary.txt` with every number in this report.

Environment notes for re-runs: Ubuntu's `gcc-multilib` metapackage
conflicts with the ARM cross compiler; install `gcc-13-multilib` and the
cross packages side by side and restore the `/usr/include/asm →
x86_64-linux-gnu/asm` symlink by hand, and run `configure` only after
that (its `socklen_t` probe is otherwise poisoned by the missing asm
headers). Pass the mbedTLS libraries as `LIBS="-lmbedtls -lmbedx509
-lmbedcrypto"` with the `-L` path in `LDFLAGS` so the static link order
is correct, and point `--with-mbedtls` at the install prefix.
