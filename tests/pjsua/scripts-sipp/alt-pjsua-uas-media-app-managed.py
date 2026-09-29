#
# Test driver for alt-pjsua-uas-media-app-managed.xml
#
# Same call flow as alt-pjsua-uas-sdp-passthrough.py, but alt_pjsua is
# configured via --id + --acc-media-app-managed (pjsua_acc_config's
# media_app_managed) instead of only --sdp-passthrough. This exercises the
# account-level default: PJSUA_CALL_MEDIA_APP_MANAGED is applied to the
# call as soon as the account is resolved for the incoming INVITE, so no
# media transport is created for the initial offer. --sdp-passthrough also
# enables the account default and avoids initial media transports. --id is
# required so alt_pjsua actually calls pjsua_acc_add() (and thus applies
# media_app_managed) instead of falling back to pjsua_acc_add_local(),
# which does not consult app_config.acc_cfg[].
#
# Run with alt_pjsua:
#   cd tests/pjsua && python3 run.py \
#       --exe ../../pjsip-apps/src/3rdparty_media_sample/alt_pjsua \
#       mod_sipp.py scripts-sipp/alt-pjsua-uas-media-app-managed.xml
#
import inc_const as const

# pjsua instance: UAS, auto-answers with both media lines verbatim. The
# second line uses an intentionally unsupported codec to verify app-managed
# calls do not depend on pjsua's codec registry or media-count policy.
PJSUA = [
    "--null-audio --max-calls=2 --no-tcp "
    "--id sip:pjsua@localhost:$SIPP_PORT "
    "--acc-media-app-managed --sdp-passthrough "
    "--auto-answer=200 "
    "--custom-sdp \"v=0\\r\\no=- 893867432491909 4062736973 IN IP4 10.162.173.125\\r\\ns=-\\r\\nt=0 0\\r\\na=msid-semantic: WMS\\r\\nm=audio 56384 RTP/AVP 116 111\\r\\nc=IN IP4 10.186.99.38\\r\\nb=AS:41\\r\\na=rtpmap:116 AMR-WB/16000\\r\\na=fmtp:116 max-red=0; mode-change-capability=2; mode-change-neighbor=1; mode-change-period=2\\r\\na=rtpmap:111 telephone-event/16000\\r\\na=fmtp:111 0-15\\r\\na=ptime:20\\r\\na=maxptime:40\\r\\na=msid:- 415221c9-141a-4cfa-905a-024af91d0e7c\\r\\na=ssrc:18411299 cname:NNI4WBv5F7m8JUWO\\r\\na=sendrecv\\r\\nm=audio 56386 RTP/AVP 123\\r\\nc=IN IP4 10.186.99.39\\r\\na=rtpmap:123 X-APP-MANAGED/8000\\r\\na=sendrecv\""
]

PJSUA_CLI_EXPECTS = [
    # Verify the real-world offer SIPp sends in its INVITE is received by
    # alt_pjsua unmodified.
    [0, r"m=audio 56832 RTP/AVP 116 107 118 96 111 110", ""],
    [0, r"a=rtpmap:116 AMR-WB/16000/1", ""],
    [0, r"a=fmtp:116 mode-change-capability=2;max-red=220", ""],
    [0, r"m=audio 56834 RTP/AVP 123", ""],
    [0, r"a=rtpmap:123 X-APP-MANAGED/8000", ""],
    # Confirm is_stream_media() leaves negotiated media to the application
    # in pjsua_media_channel_update(). This log does not verify transport
    # initialization; pjsua_call_media_is_app_managed() guards that earlier.
    [0, r"is left to the application", ""],
    [0, const.STATE_CONFIRMED, ""],
]

# No "UDP media transport created"/"RTP socket reachable" log lines should
# ever appear for this call or alt-pjsua-uas-sdp-passthrough.py.
# There is no PJSUA_CLI_EXPECTS negative match helper in this framework,
# so this is verified manually (see the
# session notes / PR description) rather than asserted here.
