#
import socket
import inc_vid as vid
import inc_const as const
import inc_util as util
from inc_cfg import *

# Attended video call transfer, pjsua<->pjsua<->pjsua (REFER with Replaces):
#   A calls B; B holds A and makes a consultation call to C; B then
#   transfers A to C with a REFER whose Refer-To carries a Replaces header,
#   so A's new INVITE replaces B's call leg to C. Exercises the attended
#   transferor (B) and transferee (A) roles in one flow.
#
# --max-calls is raised because the transferor (B) holds two calls (A and
# C) during the consultation.
XFER_ARGS = vid.VIDEO_ARGS + " --max-calls=4"

# The attended-transfer Refer-To is derived from the replaced dialog's
# remote AOR (the peer's account id / From URI), which pjsua emits WITHOUT
# a port -- even if the account --id carries one -- and NOT from its
# Contact. With no registrar in this loopback test that portless AOR
# resolves to the default SIP port (5060), so the transferee's
# INVITE-with-Replaces only reaches the replaced party if it listens
# there. A (process[0]) is that replaced party, so pin it to 5060.
#
# A is also given --bound-addr=127.0.0.1 so its transport binds the
# loopback interface only (not 0.0.0.0): this keeps the derived AOR on
# 127.0.0.1 and makes A's actual listening socket match the loopback
# free-port probe below (and avoids binding all interfaces). B and C keep
# auto-assigned ports.
SIP_PORT_DEFAULT = 5060
A_ARGS = XFER_ARGS + " --bound-addr=127.0.0.1"


# An explicitly supplied sip_port bypasses InstanceParam's free-port probe,
# so 5060 being already in use would make the test fail at startup rather
# than skip. Probe it (on the same loopback interface A binds) and skip the
# test if it isn't available.
def _port_available(port):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.bind(("127.0.0.1", port))
        return True
    except socket.error:
        return False
    finally:
        s.close()


def test_func(t):
    a = t.process[0]    # caller -> transferee
    b = t.process[1]    # callee  -> transferor
    c = t.process[2]    # transfer target

    # A calls B.
    vid.make_video_call(a, b, t.inst_params[1].uri)

    # B holds A.
    b.send("call hold")
    a.expect(const.VID_MEDIA_HOLD)
    b.expect(const.VID_MEDIA_HOLD)
    a.sync_stdout()
    b.sync_stdout()

    # B makes a consultation call to C.
    vid.setup_video_devices(c)
    b.send("call new " + t.inst_params[2].uri)
    b.expect(const.STATE_CALLING)
    c.expect(const.EVENT_INCOMING_CALL)
    c.send("call answer 200")
    b.expect(const.VID_MEDIA_ACTIVE)
    c.expect(const.VID_MEDIA_ACTIVE)
    b.expect(const.STATE_CONFIRMED)
    c.expect(const.STATE_CONFIRMED)
    b.sync_stdout()
    c.sync_stdout()

    # Attended-transfer using the consultation call (C, call 1) as the
    # current call: 'call transfer_replaces 0' sends a REFER to C whose
    # Refer-To carries a Replaces of the B<->A dialog (call 0), so C's new
    # INVITE replaces B's leg to A -- leaving A and C connected. We refer C
    # (not A) because the CLI's call-id choice list only offers calls
    # enumerated before the current one, so with C current the only valid
    # destination is call 0 = A.
    b.send("call transfer_replaces 0")
    c.expect("Call .* is being transferred")
    b.expect("Subscription state .* ACCEPTED")
    c.expect(const.STATE_CALLING)
    a.expect("Call .* is being replaced")
    b.expect("call transferred successfully")

    # The resulting A<->C call has an active video stream on both ends.
    a.expect(const.VID_MEDIA_ACTIVE)
    c.expect(const.VID_MEDIA_ACTIVE)
    b.expect(const.STATE_DISCONNECTED)

    a.sync_stdout()
    c.sync_stdout()

    vid.hangup_call(a, c)


test_param = TestParam(
        "Video call attended transfer",
        [
            InstanceParam("A", A_ARGS, sip_port=SIP_PORT_DEFAULT),
            InstanceParam("B", XFER_ARGS),
            InstanceParam("C", XFER_ARGS)
        ],
        func=test_func
        )

if not util.has_video(G_EXE) or not _port_available(SIP_PORT_DEFAULT):
    test_param.skip = True
