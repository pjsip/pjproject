#
import inc_vid as vid
import inc_const as const
import inc_util as util
from inc_cfg import *

# Change the capture device on the fly: establish a video call (whose
# capture side is pinned to the "Colorbar generator" device by the shared
# helper), then switch the capturer of the running stream to the other
# headless capture device, "Colorbar-active", with "video call cap".
#
# This is a purely local operation -- no re-INVITE, the stream keeps
# running -- so it is verified through the capture window pjsua creates
# for the new device, plus the fact that video stays Active afterwards.


def test_func(t):
    callee = t.process[0]
    caller = t.process[1]

    vid.make_video_call(caller, callee, t.inst_params[0].uri)

    # The second software capture device. Like the colorbar/null devices
    # picked in the shared helper, its ID has to be looked up at runtime
    # because it depends on how many real devices the platform has.
    cap2 = vid.find_vid_dev(caller, "Colorbar-active")
    caller.sync_stdout()

    # Switch the capturer of stream #1 (media 0 is audio, media 1 is the
    # video stream). Colorbar devices do not support the fast in-place
    # device switch, so pjsua takes the general path: it creates a new
    # capture window for the target device and reconnects the encoder to
    # it. Asserting on cap_dev=<id> in that log pins the switch to the
    # device we actually asked for.
    caller.send("video call cap 1 " + cap2)
    caller.expect(r"window id [0-9]+ created for cap_dev=" + cap2 + " ")

    caller.sync_stdout()
    callee.sync_stdout()

    # The switch is local, so it emits no media state change of its own.
    # Force a re-INVITE to make both ends re-report their media: the
    # video stream must still be Active, i.e. swapping the capturer
    # under a running stream did not tear it down.
    caller.send("call reinvite")
    caller.expect(const.VID_MEDIA_ACTIVE)
    callee.expect(const.VID_MEDIA_ACTIVE)

    caller.sync_stdout()
    callee.sync_stdout()

    vid.hangup_call(caller, callee)


test_param = TestParam(
        "Video call change capture device on the fly",
        [
            InstanceParam("callee", vid.VIDEO_ARGS),
            InstanceParam("caller", vid.VIDEO_ARGS)
        ],
        func=test_func
        )

if not util.has_video(G_EXE):
    test_param.skip = True
