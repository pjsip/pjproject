#
import inc_const as const
from inc_cfg import *


# DTMF digits to send, each carried in its own SIP INFO request.
DTMF_DIGITS = "1234"


# Send DTMF via SIP INFO from 'sender', one digit at a time, verifying that
# 'receiver' logs each digit (and that SIP INFO -- not RFC 2833 -- carried
# it) before sending the next. This mirrors mod_call.py's check_media() --
# which sends DTMF via the '#' command (RFC 2833) -- but drives the '*'
# command / PJSUA_DTMF_METHOD_SIP_INFO path instead, the one call flow with
# no other test coverage. We send digit-by-digit with event-driven sync
# (rather than firing all digits at once) both because that is how DTMF is
# used in practice and to keep the test robust on slow CI runners.
def send_dtmf_info(sender, receiver):
    for digit in DTMF_DIGITS:
        if sender.use_telnet:
            sender.send("* " + digit)
        else:
            sender.send("*")
            sender.expect("DTMF")
            sender.send(digit)
        receiver.expect(const.RX_DTMF + digit + const.RX_DTMF_INFO_METHOD)
        sender.sync_stdout()
        receiver.sync_stdout()


# Custom test function: mod_call.py's generic flow (hold/reinvite/update and
# RFC 2833 DTMF) doesn't cover SIP INFO DTMF, so we supply our own.
def test_func(t):
    callee = t.process[0]
    caller = t.process[1]

    # Caller places the call.
    caller.send("call new " + t.inst_params[0].uri)
    caller.expect(const.STATE_CALLING)

    # Callee answers with 200/OK.
    callee.expect(const.EVENT_INCOMING_CALL)
    callee.send("call answer 200")

    # Wait until the call is connected on both endpoints.
    caller.expect(const.STATE_CONFIRMED)
    callee.expect(const.STATE_CONFIRMED)

    caller.sync_stdout()
    callee.sync_stdout()

    # Caller -> callee DTMF over SIP INFO.
    send_dtmf_info(caller, callee)

    caller.sync_stdout()
    callee.sync_stdout()

    # Callee -> caller DTMF over SIP INFO (exercise the other direction).
    send_dtmf_info(callee, caller)

    caller.sync_stdout()
    callee.sync_stdout()

    # Hangup call.
    caller.send("call hangup")
    caller.expect(const.STATE_DISCONNECTED)
    callee.expect(const.STATE_DISCONNECTED)


# Basic call exercising DTMF transmission over SIP INFO
# (PJSUA_DTMF_METHOD_SIP_INFO), in both directions.
test_param = TestParam(
        "DTMF via SIP INFO",
        [
            InstanceParam("callee", "--null-audio --max-calls=1"),
            InstanceParam("caller", "--null-audio --max-calls=1")
        ],
        func=test_func
        )
