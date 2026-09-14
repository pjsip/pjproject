#
# Test driver for alt-pjsua-uas-late-offer.xml
#
# alt_pjsua acts as UAS and auto-answers an SDP-less INVITE from SIPp
# (UAC). alt_pjsua has no offer to answer, so it puts its own SDP offer
# in the 200 OK; SIPp answers it in the ACK. The call should reach
# STATE_CONFIRMED.
# This scenario tests the handling of late SDP offers with dummy codecs.
#
# Run with standard pjsua:
#   cd tests/pjsua && python3 run.py mod_sipp.py \
#       scripts-sipp/alt-pjsua-uas-late-offer.xml
#
# Run with alt_pjsua:
#   cd tests/pjsua && python3 run.py \
#       --exe ../../pjsip-apps/src/3rdparty_media_sample/alt_pjsua \
#       mod_sipp.py scripts-sipp/alt-pjsua-uas-late-offer-dummy-codecs.xml
#
import inc_const as const

# pjsua instance: UAS, auto-answers the SDP-less INVITE with its own offer.
PJSUA = ["--null-audio --max-calls=1 --no-tcp --auto-answer=200 --dummy-codecs"]

PJSUA_CLI_EXPECTS = [
    # Verify the SDP answer SIPp sends in its ACK is received by alt_pjsua.
    # Codec check only (no IP/port) for environment independence.
    [0, r"v=0", ""],
    [0, r"s=-", ""],
    [0, r"t=0 0", ""],
    [0, r"a=rtpmap:0 PCMU/8000", ""],
    [0, r"a=rtpmap:96 H263-1998/90000", ""],
    [0, const.STATE_CONFIRMED, ""],
]
