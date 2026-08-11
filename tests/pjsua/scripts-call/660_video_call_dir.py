#
import inc_vid as vid
import inc_const as const
import inc_util as util
from inc_cfg import *

# Change video direction on the fly: establish a bidirectional video
# call, then stop receiving video ("video call rx Off") and start again
# ("video call rx On"). Both changes are renegotiated with a re-INVITE,
# so the resulting direction is asserted on the wire -- sendonly, then
# back to sendrecv -- as well as through the media state each side
# reports.
#
# Note the On/Off argument is capitalized: the CLI matches static choice
# values case-sensitively, and this one is declared as On/Off.


def test_func(t):
    callee = t.process[0]
    caller = t.process[1]

    vid.make_video_call(caller, callee, t.inst_params[0].uri)

    # Turn RX off on stream #1 (media 0 is audio, media 1 is the video
    # stream), leaving the encoding direction only: the caller re-offers
    # the video line as sendonly.
    #
    # The resulting states are directional, so assert each side's
    # specific state. The caller keeps an Active -- now one-way --
    # stream, while for the callee a remote sendonly offer means its own
    # video is no longer wanted, which pjsua reports as Remote hold.
    caller.send("video call rx Off 1")
    callee.expect("a=sendonly")
    caller.expect(const.VID_MEDIA_ACTIVE)
    callee.expect(const.VID_MEDIA_REMOTE_HOLD)

    caller.sync_stdout()
    callee.sync_stdout()

    # Turn RX back on: the video line returns to sendrecv and the callee
    # resumes sending, so video is Active on both ends again.
    caller.send("video call rx On 1")
    callee.expect("a=sendrecv")
    caller.expect(const.VID_MEDIA_ACTIVE)
    callee.expect(const.VID_MEDIA_ACTIVE)

    caller.sync_stdout()
    callee.sync_stdout()

    vid.hangup_call(caller, callee)


test_param = TestParam(
        "Video call change direction on the fly",
        [
            InstanceParam("callee", vid.VIDEO_ARGS),
            InstanceParam("caller", vid.VIDEO_ARGS)
        ],
        func=test_func
        )

if not util.has_video(G_EXE):
    test_param.skip = True
