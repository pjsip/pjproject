#
import inc_vid as vid
import inc_const as const
import inc_util as util
from inc_cfg import *

# Video call, hold initiated by the caller. Call setup and headless video
# device selection are handled by the shared inc_vid helpers; this test
# only drives the hold and verifies the video stream state.


def test_func(t):
    callee = t.process[0]
    caller = t.process[1]

    # Establish a confirmed video call with an active video stream.
    vid.make_video_call(caller, callee, t.inst_params[0].uri)

    # Caller initiates hold. The video stream goes on hold on both sides,
    # and the states are directional: the caller (initiator) reports Local
    # hold while the callee reports Remote hold. Assert each side's
    # specific state so a regression that swaps them (or reports the same
    # state on both) is caught.
    caller.send("call hold")
    caller.expect(const.VID_MEDIA_LOCAL_HOLD)
    callee.expect(const.VID_MEDIA_REMOTE_HOLD)

    caller.sync_stdout()
    callee.sync_stdout()

    # Hangup.
    caller.send("call hangup")
    caller.expect(const.STATE_DISCONNECTED)
    callee.expect(const.STATE_DISCONNECTED)


test_param = TestParam(
        "Video call hold (initiating)",
        [
            InstanceParam("callee", vid.VIDEO_ARGS),
            InstanceParam("caller", vid.VIDEO_ARGS)
        ],
        func=test_func
        )

# Skip when the build under test has no usable video codec (e.g. built
# without PJMEDIA_HAS_VIDEO, or without a video codec library) -- the call
# can't negotiate a video stream in that case.
if not util.has_video(G_EXE):
    test_param.skip = True
