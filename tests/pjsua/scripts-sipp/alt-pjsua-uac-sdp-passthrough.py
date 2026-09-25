#
# Test driver for alt-pjsua-uac-sdp-passthrough.xml
#
# alt_pjsua acts as UAC with --sdp-passthrough. It places a call to SIPp
# (UAS) using a real-world SDP offer (see real-world-sdp.txt: AMR-WB/AMR/
# telephone-event, 6 payload types). SIPp answers with the real-world
# answer verbatim (only 2 payload types, with fmtp params that differ from
# the offer's). With PJSUA_CALL_SDP_PASSTHROUGH set, pjmedia's SDP
# negotiator must not reconcile/rewrite this answer against the offer: the
# 200 OK body received by alt_pjsua must show up unmodified in its log, and
# the call should still reach STATE_CONFIRMED.
#
# Run with alt_pjsua:
#   cd tests/pjsua && python3 run.py \
#       --exe ../../pjsip-apps/src/3rdparty_media_sample/alt_pjsua \
#       mod_sipp.py scripts-sipp/alt-pjsua-uac-sdp-passthrough.xml
#
import inc_const as const

# pjsua instance: UAC. No URI in args so the call is initiated via CLI
# after cli_main() has set up the telnet log redirect.
PJSUA = [
    "--null-audio --max-calls=1 --no-tcp --sdp-passthrough "
    "--custom-sdp \"v=0\\r\\no=- 893867431174198 4062473431 IN IP4 10.162.173.125\\r\\ns=SS VOIP\\r\\nc=IN IP4 10.186.99.36\\r\\nt=0 0\\r\\nm=audio 56832 RTP/AVP 116 107 118 96 111 110\\r\\nb=AS:41\\r\\nb=RS:512\\r\\nb=RR:1537\\r\\na=rtpmap:116 AMR-WB/16000/1\\r\\na=fmtp:116 mode-change-capability=2;max-red=220\\r\\na=rtpmap:107 AMR-WB/16000/1\\r\\na=fmtp:107 octet-align=1;mode-change-capability=2;max-red=220\\r\\na=rtpmap:118 AMR/8000/1\\r\\na=fmtp:118 mode-change-capability=2;max-red=220\\r\\na=rtpmap:96 AMR/8000/1\\r\\na=fmtp:96 octet-align=1;mode-change-capability=2;max-red=220\\r\\na=rtpmap:111 telephone-event/16000\\r\\na=fmtp:111 0-15\\r\\na=rtpmap:110 telephone-event/8000\\r\\na=fmtp:110 0-15\\r\\na=curr:qos local none\\r\\na=curr:qos remote none\\r\\na=des:qos mandatory local sendrecv\\r\\na=des:qos optional remote sendrecv\\r\\na=sendrecv\\r\\na=ptime:20\\r\\na=maxptime:240\""
]

PJSUA_CLI_EXPECTS = [
    [0, "", "call new $SIPP_URI"],
    # Verify the real-world answer SIPp sends in its 200 OK is received by
    # alt_pjsua unmodified: the negotiated fmtp params and codec set stay
    # exactly as SIPp sent them (only 116/111, fmtp diverging from the
    # offer), instead of being reconciled against the 6-codec offer.
    [0, r"m=audio 56384 RTP/AVP 116 111", ""],
    [0, r"a=rtpmap:116 AMR-WB/16000", ""],
    [0, r"a=fmtp:116 max-red=0; mode-change-capability=2; "
        r"mode-change-neighbor=1; mode-change-period=2", ""],
    [0, r"a=rtpmap:111 telephone-event/16000", ""],
    [0, const.STATE_CONFIRMED, "call hangup"],
]
