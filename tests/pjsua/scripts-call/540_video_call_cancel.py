#
import inc_vid as vid
import inc_const as const
import inc_util as util
from inc_cfg import *

# Video call cancelled by the caller before it is answered (CANCEL). No
# media is established, so no video device setup is needed; the INVITE
# still carries the video offer.


def test_func(t):
    callee = t.process[0]
    caller = t.process[1]

    # Caller places the video call.
    caller.send("call new " + t.inst_params[0].uri)
    caller.expect(const.STATE_CALLING)

    # Verify the callee actually received a video offer (enabled m=video
    # with a non-zero port), so this exercises the video CANCEL case rather
    # than passing for an audio-only INVITE.
    callee.expect("m=video [1-9]")

    # Callee rings (180) but does not answer.
    callee.expect(const.EVENT_INCOMING_CALL)
    callee.send("call answer 180")
    caller.expect("SIP/2.0 180")

    caller.sync_stdout()
    callee.sync_stdout()

    # Caller cancels before the call is answered -> CANCEL; both disconnect.
    # Assert the CANCEL on the callee: it receives CANCEL before it
    # disconnects, whereas on the caller the DISCONNECTED log precedes the
    # outgoing CANCEL, so expecting CANCEL there would race the disconnect.
    caller.send("call hangup")
    callee.expect("CANCEL sip:")
    caller.expect(const.STATE_DISCONNECTED)
    callee.expect(const.STATE_DISCONNECTED)


test_param = TestParam(
        "Video call CANCEL (before answer)",
        [
            InstanceParam("callee", vid.VIDEO_ARGS),
            InstanceParam("caller", vid.VIDEO_ARGS)
        ],
        func=test_func
        )

if not util.has_video(G_EXE):
    test_param.skip = True
