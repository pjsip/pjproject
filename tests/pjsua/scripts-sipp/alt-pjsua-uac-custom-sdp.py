#
# Test driver for alt-pjsua-uac-custom-sdp.xml
#
# alt_pjsua acts as UAC. It places a call to SIPp (UAS) using a custom SDP
# with audio (PCMU) and video (H263-1998). The call should reach
# STATE_CONFIRMED.
#
# Run with alt_pjsua:
#   cd tests/pjsua && python3 run.py \
#       --exe ../../pjsip-apps/src/3rdparty_media_sample/alt_pjsua \
#       mod_sipp.py scripts-sipp/alt-pjsua-uac-custom-sdp.xml
#
import inc_const as const

# pjsua instance: UAC. No URI in args so the call is initiated via CLI
# after cli_main() has set up the telnet log redirect.
# --rtp-port 4000 pins the audio RTP port; video uses 4002 (next even port).
# PJMEDIA_HAS_VIDEO=1 in config_site.h makes pjsua_call_setting_default set
# vid_cnt=1, so med_prov_cnt=2 (audio+video), matching the custom SDP.
PJSUA = [
    "--null-audio --max-calls=1 --no-tcp --rtp-port 4000 "
    "--custom-sdp \"v=0\\r\\no=- 1234567890 1234567890 IN IP4 127.0.0.1"
    "\\r\\ns=-\\r\\nc=IN IP4 127.0.0.1\\r\\nt=0 0"
    "\\r\\nm=audio 4000 RTP/AVP 0\\r\\na=rtpmap:0 PCMU/8000"
    "\\r\\nm=video 4002 RTP/AVP 96\\r\\na=rtpmap:96 H263-1998/90000\""
]

PJSUA_CLI_EXPECTS = [
    [0, "", "call new $SIPP_URI"],
    # Verify the SDP SIPp sends in its 200 OK is received by alt_pjsua.
    # Codec check only (no IP/port) for environment independence.
    [0, r"v=0", ""],
    [0, r"s=-", ""],
    [0, r"t=0 0", ""],
    [0, r"a=rtpmap:0 PCMU/8000", ""],
    [0, r"a=rtpmap:96 H263-1998/90000", ""],
    [0, const.STATE_CONFIRMED, "call hangup"],
]
