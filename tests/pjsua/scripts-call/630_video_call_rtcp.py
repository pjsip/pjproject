#
import time
import inc_vid as vid
import inc_const as const
import inc_util as util
from inc_cfg import *

# Video call RTCP support: after the call is up, wait for at least one RTCP
# reporting interval and then dump the call quality. For the video stream,
# the TX statistics' "last update" must show a time "ago" (not "never"),
# which means an RTCP report from the peer was received for that stream --
# i.e. RTCP is flowing on the video media.
#
# The default RTCP interval is 5s (PJMEDIA_RTCP_INTERVAL), and it is
# wall-clock driven, so a fixed wait is reliable regardless of CPU load.
RTCP_WAIT = 8


def test_func(t):
    callee = t.process[0]
    caller = t.process[1]

    vid.make_video_call(caller, callee, t.inst_params[0].uri)

    # Let RTCP reports be exchanged in both directions.
    time.sleep(RTCP_WAIT)

    # Dump call quality and assert the video stream received peer RTCP.
    caller.send("call dump_q")
    caller.expect(r"#[0-9]+ video")
    caller.expect(r"TX .*last update:.*ago")

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
