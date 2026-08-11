#
import inc_vid as vid
import inc_const as const
import inc_util as util
from inc_cfg import *

# Video call pinned to H.264. Both ends raise H264 to the highest codec
# priority before the call, so it heads the offer and is what the answer
# selects, and the test then verifies H264 is the codec each endpoint
# reports starting its video stream with.
#
# That report, not the SDP, is what is checked: it names the codec the
# stream actually runs, whereas an m=video line lists every codec merely
# offered. So a regression that negotiated something other than H264 --
# or failed to apply the priority at all -- is caught, while one that
# only reshuffles the offered payload types is not.


def test_func(t):
    callee = t.process[0]
    caller = t.process[1]

    # Prefer H264 on both ends. "H264" is a prefix of the codec id the
    # CLI lists ("H264/<payload type>"), which is enough to select it;
    # 255 is the maximum priority, so it sorts ahead of any other video
    # codec the build happens to have.
    caller.send("video codec prio H264 255")
    callee.send("video codec prio H264 255")

    caller.sync_stdout()
    callee.sync_stdout()

    # Establish the call and assert the codec both ends report starting
    # the stream with. Checking both sides covers the whole negotiation:
    # the caller offered H264 and the callee answered with it.
    vid.make_video_call(caller, callee, t.inst_params[0].uri, codec="H264")

    vid.hangup_call(caller, callee)


test_param = TestParam(
        "Video call with H264",
        [
            InstanceParam("callee", vid.VIDEO_ARGS),
            InstanceParam("caller", vid.VIDEO_ARGS)
        ],
        func=test_func
        )

# Skip unless the build under test actually has H264 -- video support
# alone is not enough, the build may have been configured with a
# different video codec library (e.g. VPX only).
if not util.has_vid_codec(G_EXE, "H264"):
    test_param.skip = True
