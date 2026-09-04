#
import time
import inc_const as const
from inc_cfg import *

# Audio call RTCP support: once the call is up, the peer must send RTCP
# reports for the audio stream. We verify this via the call-quality dump:
# the audio stream's TX statistics' "last update" shows a time "ago" (not
# "never") once an RTCP report from the peer has been received for that
# stream.
#
# The default RTCP interval is ~5s (and jittered), so how soon the first
# report arrives can vary on a loaded runner. Poll the dump with a bounded,
# generous budget rather than assuming a fixed delay.
POLL_ATTEMPTS = 15
POLL_INTERVAL = 2


def rtcp_received(ua):
    ua.send("call dump_q")
    ua.expect(r"#[0-9]+ audio")
    line = ua.expect(r"TX .*last update:.*(ago|never)")
    return "ago" in line


def test_func(t):
    callee = t.process[0]
    caller = t.process[1]

    caller.send("call new " + t.inst_params[0].uri)
    caller.expect(const.STATE_CALLING)

    callee.expect(const.EVENT_INCOMING_CALL)
    callee.send("call answer 200")

    caller.expect(const.MEDIA_ACTIVE)
    callee.expect(const.MEDIA_ACTIVE)
    caller.expect(const.STATE_CONFIRMED)
    callee.expect(const.STATE_CONFIRMED)
    caller.sync_stdout()
    callee.sync_stdout()

    # Poll until the caller has received the peer's RTCP for the audio
    # stream (or fail if it never arrives within the budget).
    got_rtcp = False
    for attempt in range(POLL_ATTEMPTS):
        if rtcp_received(caller):
            got_rtcp = True
            break
        time.sleep(POLL_INTERVAL)
    if not got_rtcp:
        raise TestError("no RTCP received for the audio stream")

    caller.sync_stdout()
    callee.sync_stdout()

    caller.send("call hangup")
    callee.expect("BYE sips?:")
    caller.expect(const.STATE_DISCONNECTED)
    callee.expect(const.STATE_DISCONNECTED)


test_param = TestParam(
        "Audio call RTCP",
        [
            InstanceParam("callee", "--null-audio --max-calls=1 --no-tcp"),
            InstanceParam("caller", "--null-audio --max-calls=1 --no-tcp")
        ],
        func=test_func
        )
