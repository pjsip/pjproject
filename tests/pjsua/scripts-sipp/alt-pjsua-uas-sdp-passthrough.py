#
# Test driver for alt-pjsua-uas-sdp-passthrough.xml
#
# alt_pjsua acts as UAS with --sdp-passthrough and --auto-answer=200. SIPp
# sends an INVITE with the real-world SDP offer from real-world-sdp.txt
# (AMR-WB/AMR/telephone-event, 6 payload types). alt_pjsua answers with the
# real-world answer (via --custom-sdp), which only has 2 payload types and
# fmtp params that would normally be reconciled/filtered against the offer
# by pjmedia's SDP negotiator. With PJSUA_CALL_SDP_PASSTHROUGH set, the
# 200 OK body must carry this answer unmodified, and the call should still
# reach STATE_CONFIRMED.
#
# Run with alt_pjsua:
#   cd tests/pjsua && python3 run.py \
#       --exe ../../pjsip-apps/src/3rdparty_media_sample/alt_pjsua \
#       mod_sipp.py scripts-sipp/alt-pjsua-uas-sdp-passthrough.xml
#
import inc_const as const

# pjsua instance: UAS, auto-answers with the real-world answer verbatim.
PJSUA = [
    "--null-audio --max-calls=1 --no-tcp --sdp-passthrough "
    "--auto-answer=200 "
    "--custom-sdp \"v=0\\r\\no=- 893867432491909 4062736973 IN IP4 10.162.173.125\\r\\ns=-\\r\\nt=0 0\\r\\na=msid-semantic: WMS\\r\\nm=audio 56384 RTP/AVP 116 111\\r\\nc=IN IP4 10.186.99.38\\r\\nb=AS:41\\r\\na=rtpmap:116 AMR-WB/16000\\r\\na=fmtp:116 max-red=0; mode-change-capability=2; mode-change-neighbor=1; mode-change-period=2\\r\\na=rtpmap:111 telephone-event/16000\\r\\na=fmtp:111 0-15\\r\\na=ptime:20\\r\\na=maxptime:40\\r\\na=msid:- 415221c9-141a-4cfa-905a-024af91d0e7c\\r\\na=ssrc:18411299 cname:NNI4WBv5F7m8JUWO\\r\\na=sendrecv\""
]

PJSUA_CLI_EXPECTS = [
    # Verify the real-world offer SIPp sends in its INVITE is received by
    # alt_pjsua unmodified.
    [0, r"m=audio 56832 RTP/AVP 116 107 118 96 111 110", ""],
    [0, r"a=rtpmap:116 AMR-WB/16000/1", ""],
    [0, r"a=fmtp:116 mode-change-capability=2;max-red=220", ""],
    [0, const.STATE_CONFIRMED, ""],
]
