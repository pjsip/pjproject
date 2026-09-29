#
# Test driver for alt-pjsua-uas-multiple-amr.xml
#
# alt_pjsua acts as UAS with --sdp-passthrough and --auto-answer=200. SIPp
# sends an INVITE with a real-world SDP offer advertising multiple AMR
# variants (AMR-WB PT 116 and PT 107, AMR PT 118 and PT 96 -- same rtpmap,
# different fmtp octet-align --, G.722 PT 9, and two telephone-event
# payloads PT 111 and PT 110). alt_pjsua answers with a custom SDP (via
# --custom-sdp) that picks PT 118 only. With PJSUA_CALL_MEDIA_APP_MANAGED
# set, the 200 OK body must carry this answer unmodified -- pjmedia's SDP
# negotiator must not reconcile/merge/reject it against the offer's
# duplicate-rtpmap payload types, which is the original problem this test
# exists for (see alt-pjsua-uas-amr-sdp for the plain, non-duplicate case).
#
# Once the call is confirmed, alt_pjsua is driven via its CLI to send a
# re-INVITE ("call reinvite") and UPDATE ("call update") without supplying
# another SDP. App-managed calls must reuse their active local SDP instead
# of generating PJSUA's all-port-zero placeholder. SIPp verifies both
# request bodies still carry PT 118 unmodified before answering them.
#
# Run with alt_pjsua:
#   cd tests/pjsua && python3 run.py \
#       --exe ../../pjsip-apps/src/3rdparty_media_sample/alt_pjsua \
#       mod_sipp.py scripts-sipp/alt-pjsua-uas-multiple-amr.xml
#
import inc_const as const

# pjsua instance: UAS, auto-answers with a custom SDP that selects AMR
# (PT 118, octet-align=0) out of the offer's duplicate-rtpmap PT 118/PT 96.
PJSUA = [
    "--null-audio --max-calls=1 --no-tcp --sdp-passthrough "
    "--auto-answer=200 "
    "--custom-sdp \"v=0\\r\\no=- 3832217418 3832217418 IN IP4 10.162.173.125"
    "\\r\\ns=-"
    "\\r\\nc=IN IP4 10.186.99.39"
    "\\r\\nt=0 0"
    "\\r\\nm=audio 4002 RTP/AVP 118 110"
    "\\r\\nb=AS:29"
    "\\r\\na=rtpmap:118 AMR/8000"
    "\\r\\na=fmtp:118 octet-align=0;mode-change-capability=2;max-red=0"
    "\\r\\na=rtpmap:110 telephone-event/8000"
    "\\r\\na=fmtp:110 0-15"
    "\\r\\na=rtcp:4003 IN IP4 10.186.99.39"
    "\\r\\na=sendrecv"
    "\\r\\na=ptime:20"
    "\\r\\na=maxptime:40\""
]

PJSUA_CLI_EXPECTS = [
    # Verify the multiple AMR/AMR-WB rtpmaps from SIPp's INVITE are received
    # unmodified (passthrough must not touch the incoming offer either).
    [0, r"a=rtpmap:116 AMR-WB/16000", ""],
    [0, r"a=rtpmap:118 AMR/8000", ""],
    [0, r"a=fmtp:96 octet-align=1", ""],
    # The call must reach CONFIRMED state. Once it does, drive a re-INVITE
    # from alt_pjsua's side via its CLI ("call reinvite", sc='v').
    [0, const.STATE_CONFIRMED, "call reinvite"],
    [0, r"is left to the application", ""],
    # SIPp sends OPTIONS after the re-INVITE ACK, ensuring that transaction
    # is complete before the UPDATE is started.
    [0, r"Request msg OPTIONS", "call update"],
]
