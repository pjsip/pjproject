#
import inc_vid as vid
import inc_const as const
import inc_util as util
from inc_cfg import *

# Video call re-INVITE: put the call on hold, then resume it with a
# re-INVITE and verify the video stream returns to Active on both ends.


def test_func(t):
    callee = t.process[0]
    caller = t.process[1]

    vid.make_video_call(caller, callee, t.inst_params[0].uri)

    # Hold, so the resume re-INVITE has an observable Active transition.
    caller.send("call hold")
    caller.expect(const.VID_MEDIA_LOCAL_HOLD)
    callee.expect(const.VID_MEDIA_REMOTE_HOLD)

    caller.sync_stdout()
    callee.sync_stdout()

    # Resume via re-INVITE. Assert the outgoing and incoming INVITE first,
    # so a regression that resumed via UPDATE instead would be caught, then
    # the video stream goes back to Active on both.
    caller.send("call reinvite")
    caller.expect("INVITE sips?:")
    callee.expect("INVITE sips?:")
    caller.expect(const.VID_MEDIA_ACTIVE)
    callee.expect(const.VID_MEDIA_ACTIVE)

    caller.sync_stdout()
    callee.sync_stdout()

    vid.hangup_call(caller, callee)


test_param = TestParam(
        "Video call re-INVITE (resume from hold)",
        [
            InstanceParam("callee", vid.VIDEO_ARGS),
            InstanceParam("caller", vid.VIDEO_ARGS)
        ],
        func=test_func
        )

if not util.has_video(G_EXE):
    test_param.skip = True
