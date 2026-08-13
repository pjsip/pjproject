#
import inc_vid as vid
import inc_const as const
import inc_util as util
from inc_cfg import *

# Unattended (blind) video call transfer, pjsua<->pjsua<->pjsua:
#   A calls B; B transfers A to C via REFER (no Replaces); A places a new
#   video call to C. Exercises both the transferor (B, sends REFER and
#   receives transfer-progress NOTIFY) and the transferee (A, follows the
#   REFER and calls C) roles in one flow.
#
# --max-calls is raised because the transferee briefly holds two calls
# (the original to B and the new one to C) during the transfer.
XFER_ARGS = vid.VIDEO_ARGS + " --max-calls=4"


def test_func(t):
    a = t.process[0]    # caller -> transferee (receives REFER, calls C)
    b = t.process[1]    # callee  -> transferor (sends REFER)
    c = t.process[2]    # transfer target

    # A calls B (sets up video devices on A and B).
    vid.make_video_call(a, b, t.inst_params[1].uri)

    # Prepare C's headless video devices before it receives the call.
    vid.setup_video_devices(c)

    # B transfers A to C (unattended REFER with Refer-To: <C>).
    b.send("call transfer " + t.inst_params[2].uri)

    # A follows the REFER and places a new video call to C.
    a.expect(const.STATE_CALLING)
    c.expect(const.EVENT_INCOMING_CALL)
    c.send("call answer 200")

    # The transferred call (A<->C) has an active video stream on both ends.
    a.expect(const.VID_MEDIA_ACTIVE)
    c.expect(const.VID_MEDIA_ACTIVE)

    # The transferor sees the transfer succeed (via NOTIFY) and its
    # original call to A is torn down.
    b.expect("call transferred successfully")
    b.expect(const.STATE_DISCONNECTED)

    a.sync_stdout()
    c.sync_stdout()

    # Tear down from C, which holds exactly the one A<->C call: until A has
    # processed B's BYE for the original A<->B leg it still has two calls,
    # and (telnet-mode sync_stdout() being a no-op) hanging up A's current
    # call could target the wrong one.
    vid.hangup_call(c, a)


test_param = TestParam(
        "Video call unattended transfer",
        [
            InstanceParam("A", XFER_ARGS),
            InstanceParam("B", XFER_ARGS),
            InstanceParam("C", XFER_ARGS)
        ],
        func=test_func
        )

if not util.has_video(G_EXE):
    test_param.skip = True
