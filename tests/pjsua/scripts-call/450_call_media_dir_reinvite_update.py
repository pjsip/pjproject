#
import inc_const as const
import inc_util as util
from inc_cfg import *

# Media direction changed mid-call via pjsua_call_setting.media_dir in
# pjsua_call_reinvite2() and pjsua_call_update2(): after a sendrecv call
# is established, the caller sets the direction with "call media_dir" and
# then sends
#   - a re-INVITE offering sendonly (callee answers recvonly),
#   - an UPDATE offering recvonly (callee answers sendonly),
#   - a re-INVITE offering sendrecv again (callee answers sendrecv).
# Each offer is checked as received by the callee, along with its answer
# and the resulting media state on both ends.

LOCAL_HOLD = r"Call [0-9]+ media [0-9]+ .*, status is Local hold"
REMOTE_HOLD = r"Call [0-9]+ media [0-9]+ .*, status is Remote hold"


def change_dir(caller, callee, cmd, method, offer, answer,
               caller_state, callee_state):
    caller.send("call media_dir " + offer)
    caller.expect("Media #0 direction set to " + offer)
    caller.send("call " + cmd)

    callee.expect(method + " sips?:")
    util.expect_sdp_attr(callee, "audio", "a=" + offer, offer + " offer")
    callee.expect(callee_state)
    util.expect_sdp_attr(callee, "audio", "a=" + answer, answer + " answer")
    caller.expect(caller_state)

    caller.sync_stdout()
    callee.sync_stdout()


def test_func(t):
    callee = t.process[0]
    caller = t.process[1]

    caller.send("call new " + t.inst_params[0].uri)
    caller.expect(const.STATE_CALLING)
    callee.expect(const.EVENT_INCOMING_CALL)
    callee.send("call answer 200")
    caller.expect(const.MEDIA_ACTIVE)
    callee.expect(const.MEDIA_ACTIVE)
    caller.expect(const.STATE_CONFIRMED)
    callee.expect(const.STATE_CONFIRMED)

    caller.sync_stdout()
    callee.sync_stdout()

    change_dir(caller, callee, "reinvite", "INVITE", "sendonly", "recvonly",
               LOCAL_HOLD, REMOTE_HOLD)
    change_dir(caller, callee, "update", "UPDATE", "recvonly", "sendonly",
               REMOTE_HOLD, const.MEDIA_ACTIVE)
    change_dir(caller, callee, "reinvite", "INVITE", "sendrecv", "sendrecv",
               const.MEDIA_ACTIVE, const.MEDIA_ACTIVE)

    caller.send("call hangup")
    callee.expect("BYE sips?:")
    caller.expect(const.STATE_DISCONNECTED)
    callee.expect(const.STATE_DISCONNECTED)


test_param = TestParam(
        "Audio call media direction change in re-INVITE/UPDATE",
        [
            InstanceParam("callee", "--null-audio --max-calls=1 --no-tcp"),
            InstanceParam("caller", "--null-audio --max-calls=1 --no-tcp")
        ],
        func=test_func
        )
