#
import inc_vid as vid
import inc_const as const
import inc_util as util
from inc_cfg import *

# Unattended video call transfer with norefersub (RFC 4488): the transferor
# (B) is started with --norefersub, so its REFER carries "Refer-Sub: false"
# and the transferee (A) does not create the implicit REFER subscription
# (no NOTIFY traffic). The transfer itself still completes: A places a new
# video call to C.
XFER_ARGS = vid.VIDEO_ARGS + " --max-calls=4"


def test_func(t):
    a = t.process[0]    # caller -> transferee
    b = t.process[1]    # callee  -> transferor (started with --norefersub)
    c = t.process[2]    # transfer target

    vid.make_video_call(a, b, t.inst_params[1].uri)
    vid.setup_video_devices(c)

    # B transfers A to C; its REFER suppresses the subscription.
    b.send("call transfer " + t.inst_params[2].uri)

    # A receives the REFER carrying Refer-Sub: false...
    a.expect("Refer-Sub: *false")
    # ...and B, seeing the suppression, terminates its transfer event
    # subscription instead of keeping it for NOTIFYs. Asserting this proves
    # the no-subscription behaviour, not merely that the header was sent.
    b.expect("Xfer subscription suppressed")

    # A follows the REFER to C.
    a.expect(const.STATE_CALLING)
    c.expect(const.EVENT_INCOMING_CALL)
    c.send("call answer 200")

    # The transferred call (A<->C) has an active video stream on both ends.
    a.expect(const.VID_MEDIA_ACTIVE)
    c.expect(const.VID_MEDIA_ACTIVE)

    a.sync_stdout()
    c.sync_stdout()

    # Tear down from C, which has exactly one call (A<->C): with
    # norefersub A may briefly still hold the original A<->B call, so
    # hanging up from A's (ambiguous) current call would be unreliable.
    vid.hangup_call(c, a)
    # Best-effort cleanup of any residual leg on B.
    b.send("call hangup all")


test_param = TestParam(
        "Video call unattended transfer (norefersub)",
        [
            InstanceParam("A", XFER_ARGS),
            InstanceParam("B", XFER_ARGS + " --norefersub"),
            InstanceParam("C", XFER_ARGS)
        ],
        func=test_func
        )

if not util.has_video(G_EXE):
    test_param.skip = True
