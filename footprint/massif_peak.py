#!/usr/bin/env python3
"""Summarize a valgrind massif output file.

Prints the peak snapshot (max mem_heap_B + mem_heap_extra_B), the last
snapshot (steady state before exit), and the top allocation subtrees of
the detailed snapshot closest to the peak.
"""
import re
import sys


def parse(path):
    snaps = []
    cur = None
    tree = []
    in_tree = False
    for line in open(path):
        line = line.rstrip("\n")
        if line.startswith("snapshot="):
            if cur is not None:
                cur["tree"] = tree
                snaps.append(cur)
            cur = {"n": int(line.split("=")[1])}
            tree = []
            in_tree = False
        elif cur is not None and "=" in line and not line.startswith(" "):
            k, v = line.split("=", 1)
            if k in ("time", "mem_heap_B", "mem_heap_extra_B", "mem_stacks_B"):
                cur[k] = int(v)
            elif k == "heap_tree":
                cur["heap_tree"] = v
                in_tree = v in ("detailed", "peak")
        elif in_tree:
            tree.append(line)
    if cur is not None:
        cur["tree"] = tree
        snaps.append(cur)
    return snaps


def fmt(b):
    return "%d B (%.1f KiB)" % (b, b / 1024.0)


def show(snap, label):
    total = snap["mem_heap_B"] + snap["mem_heap_extra_B"]
    print("%s: snapshot %d t=%dms" % (label, snap["n"], snap.get("time", -1)))
    print("  mem_heap_B       = %s" % fmt(snap["mem_heap_B"]))
    print("  mem_heap_extra_B = %s" % fmt(snap["mem_heap_extra_B"]))
    print("  total            = %s" % fmt(total))


def main():
    snaps = parse(sys.argv[1])
    if not snaps:
        print("no snapshots")
        return
    peak = max(snaps, key=lambda s: s["mem_heap_B"] + s["mem_heap_extra_B"])
    show(peak, "PEAK")
    show(snaps[-1], "LAST")
    # steady state: the value held across the longest snapshot gap (the
    # quiet hold produces no allocation events, hence no snapshots)
    gap_i, gap = None, 0
    for i in range(len(snaps) - 1):
        d = snaps[i + 1].get("time", 0) - snaps[i].get("time", 0)
        if d > gap:
            gap_i, gap = i, d
    if gap_i is not None and gap > 15000:
        show(snaps[gap_i], "STEADY(hold %.0fs)" % (gap / 1000.0))
    if "--plot" in sys.argv:
        for s in snaps:
            total = s["mem_heap_B"] + s["mem_heap_extra_B"]
            print("t=%8dms total=%9d B" % (s.get("time", -1), total))
    detailed = [s for s in snaps if s.get("heap_tree") in ("peak", "detailed")]
    if detailed:
        best = min(detailed, key=lambda s: abs(s["n"] - peak["n"]))
        print("TOP ALLOCATION SUBTREES (snapshot %d, depth<=2):" % best["n"])
        for line in best["tree"]:
            depth = (len(line) - len(line.lstrip())) // 1
            m = re.match(r"\s*n\d+: (\d+) (.*)", line)
            if not m:
                continue
            indent = len(line) - len(line.lstrip(" "))
            if indent <= 2:
                size = int(m.group(1))
                if size > 16384:
                    print("  %8d KiB  %s" % (size // 1024, m.group(2)[:100]))


if __name__ == "__main__":
    main()
