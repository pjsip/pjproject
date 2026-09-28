#
import inc_const as const
from inc_cfg import *

# IP change on the callee while an audio call is in early media: the callee
# answers 183 with SDP, then handles an IP change. An incoming call in the
# EARLY state is neither hung up (hangup_calls is off by default) nor
# refreshed, so it must survive; the callee then answers 200 and the call
# is confirmed with audio still active on both ends.


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

    callee.send("ip_change")
    callee.expect("IP change progress report : done")

    callee.sync_stdout()
    caller.sync_stdout()

    callee.send("call answer 200")
    caller.expect(const.STATE_CONFIRMED)
    callee.expect(const.STATE_CONFIRMED)

    caller.sync_stdout()
    callee.sync_stdout()

    caller.send("call hangup")
    callee.expect("BYE sips?:")
    caller.expect(const.STATE_DISCONNECTED)
    callee.expect(const.STATE_DISCONNECTED)


test_param = TestParam(
        "Audio call early media, IP change on callee",
        [
            InstanceParam("callee", "--null-audio --max-calls=1 --no-tcp"),
            InstanceParam("caller", "--null-audio --max-calls=1 --no-tcp")
        ],
        func=test_func
        )
