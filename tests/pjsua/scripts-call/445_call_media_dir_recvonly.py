#
import inc_const as const
import inc_util as util
from inc_cfg import *

# Media direction set via pjsua_call_setting.media_dir when answering: on
# the incoming call, the callee runs "call media_dir recvonly" and then
# "call answer 200", i.e. pjsua_call_answer2() with
# PJSUA_CALL_SET_MEDIA_DIR and PJMEDIA_DIR_DECODING, so it answers the
# caller's sendrecv offer with recvonly. The caller then only sends
# (Active), while pjsua reports the callee, which does not send, as Remote
# hold.
#
# The setting persists for subsequent offers: the callee's own unhold
# re-INVITE ("call reinvite" sets PJSUA_CALL_UNHOLD) offers recvonly, and
# the caller answers sendonly.

REMOTE_HOLD = r"Call [0-9]+ media [0-9]+ .*, status is Remote hold"


def test_func(t):
    callee = t.process[0]
    caller = t.process[1]

    caller.send("call new " + t.inst_params[0].uri)
    caller.expect(const.STATE_CALLING)

    util.expect_sdp_attr(callee, "audio", "a=sendrecv", "sendrecv offer")
    callee.expect(const.EVENT_INCOMING_CALL)
    callee.send("call media_dir recvonly")
    callee.expect("Media #0 direction will be recvonly")
    callee.send("call answer 200")
    callee.expect(REMOTE_HOLD)
    util.expect_sdp_attr(callee, "audio", "a=recvonly", "recvonly answer")

    util.expect_sdp_attr(caller, "audio", "a=recvonly", "recvonly answer")
    caller.expect(const.MEDIA_ACTIVE)
    caller.expect(const.STATE_CONFIRMED)
    callee.expect(const.STATE_CONFIRMED)

    caller.sync_stdout()
    callee.sync_stdout()

    callee.send("call reinvite")
    util.expect_sdp_attr(caller, "audio", "a=recvonly", "recvonly offer")
    caller.expect(const.MEDIA_ACTIVE)
    util.expect_sdp_attr(caller, "audio", "a=sendonly", "sendonly answer")
    callee.expect(REMOTE_HOLD)

    caller.sync_stdout()
    callee.sync_stdout()

    caller.send("call hangup")
    callee.expect("BYE sips?:")
    caller.expect(const.STATE_DISCONNECTED)
    callee.expect(const.STATE_DISCONNECTED)


test_param = TestParam(
        "Audio call media direction recvonly on answer",
        [
            InstanceParam("callee", "--null-audio --max-calls=1 --no-tcp"),
            InstanceParam("caller", "--null-audio --max-calls=1 --no-tcp")
        ],
        func=test_func
        )
