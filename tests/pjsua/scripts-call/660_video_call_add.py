#
import inc_vid as vid
import inc_const as const
import inc_util as util
from inc_cfg import *

# Add video on the fly: establish an audio-only call, then add a video
# stream to it with "video call add" and verify the resulting re-INVITE
# negotiates an active video stream on both ends.


def test_func(t):
    callee = t.process[0]
    caller = t.process[1]

    # Both instances still run with --video (so the callee accepts the
    # video offer later on), and both are pinned to headless video
    # devices before any video stream exists.
    vid.setup_video_devices(caller)
    vid.setup_video_devices(callee)

    # Start audio only. "video disable" clears vid_cnt in the call
    # setting that the next "call new" derives its offer from, so the
    # initial INVITE carries no m=video line -- that is what makes the
    # later add an actual on-the-fly addition rather than a no-op.
    caller.send("video disable")
    caller.sync_stdout()

    caller.send("call new " + t.inst_params[0].uri)
    caller.expect(const.STATE_CALLING)

    # Verify the starting point really is an audio-only call, rather than
    # assuming "video disable" worked: the callee summarises the offered
    # media when it reports the incoming call, and it must count no video
    # stream. Without this the test would still pass if the initial
    # INVITE already carried video -- "video call add" would then simply
    # add a second video stream and the checks below would not notice.
    # This line precedes the answer prompt in the same log statement, so
    # it has to be expected before EVENT_INCOMING_CALL.
    callee.expect("Media count: 1 audio & 0 video")
    callee.expect(const.EVENT_INCOMING_CALL)
    callee.send("call answer 200")

    caller.expect(const.STATE_CONFIRMED)
    callee.expect(const.STATE_CONFIRMED)

    caller.sync_stdout()
    callee.sync_stdout()

    # Add the video stream. Assert the callee received an offer with an
    # enabled m=video line (non-zero port) first -- that proves the add
    # went out on the wire -- then that video is Active on both ends.
    caller.send("video call add")
    callee.expect("m=video [1-9]")
    caller.expect(const.VID_MEDIA_ACTIVE)
    callee.expect(const.VID_MEDIA_ACTIVE)

    caller.sync_stdout()
    callee.sync_stdout()

    vid.hangup_call(caller, callee)


test_param = TestParam(
        "Video call add video on the fly",
        [
            InstanceParam("callee", vid.VIDEO_ARGS),
            InstanceParam("caller", vid.VIDEO_ARGS)
        ],
        func=test_func
        )

if not util.has_video(G_EXE):
    test_param.skip = True
