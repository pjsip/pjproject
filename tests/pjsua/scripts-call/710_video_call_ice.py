#
import inc_vid as vid
import inc_const as const
import inc_util as util
from inc_cfg import *

# Video call over ICE. Both endpoints run with --use-ice, so every media
# stream -- audio and video alike -- gets its own ICE session that has to
# complete before the transport addresses are settled. No STUN server is
# configured, so the candidates are host candidates only, which is all a
# loopback call needs.
#
# What this adds over an audio-only ICE test is the video stream's own
# ICE session: a video call negotiates a second one, and the address
# update that follows ICE has to carry the video stream through intact.

ICE_ARGS = vid.VIDEO_ARGS + " --use-ice"


def test_func(t):
    callee = t.process[0]
    caller = t.process[1]

    # sync=False: everything asserted below happens on its own once ICE
    # completes, rather than in response to something this test sends, so
    # the helper must not flush output on the way out -- in non-telnet
    # mode that flush reads ahead to an echoed marker and could consume
    # the very logs checked here.
    vid.make_video_call(caller, callee, t.inst_params[0].uri, sync=False)

    # Once ICE completes, the controlling agent (the caller) re-offers to
    # replace the addresses in the SDP with the selected candidate pairs.
    # This is asserted instead of the per-transport "ICE negotiation
    # success" logs: those race with the call-state transition the call
    # helper above already waited for -- on the callee they land before
    # CONFIRMED, on the caller after -- whereas the re-offer is always
    # later than both. It is scheduled on a timer once every stream's ICE
    # session is running, so it also implies ICE did succeed.
    caller.expect("sending (UPDATE|re-INVITE) for updating ICE transport "
                  "address")

    # The video stream really is under ICE: it carries candidates of its
    # own in that re-offer, not just the audio stream above it. The check
    # is scoped to the m=video section -- see expect_vid_sdp_attr() --
    # since the audio section is full of candidates the video one would
    # otherwise be credited with.
    vid.expect_vid_sdp_attr(callee, "a=candidate", "ICE candidate")

    # And video survives the address update, still Active on both ends.
    caller.expect(const.VID_MEDIA_ACTIVE)
    callee.expect(const.VID_MEDIA_ACTIVE)

    caller.sync_stdout()
    callee.sync_stdout()

    vid.hangup_call(caller, callee)


test_param = TestParam(
        "Video call with ICE",
        [
            InstanceParam("callee", ICE_ARGS),
            InstanceParam("caller", ICE_ARGS)
        ],
        func=test_func
        )

if not util.has_video(G_EXE):
    test_param.skip = True
