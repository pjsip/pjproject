#!/usr/bin/env python3
# Copyright (C) 2026 Teluu Inc. (http://www.teluu.com)
# GPLv2+ (same license as the project).
#
# Generates the seed corpus for fuzz-sip-inv: structurally valid call
# scenarios (control byte + URIs + SDP offer) covering the main INVITE
# usage features (200/183 answers, 100rel, session timer, UPDATE,
# re-INVITE, offerless INVITE, and a truncated SDP).
#
# Usage: gen_sip_inv_seeds.py <output-zip-path>

import sys
import zipfile

URI = b"sip:bob@example.com"
TARGET = b"sip:bob@example.com;transport=udp"

SDP_FULL = (
    b"v=0\r\n"
    b"o=- 3414953978 3414953978 IN IP4 127.0.0.1\r\n"
    b"s=inv-fuzz-seed\r\n"
    b"c=IN IP4 127.0.0.1\r\n"
    b"t=0 0\r\n"
    b"m=audio 4000 RTP/AVP 0 8\r\n"
    b"a=rtpmap:0 PCMU/8000\r\n"
    b"a=rtpmap:8 PCMA/8000\r\n"
)

SDP_TRUNCATED = (
    b"v=0\r\n"
    b"o=- 3414953978 3414953978 IN IP4 127.0.0.1\r\n"
    b"s=inv-fuzz-seed\r\n"
    b"c=IN IP4 127.0.0.1\r\n"
    b"t=0 0\r\n"
    b"m=audio 4000 RTP/AVP"
)

# Control byte bits:
#   0x01 SUPPORT_100REL   0x02 REQUIRE_100REL
#   0x04 TIMER+ALWAYS_USE 0x08 SUPPORT_UPDATE
#   bits 4-6 answer code index {200, 183, 486, 488, 500}
#   0x80 mid-call re-INVITE/UPDATE
SEEDS = [
    (0x00, SDP_FULL),           # plain call, 200 answer
    (0x01 | 0x10, SDP_FULL),    # 100rel support, 183 then 200
    (0x03 | 0x10, SDP_FULL),    # 100rel required, 183 then 200
    (0x04, SDP_FULL),           # session timer always
    (0x08, SDP_FULL),           # UPDATE support
    (0x80, SDP_FULL),           # re-INVITE after confirm
    (0x88, SDP_FULL),           # UPDATE after confirm
    (0x20, SDP_FULL),           # 486 Busy Here
    (0x30, SDP_FULL),           # 488 Not Acceptable Here
    (0x40, SDP_FULL),           # 500
    (0x80, b""),                # offerless INVITE + re-INVITE
    (0x00, SDP_TRUNCATED),      # malformed-ish SDP offer
]


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 1
    out = sys.argv[1]
    with zipfile.ZipFile(out, "w", zipfile.ZIP_STORED) as zf:
        for i, (ctl, sdp) in enumerate(SEEDS):
            data = bytes([ctl]) + URI + b"\x00" + TARGET + b"\x00" + sdp
            zf.writestr("%02d" % (i + 1), data)
    print("wrote %d seeds to %s" % (len(SEEDS), out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
