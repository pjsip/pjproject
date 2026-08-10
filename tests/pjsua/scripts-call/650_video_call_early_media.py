#
import inc_vid as vid
import inc_const as const
import inc_util as util
from inc_cfg import *

# Video call with early media: the callee answers 183 Session Progress
# (which carries the SDP answer), so the video stream negotiates and goes
# Active while the call is still in the EARLY state -- before the final
# 200 OK.


def test_func(t):
    callee = t.process[0]
    caller = t.process[1]

    vid.setup_video_devices(caller)
    vid.setup_video_devices(callee)

    caller.send("call new " + t.inst_params[0].uri)
    caller.expect(const.STATE_CALLING)

    # Callee answers 183 with SDP -> early media.
    callee.expect(const.EVENT_INCOMING_CALL)
    callee.send("call answer 183")

    # Both enter the EARLY state and the video stream goes Active there,
    # before the call is confirmed. The two logs are emitted in a different
    # order on each side (the caller sees EARLY then media-active on
    # receiving the 183; the callee activates media while building the 183,
    # then transitions to EARLY), so assert each side in its own order.
    caller.expect(const.STATE_EARLY)
    caller.expect(const.VID_MEDIA_ACTIVE)
    callee.expect(const.VID_MEDIA_ACTIVE)
    callee.expect(const.STATE_EARLY)

    caller.sync_stdout()
    callee.sync_stdout()

    # Callee now accepts the call; media stays active through CONFIRMED
    # (the 200 OK does not re-run offer/answer).
    callee.send("call answer 200")
    caller.expect(const.STATE_CONFIRMED)
    callee.expect(const.STATE_CONFIRMED)

    caller.sync_stdout()
    callee.sync_stdout()

    vid.hangup_call(caller, callee)


test_param = TestParam(
        "Video call early media (183 w/ SDP)",
        [
            InstanceParam("callee", vid.VIDEO_ARGS),
            InstanceParam("caller", vid.VIDEO_ARGS)
        ],
        func=test_func
        )

if not util.has_video(G_EXE):
    test_param.skip = True
