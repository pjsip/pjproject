#
import inc_vid as vid
import inc_const as const
import inc_util as util
from inc_cfg import *

# Video call with SIP session timer (RFC 4028) negotiation. Both endpoints
# run with --use-timer=2 (session timer required) and the minimum
# Session-Expires of 90s. The test verifies the timer is negotiated on the
# video call: the INVITE carries Session-Expires/Require:timer and the 200
# OK confirms it (with a refresher). A live mid-call refresh is not
# asserted -- it fires at SE/2, i.e. no sooner than 45s (the 90s SE floor
# is enforced by pjsua), which is impractical for a fast test; the
# existing session-timer tests likewise check negotiation only.
TIMER_ARGS = vid.VIDEO_ARGS + " --use-timer=2 --timer-se=90 --timer-min-se=90"


def test_func(t):
    callee = t.process[0]
    caller = t.process[1]

    vid.setup_video_devices(caller)
    vid.setup_video_devices(callee)

    caller.send("call new " + t.inst_params[0].uri)
    caller.expect(const.STATE_CALLING)

    # The incoming INVITE must carry the required session-timer offer:
    # Require: timer (from --use-timer=2) followed by Session-Expires. Both
    # are serialized in that order and precede the incoming-call
    # notification. Asserting Require too ensures the timer is actually
    # *required*, not merely that a Session-Expires slipped in.
    callee.expect("Require: *timer")
    callee.expect("Session-Expires: *90")
    callee.expect(const.EVENT_INCOMING_CALL)
    callee.send("call answer 200")

    # The 200 OK confirms the session timer (with a negotiated refresher),
    # and the video stream negotiates Active.
    caller.expect("Session-Expires: *90.*refresher=")
    caller.expect(const.VID_MEDIA_ACTIVE)
    callee.expect(const.VID_MEDIA_ACTIVE)
    caller.expect(const.STATE_CONFIRMED)
    callee.expect(const.STATE_CONFIRMED)

    caller.sync_stdout()
    callee.sync_stdout()

    vid.hangup_call(caller, callee)


test_param = TestParam(
        "Video call session timer",
        [
            InstanceParam("callee", TIMER_ARGS),
            InstanceParam("caller", TIMER_ARGS)
        ],
        func=test_func
        )

if not util.has_video(G_EXE):
    test_param.skip = True
