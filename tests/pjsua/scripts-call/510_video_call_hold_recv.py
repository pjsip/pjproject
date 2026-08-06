#
import inc_vid as vid
import inc_const as const
import inc_util as util
from inc_cfg import *

# Video call, hold received by the caller (the callee initiates it). The
# mirror of 500_video_call_hold.py, which tests the initiating side.


def test_func(t):
    callee = t.process[0]
    caller = t.process[1]

    vid.make_video_call(caller, callee, t.inst_params[0].uri)

    # Callee initiates hold; the caller receives it. The callee (initiator)
    # reports Local hold on the video stream, the caller Remote hold.
    callee.send("call hold")
    callee.expect(const.VID_MEDIA_LOCAL_HOLD)
    caller.expect(const.VID_MEDIA_REMOTE_HOLD)

    caller.sync_stdout()
    callee.sync_stdout()

    vid.hangup_call(caller, callee)


test_param = TestParam(
        "Video call hold (receiving)",
        [
            InstanceParam("callee", vid.VIDEO_ARGS),
            InstanceParam("caller", vid.VIDEO_ARGS)
        ],
        func=test_func
        )

if not util.has_video(G_EXE):
    test_param.skip = True
