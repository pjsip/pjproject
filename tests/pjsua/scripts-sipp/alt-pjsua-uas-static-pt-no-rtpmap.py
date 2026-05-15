#
# Test driver for alt-pjsua-uas-static-pt-no-rtpmap.xml
#
# alt_pjsua acts as UAS with --auto-answer=200. SIPp sends an INVITE with a
# static payload type (PT 0, PCMU) and NO a=rtpmap attribute, which is valid
# per RFC 4566 but exercises the get_codec_info() fallback path in
# get_audio_codec_info_param(). With an empty codec registry, that call
# returns PJ_ENOTFOUND; the fix treats this as non-fatal so the call can
# proceed. The test verifies STATE_CONFIRMED is reached.
#
# Run with alt_pjsua:
#   cd tests/pjsua && python3 run.py \
#       --exe ../../pjsip-apps/src/3rdparty_media_sample/alt_pjsua \
#       mod_sipp.py scripts-sipp/alt-pjsua-uas-static-pt-no-rtpmap.xml
#
import inc_const as const

# pjsua instance: UAS, auto-answers with a custom SDP that uses PT 0 (PCMU)
# without a=rtpmap so that the get_codec_info() fallback path is exercised.
# --rtp-port 4002 pins the audio RTP port.
PJSUA = [
    "--null-audio --max-calls=1 --no-tcp --rtp-port 4002 "
    "--auto-answer=200 "
    "--custom-sdp \"v=0\\r\\no=- 1234567890 1234567890 IN IP4 127.0.0.1"
    "\\r\\ns=-\\r\\nc=IN IP4 127.0.0.1\\r\\nt=0 0"
    "\\r\\nm=audio 4002 RTP/AVP 0\""
]

PJSUA_CLI_EXPECTS = [
    # The call must reach CONFIRMED state, proving that a static PT without
    # rtpmap is handled gracefully when the codec registry is empty.
    [0, const.STATE_CONFIRMED, ""],
]
