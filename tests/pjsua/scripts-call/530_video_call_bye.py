#
import inc_vid as vid
import inc_util as util
from inc_cfg import *

# Video call terminated with BYE (normal hangup).


def test_func(t):
    callee = t.process[0]
    caller = t.process[1]

    vid.make_video_call(caller, callee, t.inst_params[0].uri)

    # Caller hangs up -> BYE; both endpoints disconnect.
    vid.hangup_call(caller, callee)


test_param = TestParam(
        "Video call BYE (hangup)",
        [
            InstanceParam("callee", vid.VIDEO_ARGS),
            InstanceParam("caller", vid.VIDEO_ARGS)
        ],
        func=test_func
        )

if not util.has_video(G_EXE):
    test_param.skip = True
