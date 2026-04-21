#
# Test driver for alt-pjsua-uas-custom-sdp.xml
#
# alt_pjsua (or pjsua) acts as UAS with --auto-answer=200. SIPp sends an
# INVITE with a PCMU offer. pjsua answers with its custom SDP. The call
# should reach STATE_CONFIRMED before SIPp terminates it with BYE.
#
# Run with standard pjsua:
#   cd tests/pjsua && python3 run.py mod_sipp.py \
#       scripts-sipp/alt-pjsua-uas-custom-sdp.xml
#
# Run with alt_pjsua:
#   cd tests/pjsua && python3 run.py \
#       --exe ../../pjsip-apps/src/3rdparty_media_sample/alt_pjsua \
#       mod_sipp.py scripts-sipp/alt-pjsua-uas-custom-sdp.xml
#
import inc_const as const

# pjsua instance: UAS, auto-answers with custom SDP.
# --rtp-port 4002 pins the audio RTP port; video uses 4004 (next even port).
PJSUA = [
    "--null-audio --max-calls=1 --no-tcp --rtp-port 4002 "
    "--auto-answer=200 "
    "--custom-sdp \"v=0\\r\\no=- 1234567890 1234567890 IN IP4 127.0.0.1"
    "\\r\\ns=-\\r\\nc=IN IP4 127.0.0.1\\r\\nt=0 0"
    "\\r\\nm=audio 4002 RTP/AVP 0\\r\\na=rtpmap:0 PCMU/8000"
    "\\r\\nm=video 4004 RTP/AVP 96\\r\\na=rtpmap:96 H263-1998/90000\""
]

PJSUA_CLI_EXPECTS = [
    # Verify the full SDP SIPp sends in its INVITE is received by alt_pjsua.
    # Codec check only (no IP/port) for environment independence.
    [0, r"v=0", ""],
    [0, r"s=-", ""],
    [0, r"t=0 0", ""],
    [0, r"a=rtpmap:0 PCMU/8000", ""],
    [0, r"a=rtpmap:96 H263-1998/90000", ""],
    [0, const.STATE_CONFIRMED, ""],
]
