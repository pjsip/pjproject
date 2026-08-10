#
import time
import inc_vid as vid
import inc_const as const
import inc_util as util
from inc_cfg import *

# Video call RTCP support: once the call is up, the peer must send RTCP
# reports for the video stream. We verify this via the call-quality dump:
# the video stream's TX statistics' "last update" shows a time "ago" (not
# "never") once an RTCP report from the peer has been received for that
# stream.
#
# Video RTCP is emitted from video-frame processing (not an independent
# wall-clock timer) and the interval is randomized up to ~5.5s, so how
# soon the first report arrives can vary on a loaded runner. Poll the dump
# with a bounded, generous budget rather than assuming a fixed delay.
POLL_ATTEMPTS = 15
POLL_INTERVAL = 2


def video_rtcp_received(ua):
    ua.send("call dump_q")
    ua.expect(r"#[0-9]+ video")
    line = ua.expect(r"TX .*last update:.*(ago|never)")
    return "ago" in line


def test_func(t):
    callee = t.process[0]
    caller = t.process[1]

    vid.make_video_call(caller, callee, t.inst_params[0].uri)

    # Poll until the caller has received the peer's RTCP for the video
    # stream (or fail if it never arrives within the budget).
    got_rtcp = False
    for attempt in range(POLL_ATTEMPTS):
        if video_rtcp_received(caller):
            got_rtcp = True
            break
        time.sleep(POLL_INTERVAL)
    if not got_rtcp:
        raise TestError("no RTCP received for the video stream")

    caller.sync_stdout()
    callee.sync_stdout()

    vid.hangup_call(caller, callee)


test_param = TestParam(
        "Video call RTCP",
        [
            InstanceParam("callee", vid.VIDEO_ARGS),
            InstanceParam("caller", vid.VIDEO_ARGS)
        ],
        func=test_func
        )

if not util.has_video(G_EXE):
    test_param.skip = True
