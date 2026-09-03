#!/usr/bin/env python3
"""Summarize an hwm_shim.c sample file.

PEAK   = max high-water mark over the whole run.
STEADY = the steady live-bytes level while the calls are up: the highest
         live value that persists for at least MIN_PLATEAU consecutive 1 s
         samples (teardown only lowers live, so the top sustained level is
         the all-calls-up steady state, not a setup transient).
"""
import re
import sys

MIN_PLATEAU = 4

def main(path):
    rows = []
    for line in open(path):
        m = re.match(r"t=([\d.]+) live=(\d+) hwm=(\d+) allocs=(\d+) (\w+)", line)
        if m:
            rows.append((float(m.group(1)), int(m.group(2)), int(m.group(3)), int(m.group(4))))
    if not rows:
        print("no samples"); return
    peak = max(r[2] for r in rows)
    # collapse into runs of near-equal live values (within 2 KB), keep those
    # lasting >= MIN_PLATEAU samples, then take the one with the highest live.
    plateaus = []
    i = 0
    while i < len(rows):
        j = i
        while j + 1 < len(rows) and abs(rows[j + 1][1] - rows[i][1]) < 2048:
            j += 1
        n = j - i + 1
        if n >= MIN_PLATEAU:
            lo = min(r[1] for r in rows[i:j + 1])
            hi = max(r[1] for r in rows[i:j + 1])
            plateaus.append((rows[i][1], n, rows[i][0], rows[j][0], lo, hi))
        i = j + 1
    print("samples=%d duration=%.0fs peak_hwm=%d B (%.1f KiB)" %
          (len(rows), rows[-1][0], peak, peak / 1024.0))
    for lv, n, t0, t1, lo, hi in sorted(plateaus, key=lambda p: -p[0]):
        print("  plateau live~%d B (%.1f KiB)  %ds  t=%.0f..%.0f" %
              (lv, lv / 1024.0, n, t0, t1))
    # steady = settled level: the LAST plateau (transients decay after setup,
    # so the final sustained level is steady, not the setup-burst peak).
    if plateaus:
        late = plateaus[-1]
        print("STEADY (settled, post-transient) = %d B (%.1f KiB)  t=%.0f..%.0f" %
              (late[0], late[0] / 1024.0, late[2], late[3]))
        print("SETUP-BURST high plateau = %d B (%.1f KiB)" %
              (max(p[0] for p in plateaus), max(p[0] for p in plateaus) / 1024.0))

if __name__ == "__main__":
    main(sys.argv[1])
