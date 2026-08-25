#
import time
import inc_const as const
import inc_util as util
from inc_cfg import *

# Audio call RTCP XR (RFC 3611, extended reports) support: once the call
# is up, pjsua exchanges RTCP XR blocks in addition to plain RTCP. We
# verify this via the call-quality dump: an "Extended reports:" section
# (with a Statistics Summary / VoIP metrics) is printed for the audio
# stream only when RTCP XR is enabled and running on that stream.
#
# RTCP XR is a build option (PJMEDIA_HAS_RTCP_XR) that is off by default,
# so this test skips unless the binary was compiled with it (see
# util.has_rtcp_xr). Like plain RTCP, the reports are sent on the ~5s
# (jittered) RTCP interval, so poll with a bounded, generous budget.
POLL_ATTEMPTS = 15
POLL_INTERVAL = 2


def xr_reported(ua):
    ua.send("call dump_q")
    ua.expect(r"#[0-9]+ audio")
    # Two conditions must hold, in order, within the same audio-stream
    # block. (1) The "Extended reports:" section is present -- printed only
    # when RTCP XR is enabled on the stream. (2) The XR summary's "TX last
    # update" shows a time "ago" (not "never"): the heading alone appears
    # as soon as XR is enabled locally, so requiring the TX update proves a
    # peer XR report was actually received, i.e. the XR exchange works.
    # Absence of either just means "not yet" -- don't raise, keep polling.
    if ua.expect(r"Extended reports:", raise_on_error=False, timeout=3) is None:
        return False
    line = ua.expect(r"TX last update: .*(ago|never)",
                     raise_on_error=False, timeout=3)
    return line is not None and "ago" in line


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

    # Poll until the caller's quality dump shows a received XR report for
    # the audio stream.
    got_xr = False
    for attempt in range(POLL_ATTEMPTS):
        if xr_reported(caller):
            got_xr = True
            break
        time.sleep(POLL_INTERVAL)
    if not got_xr:
        raise TestError("no RTCP XR report received for the audio stream "
                        "(RTCP XR compiled in but not enabled at run-time? "
                        "see PJMEDIA_STREAM_ENABLE_XR)")

    caller.sync_stdout()
    callee.sync_stdout()

    caller.send("call hangup")
    callee.expect("BYE sips?:")
    caller.expect(const.STATE_DISCONNECTED)
    callee.expect(const.STATE_DISCONNECTED)


test_param = TestParam(
        "Audio call RTCP XR",
        [
            InstanceParam("callee", "--null-audio --max-calls=1 --no-tcp --rtcp-xr"),
            InstanceParam("caller", "--null-audio --max-calls=1 --no-tcp --rtcp-xr")
        ],
        func=test_func
        )

if not util.has_rtcp_xr(G_EXE):
    test_param.skip = True
