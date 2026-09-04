#
import inc_vid as vid
import inc_const as const
import inc_util as util
from inc_cfg import *

# Video call where the callee requires 100rel: the callee answers with a
# reliable 180, so the caller must PRACK it before the call is answered
# with 200. Verifies PRACK works on a video call.


def test_func(t):
    callee = t.process[0]
    caller = t.process[1]

    vid.setup_video_devices(caller)
    vid.setup_video_devices(callee)

    caller.send("call new " + t.inst_params[0].uri)
    caller.expect(const.STATE_CALLING)

    # Callee answers with a reliable 180 (100rel); the caller PRACKs it.
    callee.expect(const.EVENT_INCOMING_CALL)
    callee.send("call answer 180")
    caller.expect("SIP/2.0 180")
    callee.expect("PRACK sips?:")

    caller.sync_stdout()
    callee.sync_stdout()

    # Now answer with 200; the video stream negotiates Active on both ends.
    callee.send("call answer 200")
    caller.expect(const.VID_MEDIA_ACTIVE)
    callee.expect(const.VID_MEDIA_ACTIVE)
    caller.expect(const.STATE_CONFIRMED)
    callee.expect(const.STATE_CONFIRMED)

    caller.sync_stdout()
    callee.sync_stdout()

    vid.hangup_call(caller, callee)


test_param = TestParam(
        "Video call 100rel/PRACK (callee requires)",
        [
            InstanceParam("callee", vid.VIDEO_ARGS + " --use-100rel"),
            InstanceParam("caller", vid.VIDEO_ARGS)
        ],
        func=test_func
        )

if not util.has_video(G_EXE):
    test_param.skip = True
