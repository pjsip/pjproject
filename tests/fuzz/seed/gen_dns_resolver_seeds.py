#!/usr/bin/env python3
# Copyright (C) 2026 Teluu Inc. (http://www.teluu.com)
# GPLv2+ (same license as the project).
#
# Generates the seed corpus for fuzz-dns-resolver: structurally valid DNS
# replies for the fixed query name used by the harness, covering A, SRV,
# CNAME chasing, NXDOMAIN, SERVFAIL, TC-bit (TCP requery), truncated
# garbage, plus the control-byte variants (join, cache, cancel).
#
# Usage: gen_dns_resolver_seeds.py <output-zip-path>

import struct
import sys
import zipfile

QNAME = b"\x04fuzz\x07example\x03com\x00"
QTYPE_A = 1
QTYPE_SRV = 33


def header(flags, qd, an):
    return struct.pack(">HHHHHH", 0x0000, flags, qd, an, 0, 0)


def question(qtype):
    return QNAME + struct.pack(">HH", qtype, 1)


def a_rr():
    # name (compressed ptr to offset 12), A, IN, ttl 60, rdlen 4, 127.0.0.1
    return struct.pack(">HHHIH", 0xC00C, QTYPE_A, 1, 60, 4) + bytes(
        [127, 0, 0, 1])


def srv_rr():
    # SRV rdata: priority, weight, port, target
    target = b"\x03sip\x07example\x03com\x00"
    return (struct.pack(">HHHIH", 0xC00C, QTYPE_SRV, 1, 60, 6 + len(target))
            + struct.pack(">HHH", 0, 5, 5060) + target)


def cname_rr():
    target = b"\x05other\x07example\x03com\x00"
    return (struct.pack(">HHHIH", 0xC00C, 5, 1, 60, len(target)) + target)


SEEDS = [
    # (ctl byte, payload description)
    (0x00, header(0x8180, 1, 1) + question(QTYPE_A) + a_rr()),      # A ok
    (0x01, header(0x8180, 1, 1) + question(QTYPE_SRV) + srv_rr()),  # SRV ok
    (0x00, header(0x8180, 1, 1) + question(QTYPE_A) + cname_rr()),  # CNAME
    (0x00, header(0x8183, 1, 0) + question(QTYPE_A)),               # NXDOMAIN
    (0x00, header(0x8182, 1, 0) + question(QTYPE_A)),               # SERVFAIL
    (0x00, header(0xA180, 1, 0) + question(QTYPE_A)),               # TC bit
    (0x04, header(0x8180, 1, 1) + question(QTYPE_A) + a_rr()),      # cache
    (0x08, header(0x8180, 1, 1) + question(QTYPE_A) + a_rr()),      # cancel
    (0x02, header(0x8180, 1, 1) + question(QTYPE_A) + a_rr()),      # join
    (0x00, header(0x8180, 1, 1)[:9]),                               # truncated
    (0x00, header(0x8180, 1, 0) + question(QTYPE_A)),               # no answer
]


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 1
    out = sys.argv[1]
    with zipfile.ZipFile(out, "w", zipfile.ZIP_STORED) as zf:
        for i, (ctl, payload) in enumerate(SEEDS):
            data = bytes([ctl]) + payload
            zf.writestr("%02d" % (i + 1), data)
    print("wrote %d seeds to %s" % (len(SEEDS), out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
