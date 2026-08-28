# PJSIP footprint measurement — minimal embedded configuration

**Date:** 2026-08-28
**Question answered:** does a minimal PJSIP build — G.711 A/µ-law + G.722 +
RFC 4733 telephone-event, SDES-SRTP, SIP over TLS (mbedTLS), no video, no
ICE/STUN/TURN usage, 4 concurrent bidirectional calls — fit a target with
**640 KB RAM and 2 MB flash** (STM32H563, Cortex-M33, no MMU, no filesystem)?

**Short answer:** flash fits easily (~52 % of 2 MB used). RAM does **not**
fit with stock defaults — but it does fit after configuration tuning whose
biggest lever is measured in this report, with the caveat that the margin
then depends on peak (call-setup) behaviour, also quantified below. All
numbers are measured, none extrapolated:

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
  Thumb-2 (Linux EABI hard-float). Thumb-2 code size is close across
  M-/A-profile, so this stands in for Cortex-M33 code size. The measured
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
call sites cost ~82 KiB). Either way **~50–54 % of the 2 MB part**,
leaving ~1 MB for RTOS, IP stack, drivers and application. Linking
`simple_pjsua` instead gives the same number (±0.4 KB), so the result is
not an artifact of the app chosen; the full interactive pjsua console
costs ~160 KB more.

Section detail of the headline ELF: `.text` 787,096, `.rodata` 246,416,
`.data.rel.ro`+`.got` 17,360 (Linux/PIE artifact, counted in both flash
and RAM to stay conservative), `.data` 6,008, `.bss` 201,548.

### Where the flash goes

Exact per-archive bytes kept in the final ELF, from the linker map
(`footprint/mapsum.py`; string constants are merged across objects by the
linker, so they are shown as one line — the columns sum exactly to the
ELF sections):

| Archive | code+const (B) | share |
|---|---|---|
| mbedTLS total (`libmbedcrypto` 156,432 + `libmbedtls` 102,400 + `libmbedx509` 13,188) | 272,020 | 26 % |
| string constants, merged (log/format strings of everything) | 152,972 | 14 % |
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
(~26 %); SIP core+UA+simple+pjsua-lib together ~27 %; media ~13 %. If
flash ever became tight: pjnath (69 KB) is linked because pjsua-lib
references ICE/TURN entry points even when unused; libresample (43 KB)
could be dropped with a resample-less configuration; and stripping
logging saves the 82 KiB noted above.

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
  with unchanged code size. For a 4-call embedded endpoint this setting
  is the single biggest RAM decision in the whole stack.
* The per-call structures are *not* static: `pjsua_var.calls` is a
  pointer; the call array (17,592 B × `max_calls`, ≈ 88 KB at
  `max_calls=5`) is pool-allocated at `pjsua_init()` and shows up in the
  base heap of §4. Dropping `max_calls` to 4 saves 17.6 KB of heap.
* Everything else is small: exception registry 5,760, SIP status
  phrases 5,680, TLS cipher list 2,048, etc.

---

## 4. Dynamic RAM (Build B — 32-bit, valgrind massif)

Scenario: two pjsua instances on localhost; TLS-only SIP transport
(RSA-2048 self-signed certificate; negotiated cipher
`TLS-ECDHE-RSA-WITH-CHACHA20-POLY1305-SHA256`); `--use-srtp 2`
(mandatory); G.722 the only negotiated codec (plus telephone-event);
`--null-audio`, VAD off, 20 ms ptime. RTP was verified flowing both
directions (~50 pkt/s per direction per call). Calls held in steady
state ≥ 90 s per data point; `dd` pool dump captured before shutdown.
Massif totals are `mem_heap_B + mem_heap_extra_B` (allocator bookkeeping
included; pure `mem_heap_B` is ~1 % lower).

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
  transport 6 KB, group locks/misc ~13 KB.
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
choice between these two is RAM-neutral at this scale; the jitter
buffer stores encoded frames (160 B per 20 ms for both).

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
