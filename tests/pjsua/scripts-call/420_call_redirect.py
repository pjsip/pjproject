#
import inc_const as const
from inc_cfg import *

# 3xx redirection of an audio call, pjsua<->pjsua<->pjsua:
#   A calls B; B answers 302 Moved Temporarily with a Contact of C; A
#   (default redirect handling = ACCEPT_REPLACE) automatically re-sends the
#   INVITE to C, which answers 200. Verifies pjsua follows a 3xx redirect
#   on an audio call. The 302 Contact is C's full URI (with port), so no
#   default-port pinning is needed here.


def test_func(t):
    a = t.process[0]    # caller (follows the redirect)
    b = t.process[1]    # first callee, redirects to C with 302
    c = t.process[2]    # redirect target

    # A calls B.
    a.send("call new " + t.inst_params[1].uri)
    a.expect(const.STATE_CALLING)

    # B redirects the call to C with 302 Moved Temporarily (Contact: <C>).
    b.expect(const.EVENT_INCOMING_CALL)
    b.send("call answer 302 " + t.inst_params[2].uri)

    # A follows the redirect (default ACCEPT_REPLACE) and re-INVITEs C.
    c.expect(const.EVENT_INCOMING_CALL)
    c.send("call answer 200")

    # The redirected call (A<->C) has an active audio stream on both ends.
    a.expect(const.MEDIA_ACTIVE)
    c.expect(const.MEDIA_ACTIVE)
    a.expect(const.STATE_CONFIRMED)
    c.expect(const.STATE_CONFIRMED)

    a.sync_stdout()
    c.sync_stdout()

    a.send("call hangup")
    c.expect("BYE sips?:")
    a.expect(const.STATE_DISCONNECTED)
    c.expect(const.STATE_DISCONNECTED)


test_param = TestParam(
        "Audio call 3xx redirect",
        [
            InstanceParam("A", "--null-audio --max-calls=1 --no-tcp"),
            InstanceParam("B", "--null-audio --max-calls=1 --no-tcp"),
            InstanceParam("C", "--null-audio --max-calls=1 --no-tcp")
        ],
        func=test_func
        )
