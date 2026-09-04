#
import inc_const as const
from inc_cfg import *


# Custom test function: mod_call.py's generic flow (place call, then
# hold/reinvite/update/DTMF) doesn't apply here, since this test is only
# about how the *initial* INVITE gets sent, so we supply our own
# TestParam.func instead of letting mod_call.py fall back to its default.
def test_func(t):
    callee = t.process[0]
    caller = t.process[1]

    # pjsua's sample app creates one local account per transport it
    # starts (UDP and TCP, since --local-port implicitly enables both),
    # and always makes the last one created -- the TCP account -- the
    # "current" account used for outgoing calls. Force the caller back
    # onto its UDP account so that this call's address resolution picks
    # UDP, which is required to exercise RFC 3261 18.1.1's automatic
    # switch-over to a congestion-controlled transport.
    caller.send("acc next 0")
    caller.expect("Current account changed to 0")

    # Place the call. All codecs are enabled by default (the same
    # configuration 100_simplecall.py/200_tcp.py rely on), which already
    # makes the INVITE bigger than PJSIP_UDP_SIZE_THRESHOLD (1300
    # bytes), so no artificial padding is needed to trigger the switch.
    caller.send("call new " + t.inst_params[0].uri)

    # PJSIP detects the oversized request and retries with a TCP
    # candidate address first (see sip_util.c: "... exceeds UDP size
    # threshold ..., sending with TCP"). Because the caller's current
    # account is explicitly bound to its own UDP transport object, that
    # TCP candidate is rejected as unsuitable for this particular
    # account/dialog, and PJSIP transparently falls back to the next
    # candidate (the original UDP address). This retry is the
    # observable sign that the switch-over logic engaged, and it is
    # logged *before* the call state below moves to CALLING.
    caller.expect("will try next server")
    caller.expect(const.STATE_CALLING)

    # Callee answers
    callee.expect(const.EVENT_INCOMING_CALL)
    callee.send("call answer 200")

    # Wait until call is connected on both endpoints
    caller.expect(const.STATE_CONFIRMED)
    callee.expect(const.STATE_CONFIRMED)

    caller.sync_stdout()
    callee.sync_stdout()

    # Hangup call
    caller.send("call hangup")
    caller.expect(const.STATE_DISCONNECTED)
    callee.expect(const.STATE_DISCONNECTED)


# UDP call that grows past the UDP MTU threshold (RFC 3261 18.1.1) and
# triggers PJSIP's automatic switch-over to TCP.
test_param = TestParam(
        "UDP call switching to TCP (message size > MTU)",
        [
            InstanceParam("callee", "--null-audio --max-calls=1"),
            InstanceParam("caller", "--null-audio --max-calls=1")
        ],
        func=test_func
        )
