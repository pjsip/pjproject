#!/usr/bin/env python3
# Compare an embedded_ua build and its self-call run with the committed
# footprint limits of a target: flash and RAM from the build log, the heap's
# high-water mark from the run log (embedded_ua's "C heap:" line after "Self
# calls done"). Growth beyond the tolerance fails; a shrink is only reported,
# so that the limit gets lowered by hand.
#
#   footprint_check.py <target> <build.log> <run.log> <limits.json> [tol%]
#
# limits.json is a list of {"target", "flash", "ram", "heap_peak"}, one per
# target. ci-zephyr.yml runs it; to refresh a target's entry, paste the JSON
# it prints.
import json
import os
import re
import sys


def measure(build_log, run_log):
    build = open(build_log).read()
    run = open(run_log).read()
    flash = re.search(r"^\s*FLASH:\s+(\d+) B", build, re.M)
    ram = re.search(r"^\s*RAM:\s+(\d+) B", build, re.M)
    heap = re.findall(r"C heap: \d+ used, (\d+) peak", run)
    if not (flash and ram and heap):
        sys.exit("footprint_check: FLASH, RAM or the C heap line not found")
    return {"flash": int(flash.group(1)), "ram": int(ram.group(1)),
            "heap_peak": int(heap[-1])}


def main():
    target, build_log, run_log, limits_file = sys.argv[1:5]
    tolerance = float(sys.argv[5]) if len(sys.argv) > 5 else 2.0
    limits = [l for l in json.load(open(limits_file)) if l["target"] == target]
    if not limits:
        sys.exit("footprint_check: no limits for target %s" % target)
    baseline = limits[0]
    now = measure(build_log, run_log)
    now = dict([("target", target)] + list(now.items()))
    names = {"flash": "flash", "ram": "static RAM",
             "heap_peak": "heap peak of two calls"}
    rows = []
    grown = []
    shrunk = []

    for key in ("flash", "ram", "heap_peak"):
        delta = now[key] - baseline[key]
        pct = 100.0 * delta / baseline[key]
        rows.append((key, baseline[key], now[key], delta, pct))
        if pct > tolerance:
            grown.append(key)
        elif pct < -tolerance:
            shrunk.append(key)

    print("%-10s %10s %10s %8s %7s" % ("", "limit", "now", "bytes", "delta"))
    for key, limit, cur, delta, pct in rows:
        print("%-10s %10d %10d %+8d %+6.1f%%%s" % (
              key, limit, cur, delta, pct,
              "  GREW" if key in grown else "  shrank" if key in shrunk else ""))

    entry = json.dumps(now)
    for key, limit, cur, delta, pct in rows:
        if key in grown:
            print("::error::%s: the %s of embedded_ua grew by %d bytes "
                  "(%+.1f%%, from %d to %d), more than the %g%% the footprint "
                  "check allows. If this growth is intended, set the %s entry "
                  "in zephyr/ci-footprint-limits.json to %s"
                  % (target, names[key], delta, pct, limit, cur, tolerance,
                     target, entry))
    for key, limit, cur, delta, pct in rows:
        if key in shrunk:
            print("::notice::%s: the %s of embedded_ua shrank by %d bytes "
                  "(%+.1f%%); lower the %s entry in "
                  "zephyr/ci-footprint-limits.json to %s so that the check "
                  "keeps its meaning" % (target, names[key], -delta, pct,
                                         target, entry))
    print("limits entry for this build:", entry)

    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a") as f:
            f.write("### Footprint of embedded_ua on %s%s\n\n" % (
                    target, ": GREW beyond %g%%" % tolerance if grown else ""))
            f.write("| | limit | now | bytes | delta |\n|---|---:|---:|---:|---:|\n")
            for key, limit, cur, delta, pct in rows:
                f.write("| %s | %d | %d | %+d | %+.1f%% %s|\n" % (
                        names[key], limit, cur, delta, pct,
                        "**grew**" if key in grown else ""))
            if grown:
                f.write("\nIf the growth is intended, set the `%s` entry in "
                        "`zephyr/ci-footprint-limits.json` to:\n\n```json\n%s\n```\n"
                        % (target, entry))
    sys.exit(1 if grown else 0)


if __name__ == "__main__":
    main()
