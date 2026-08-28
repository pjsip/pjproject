#!/usr/bin/env python3
"""Sum linked bytes per archive from a GNU ld map file."""
import re
import sys
from collections import defaultdict

FLASH = ("text", "rodata", "ARM.exidx", "ARM.extab", "data.rel.ro", "got",
         "init_array", "fini_array", "data")
RAM = ("data.rel.ro", "got", "data", "bss", "init_array", "fini_array")

acc = defaultdict(lambda: [0, 0, 0])  # archive -> [textish, data, bss]

pend_sec = None
lines = open(sys.argv[1], errors="replace").read().splitlines()
i = 0
started = False
discard = False
for i, line in enumerate(lines):
    if line.startswith("Linker script and memory map"):
        started = True
    if not started:
        continue
    if line.startswith("/DISCARD/"):
        discard = True
        continue
    if line and line[0] not in " \t" and (line[0] == "." or line[0].isalpha()):
        discard = False
    if discard:
        continue
    m = re.match(r"^ (\.[\w.$]+)$", line)
    if m:
        pend_sec = m.group(1)
        continue
    m = re.match(r"^ (\.[\w.$]+)?\s+0x[0-9a-f]+\s+(0x[0-9a-f]+)\s+(\S+)$", line)
    if not m:
        pend_sec = None
        continue
    sec = m.group(1) or pend_sec
    pend_sec = None
    if not sec:
        continue
    size = int(m.group(2), 16)
    obj = m.group(3)
    if size == 0 or not obj.startswith("/"):
        continue
    am = re.match(r".*/([^/]+\.a)\(", obj)
    if am:
        arch = am.group(1)
        arch = re.sub(r"-arm-unknown-linux-gnueabihf", "", arch)
    elif obj.endswith(".o"):
        arch = "(app/crt objects)"
    else:
        arch = obj.rsplit("/", 1)[-1]
    # ld reports each merged-string pool once, on its first contributor;
    # bucket those separately instead of misattributing them
    if ".str1." in sec:
        arch = "(string constants, merged)"
    base = sec.lstrip(".").split(".")[0]
    full = sec.lstrip(".")
    if full.startswith("data.rel.ro"):
        acc[arch][1] += size
    elif base in ("text", "rodata") or full.startswith(("ARM.", "gnu.warning")):
        acc[arch][0] += size
    elif base in ("data", "got", "init_array", "fini_array", "tdata"):
        acc[arch][1] += size
    elif base in ("bss", "tbss", "COMMON"):
        acc[arch][2] += size

tot = [0, 0, 0]
print("%-28s %10s %8s %9s" % ("archive", "text+ro", "data", "bss"))
for arch in sorted(acc, key=lambda a: -sum(acc[a][:2])):
    t, d, b = acc[arch]
    tot[0] += t; tot[1] += d; tot[2] += b
    print("%-28s %10d %8d %9d" % (arch, t, d, b))
print("%-28s %10d %8d %9d" % ("TOTAL", *tot))
