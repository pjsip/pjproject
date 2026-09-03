Subject: Re: PJSIP footprint for your STM32H563 target — measured your exact scenario

Hi Scott,

Good to reconnect — SIPit feels like a while ago now. Thanks for being so
precise about what you actually need; it made this easy to go measure
properly rather than point you at the old pages again. You were right about
both of their problems: the footprint page reports pool-fill percentage, not
a high-water mark, and neither page is on a platform anywhere near your
target. So I built your exact scenario and measured it two ways. Short
version up front, detail below, and a full write-up attached.

THE HEADLINE

Your scenario — 4 concurrent bidirectional TLS+SRTP calls, 2 on G.722 and
2 on G.711, plus a 5th stream I put on hold so one side is receive-only,
no video/ICE/AEC — measured as peak malloc high-water mark, not pool fill:

  peak heap        ~891 KB   (call-setup transient)
  steady heap      ~653 KB
  + static RAM     ~225 KB stock  /  ~71 KB with the buddy table shrunk

I measured that both under valgrind massif and under an LD_PRELOAD
allocator shim that records the same live/peak bytes your instrumented
allocator would; they agree to within 0.2%. I also ran it on a real 32-bit
ARM build (under qemu-arm, since I can't stand up a Cortex-M here) — the ARM
and x86 numbers are identical to within 0.2%, which makes sense: both are
32-bit and the heap is dominated by buffers of the same size on either ISA.
So the ARM figure is the one to quote, and it's the same ~891/653 KB.

The honest conclusion, which is not the one the old pages imply: at your
exact scenario PJSIP does not fit 640 KB, and tuning doesn't rescue it.
Static and heap are separate additive regions, so:

  stock defaults   225 KB static + 653 KB heap = ~878 KB steady
  embedded-tuned    71 KB static + 633 KB heap = ~704 KB steady
                    (tuned = buddies 8, mbedTLS 4 KB record buffers, 200 ms JB)

Even the tuned steady total is ~49 KB over 640 KB, and if you size for the
call-setup peak you're looking at ~0.9–1.1 MB. Tuning moves the heap only
about 20 KB, because the big heap items — an 88 KB call array pjsua-lib
preallocates, and the per-call jitter/dialog/transaction pools — aren't
what those knobs touch. Realistically this workload wants a part with ~1 MB
SRAM, or a different integration strategy (more below).

I'd rather tell you that now than after you've committed to the H563.

ON YOUR LIBRE NUMBER (~25 KB steady)

That's ~26x below our ~653 KB, and I don't want to wave that away. Most of
the gap is architectural — PJSIP via pjsua-lib is a batteries-included
stack (conference bridge, presence, account manager, media-transport
abstraction) that preallocates and pools; libre is a lean core. But part of
it may be the denominator, and it's worth pinning down before you treat the
two as directly comparable:

  - Our 653 KB is TOTAL process malloc — it includes mbedTLS's record
    buffers (~33 KB), libsrtp contexts (~16 KB), everything. An allocator
    that instruments only libre's own mem_alloc/mem_deref wouldn't see the
    TLS library's or libsrtp's heap at all.
  - So two questions that would make this apples-to-apples: did your libre
    run have TLS + SRTP actually up, and at what jitter-buffer depth? And
    did your allocator count heap only, or heap + static?
  - If your 25 KB is libre-core-only, the fair comparison to it is our
    pjlib-pools-only figure, ~460–530 KB — still ~20x, but not 26x.

Either way the gap is real and it's the kind of thing that decides a part
number, so I'd rather you have the honest version.

THE G.711-vs-G.722 CPU RATIO

This one I can give you a current, directional answer on. I microbenchmarked
PJSIP's own codecs (encode+decode of a 10 ms frame, instruction counts under
callgrind and native wall-clock):

  G.722 @ 16 kHz                         219,481 instr/frame   28.2 us
  G.711, computed (our minimal default)    9,613 instr/frame    1.0 us
  G.711, table-based                       2,096 instr/frame    0.35 us

So the ratio is ~23x if G.711 is computed, ~105x if it's a table lookup.
Your ~55x from the ARM9 page is consistent with a table-based G.711 build.
The reason it swings so much: G.722 is the fixed cost — plain-C fixed-point,
a 24-tap QMF filter plus full sub-band ADPCM at twice G.711's sample rate,
no DSP intrinsics — so the ratio is really just "how cheap is G.711," and
that's a table-vs-computed choice. (Our minimal-size preset compiles the
G.711 tables out to save flash, which is why our default ratio is the low
end.)

For your DSP-vs-MCU decision, the number that matters: four G.722 calls is
~88 M instructions/sec of codec work — roughly a third to a half of a
200–250 MHz Cortex-M core, before you pay for RTP, SRTP and mixing. Four
G.711 calls is ~4 M/sec, i.e. noise. So G.722 in software is a real load on
the MCU and a credible candidate for a DSP or hardware path; G.711 isn't
worth offloading. Two adjacent things worth a look on the H563: a mixed
G.711/G.722 device also pays software resampling between 8 and 16 kHz, and
the H563's on-chip AES/HASH could take the SRTP crypto off the core, which
libsrtp doesn't wire up by default.

WHERE THAT LEAVES US

If 640 KB is fixed and you want to stay on PJSIP, the lever with real
leverage is dropping below pjsua-lib to raw pjsip + pjmedia — that removes
the 88 KB preallocated call array and the conference-bridge/resampler ports,
which is the single biggest chunk of the heap. I haven't measured that
configuration yet; if it's useful I can put a number on it, and on a harder
jitter-buffer setting, so you can see how far PJSIP can actually be pushed
down before the porting effort (there's no in-tree Cortex-M/RTOS port, so
PJLIB's OS abstraction against your RTOS + lwIP is work on your side either
way). Happy to do that, or to talk through whether a ~1 MB-SRAM part changes
the calculus for you.

Full write-up with the exact config, the reproduction harness, and every
raw number is attached; there's also a one-page visual summary if that's
easier to forward: <artifact link>

Best,
Benny

---
Attachments / links:
- footprint-report.md (§0 answers your questions directly; §§1–6 are the
  underlying 4-call analysis and method)
- visual summary: https://claude.ai/code/artifact/d0e79652-aae7-4637-98b1-a7f70c682812
- harness + raw data: footprint/ on branch claude/execute-plan-3h0x0y
