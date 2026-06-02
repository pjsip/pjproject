#
# Driver for uas-keep-call-on-tsx-fail.xml. pjsua dials SIPp, presses
# 'H' to send a re-INVITE that SIPp rejects with 481. With the
# --keep-call-on-tsx-fail flag, pjsua's on_call_tsx_terminate_session
# returns PJ_TRUE so the library must NOT send BYE; the call must
# stay in CONFIRMED state until pjsua itself hangs up via 'h'.
#
import inc_const as const

PJSUA = ["--null-audio --max-calls=1 --no-tcp "
         "--keep-call-on-tsx-fail sip:127.0.0.1:$SIPP_PORT"]

PJSUA_EXPECTS = [
    # Call connected -- send hold (re-INVITE).
    [0, const.STATE_CONFIRMED, "H"],
    # The suppression log line proves the callback fired and returned
    # PJ_TRUE; only then send the hangup so the BYE is ours, not the
    # library's auto-BYE.
    [0, "suppressing termination after INVITE 481", "h"],
    # Disconnect via our explicit BYE.
    [0, const.STATE_DISCONNECTED, ""]
]

PJSUA_CLI_EXPECTS = [
    [0, const.STATE_CONFIRMED, "H"],
    [0, "suppressing termination after INVITE 481", "call hangup"],
    [0, const.STATE_DISCONNECTED, ""]
]
