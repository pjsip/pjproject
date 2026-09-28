#
import inc_const as const
from inc_cfg import *

# IP change on the caller while an audio call is in early media: the callee
# answers 183 with SDP, then the caller handles an IP change. An outgoing
# call that is not yet confirmed cannot be refreshed, so pjsua hangs it up
# (CANCEL) and the callee sees the call terminated with 487.


def test_func(t):
    callee = t.process[0]
    caller = t.process[1]

    caller.send("call new " + t.inst_params[0].uri)
    caller.expect(const.STATE_CALLING)

    callee.expect(const.EVENT_INCOMING_CALL)
    callee.send("call answer 183")

    caller.expect(const.STATE_EARLY)
    caller.expect(const.MEDIA_ACTIVE)
    callee.expect(const.MEDIA_ACTIVE)
    callee.expect(const.STATE_EARLY)

    caller.sync_stdout()
    callee.sync_stdout()

    caller.send("ip_change")
    caller.expect("Unconfirmed outgoing call to .*: hangup triggered by "
                  "IP change")
    caller.expect(const.STATE_DISCONNECTED + r" \[reason=410")
    caller.expect("IP change progress report : hangup call")
    caller.expect("IP change progress report : done")

    callee.expect("CANCEL sips?:")
    callee.expect(const.STATE_DISCONNECTED + r" \[reason=487")


test_param = TestParam(
        "Audio call early media, IP change on caller",
        [
            InstanceParam("callee", "--null-audio --max-calls=1 --no-tcp"),
            InstanceParam("caller", "--null-audio --max-calls=1 --no-tcp")
        ],
        func=test_func
        )
