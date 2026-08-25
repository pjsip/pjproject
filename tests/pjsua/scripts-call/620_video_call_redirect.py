#
import inc_vid as vid
import inc_const as const
import inc_util as util
from inc_cfg import *

# 3xx redirection of a video call, pjsua<->pjsua<->pjsua:
#   A calls B; B answers 302 Moved Temporarily with a Contact of C; A
#   (default redirect handling = ACCEPT_REPLACE) automatically re-sends the
#   INVITE to C, which answers 200. Verifies pjsua follows a 3xx redirect
#   on a video call. The 302 Contact is C's full URI (with port), so no
#   default-port pinning is needed here.


def test_func(t):
    a = t.process[0]    # caller (follows the redirect)
    b = t.process[1]    # first callee, redirects to C with 302
    c = t.process[2]    # redirect target

    # A and C exchange the media; B only sends a 302 and needs no devices.
    vid.setup_video_devices(a)
    vid.setup_video_devices(c)

    # A calls B.
    a.send("call new " + t.inst_params[1].uri)
    a.expect(const.STATE_CALLING)

    # B redirects the call to C with 302 Moved Temporarily (Contact: <C>).
    b.expect(const.EVENT_INCOMING_CALL)
    b.send("call answer 302 " + t.inst_params[2].uri)

    # A follows the redirect (default ACCEPT_REPLACE) and re-INVITEs C.
    c.expect(const.EVENT_INCOMING_CALL)
    c.send("call answer 200")

    # The redirected call (A<->C) has an active video stream on both ends.
    a.expect(const.VID_MEDIA_ACTIVE)
    c.expect(const.VID_MEDIA_ACTIVE)
    a.expect(const.STATE_CONFIRMED)
    c.expect(const.STATE_CONFIRMED)

    a.sync_stdout()
    c.sync_stdout()

    vid.hangup_call(a, c)


test_param = TestParam(
        "Video call 3xx redirect",
        [
            InstanceParam("A", vid.VIDEO_ARGS),
            InstanceParam("B", vid.VIDEO_ARGS),
            InstanceParam("C", vid.VIDEO_ARGS)
        ],
        func=test_func
        )

if not util.has_video(G_EXE):
    test_param.skip = True
