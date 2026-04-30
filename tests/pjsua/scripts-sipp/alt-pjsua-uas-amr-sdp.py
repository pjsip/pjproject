#
# Test driver for alt-pjsua-uas-amr-sdp.xml
#
# alt_pjsua acts as UAS with --auto-answer=200. SIPp sends an INVITE with a
# dynamic-PT AMR-WB (PT 116) + AMR (PT 96) SDP offer.  alt_pjsua answers with
# its custom AMR-WB SDP. The call must reach STATE_CONFIRMED, which proves the
# fix for the dynamic-PT code path in get_audio_codec_info_param():
# pjmedia_codec_mgr_find_codecs_by_id() returning PJ_ENOTFOUND for unregistered
# codecs no longer causes a hard failure.
#
# Run with alt_pjsua:
#   cd tests/pjsua && python3 run.py \
#       --exe ../../pjsip-apps/src/3rdparty_media_sample/alt_pjsua \
#       mod_sipp.py scripts-sipp/alt-pjsua-uas-amr-sdp.xml
#
import inc_const as const

# pjsua instance: UAS, auto-answers with a custom SDP that uses AMR-WB
# (PT 116, dynamic) so that the dynamic-PT code path in stream_info.c is
# exercised.  --rtp-port 4002 pins the audio RTP port.
PJSUA = [
    "--null-audio --max-calls=1 --no-tcp --rtp-port 4002 "
    "--auto-answer=200 "
    "--custom-sdp \"v=0\\r\\no=- 1234567890 1234567890 IN IP4 127.0.0.1"
    "\\r\\ns=-\\r\\nc=IN IP4 127.0.0.1\\r\\nt=0 0"
    "\\r\\nm=audio 4002 RTP/AVP 116"
    "\\r\\na=rtpmap:116 AMR-WB/16000/1\""
]

PJSUA_CLI_EXPECTS = [
    # Verify the AMR-WB rtpmap from SIPp's INVITE is received.
    [0, r"a=rtpmap:116 AMR-WB/16000", ""],
    # The call must reach CONFIRMED state, proving dynamic-PT negotiation works.
    [0, const.STATE_CONFIRMED, ""],
]
