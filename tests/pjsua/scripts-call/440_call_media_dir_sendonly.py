#
import inc_const as const
import inc_util as util
from inc_cfg import *

# Media direction set via pjsua_call_setting.media_dir on the offerer: the
# caller runs "call media_dir sendonly" before "call new", i.e.
# pjsua_call_make_call() with PJSUA_CALL_SET_MEDIA_DIR and
# PJMEDIA_DIR_ENCODING, so its INVITE offers the audio as sendonly and
# the callee answers recvonly. pjsua reports sendonly as Local hold on the
# caller and Remote hold on the callee.
#
# The setting persists for subsequent offers: an unhold re-INVITE
# ("call reinvite" sets PJSUA_CALL_UNHOLD) still offers sendonly instead
# of reverting to sendrecv.

LOCAL_HOLD = r"Call [0-9]+ media [0-9]+ .*, status is Local hold"
REMOTE_HOLD = r"Call [0-9]+ media [0-9]+ .*, status is Remote hold"


def test_func(t):
    callee = t.process[0]
    caller = t.process[1]

    caller.send("call media_dir sendonly")
    caller.expect("Media #0 direction will be sendonly")
    caller.send("call new " + t.inst_params[0].uri)
    caller.expect(const.STATE_CALLING)

    util.expect_sdp_attr(callee, "audio", "a=sendonly", "sendonly offer")
    callee.expect(const.EVENT_INCOMING_CALL)
    callee.send("call answer 200")
    callee.expect(REMOTE_HOLD)
    util.expect_sdp_attr(callee, "audio", "a=recvonly", "recvonly answer")

    caller.expect(LOCAL_HOLD)
    caller.expect(const.STATE_CONFIRMED)
    callee.expect(const.STATE_CONFIRMED)

    caller.sync_stdout()
    callee.sync_stdout()

    caller.send("call reinvite")
    util.expect_sdp_attr(callee, "audio", "a=sendonly", "sendonly offer")
    callee.expect(REMOTE_HOLD)
    util.expect_sdp_attr(callee, "audio", "a=recvonly", "recvonly answer")
    caller.expect(LOCAL_HOLD)

    caller.sync_stdout()
    callee.sync_stdout()

    caller.send("call hangup")
    callee.expect("BYE sips?:")
    caller.expect(const.STATE_DISCONNECTED)
    callee.expect(const.STATE_DISCONNECTED)


test_param = TestParam(
        "Audio call media direction sendonly on new call",
        [
            InstanceParam("callee", "--null-audio --max-calls=1 --no-tcp"),
            InstanceParam("caller", "--null-audio --max-calls=1 --no-tcp")
        ],
        func=test_func
        )
