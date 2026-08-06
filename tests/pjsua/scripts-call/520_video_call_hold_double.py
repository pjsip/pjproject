#
import inc_vid as vid
import inc_const as const
import inc_util as util
from inc_cfg import *

# Video call double hold: the caller holds, then the callee also holds
# while already held, so both sides have the video stream on hold.


def test_func(t):
    callee = t.process[0]
    caller = t.process[1]

    vid.make_video_call(caller, callee, t.inst_params[0].uri)

    # Caller holds first.
    caller.send("call hold")
    caller.expect(const.VID_MEDIA_LOCAL_HOLD)
    callee.expect(const.VID_MEDIA_REMOTE_HOLD)

    caller.sync_stdout()
    callee.sync_stdout()

    # Callee also holds (double hold): it now has both a local hold and the
    # caller's remote hold, so it reports Local hold.
    callee.send("call hold")
    callee.expect(const.VID_MEDIA_LOCAL_HOLD)
    caller.expect(const.VID_MEDIA_HOLD)

    caller.sync_stdout()
    callee.sync_stdout()

    vid.hangup_call(caller, callee)


test_param = TestParam(
        "Video call double hold",
        [
            InstanceParam("callee", vid.VIDEO_ARGS),
            InstanceParam("caller", vid.VIDEO_ARGS)
        ],
        func=test_func
        )

if not util.has_video(G_EXE):
    test_param.skip = True
