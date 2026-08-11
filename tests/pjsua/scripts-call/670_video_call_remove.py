#
import inc_vid as vid
import inc_const as const
import inc_util as util
from inc_cfg import *

# Remove video on the fly: establish a video call, then drop the video
# stream with "video call disable" and verify the re-INVITE deactivates
# the m=video line and both ends report the video stream as gone. The
# audio stream must survive -- the call stays up.


def test_func(t):
    callee = t.process[0]
    caller = t.process[1]

    vid.make_video_call(caller, callee, t.inst_params[0].uri)

    # Remove stream #1 (media 0 is audio, media 1 is the video stream
    # negotiated by --video). Removal is signalled by deactivating the
    # media line, i.e. re-offering it with port 0, so assert "m=video 0"
    # on the callee rather than just any m=video line.
    caller.send("video call disable 1")
    callee.expect("m=video 0")

    # Only video goes away. Each endpoint re-reports its media in index
    # order after the renegotiation, so audio is asserted first: still
    # Active, having been carried through untouched. Without this a
    # removal that tore down audio as well would go unnoticed -- the BYE
    # below would still succeed, since that only says the dialog is up.
    caller.expect(const.AUD_MEDIA_ACTIVE)
    callee.expect(const.AUD_MEDIA_ACTIVE)

    # The video media slot itself survives the removal, reported as None.
    caller.expect(const.VID_MEDIA_NONE)
    callee.expect(const.VID_MEDIA_NONE)

    caller.sync_stdout()
    callee.sync_stdout()

    # And the dialog is unaffected: it is still up and hangs up via BYE.
    vid.hangup_call(caller, callee)


test_param = TestParam(
        "Video call remove video on the fly",
        [
            InstanceParam("callee", vid.VIDEO_ARGS),
            InstanceParam("caller", vid.VIDEO_ARGS)
        ],
        func=test_func
        )

if not util.has_video(G_EXE):
    test_param.skip = True
