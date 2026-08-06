#
import inc_vid as vid
import inc_const as const
import inc_util as util
from inc_cfg import *

# Video call UPDATE: send an UPDATE on the established call and verify the
# video stream stays Active (re-negotiated) on both ends.


def test_func(t):
    callee = t.process[0]
    caller = t.process[1]

    vid.make_video_call(caller, callee, t.inst_params[0].uri)

    # Caller sends UPDATE. Assert the outgoing and incoming UPDATE first,
    # so a regression that used re-INVITE instead would be caught, then
    # both endpoints re-report the video stream Active.
    caller.send("call update")
    caller.expect("UPDATE sips?:")
    callee.expect("UPDATE sips?:")
    caller.expect(const.VID_MEDIA_ACTIVE)
    callee.expect(const.VID_MEDIA_ACTIVE)

    caller.sync_stdout()
    callee.sync_stdout()

    vid.hangup_call(caller, callee)


test_param = TestParam(
        "Video call UPDATE",
        [
            InstanceParam("callee", vid.VIDEO_ARGS),
            InstanceParam("caller", vid.VIDEO_ARGS)
        ],
        func=test_func
        )

if not util.has_video(G_EXE):
    test_param.skip = True
