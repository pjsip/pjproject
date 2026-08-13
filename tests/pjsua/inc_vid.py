# Shared helpers for pjsua video call tests.
#
# Every video call test needs the same headless setup: run each instance
# with --video and drive it onto software video devices -- the colorbar
# generator for capture and the null renderer for output -- so no real
# camera or display is touched. See the individual helpers below.
import re
import inc_const as const
from inc_cfg import TestError

# Default pjsua args for a video-capable instance. --video makes pjsua set
# vid_cnt=1, in_auto_show and out_auto_transmit, so a call between two such
# instances negotiates an active, bidirectional video stream automatically
# (no per-call "video enable" is needed).
#
# --no-tcp restricts each instance to a single (UDP) account. Without it,
# --local-port creates both a UDP and a TCP account and leaves TCP as the
# current one, while calls travel over UDP -- so the "video acc cap_id/
# ren_id" pins below (which target the current account) might not apply to
# the account the call actually uses, leaving it on the default camera/
# renderer. One account keeps the headless device pinning deterministic.
VIDEO_ARGS = "--null-audio --max-calls=1 --no-tcp --video"


# Look up the ID of a software video device by name from the
# "video device list" output.
#
# IDs can't be hardcoded: the colorbar and null devices register after the
# platform's real devices, so their IDs differ across platforms (e.g.
# macOS with a camera vs. a headless Linux CI runner). Hence the runtime
# lookup. 'ua' is a run.py Expect instance; expect() returns the matched
# line, from which we parse the leading device ID.
def find_vid_dev(ua, name):
    ua.send("video device list")
    # Match only a non-negative device ID. The list first prints the
    # default-alias rows ("-2 <name> (default ...)", "-1 <name>"), and on a
    # headless build those aliases ARE the colorbar/null devices -- so an
    # unanchored [0-9]+ would grab the "1"/"2" from "-1"/"-2" and select
    # the wrong device (e.g. Null capture instead of Null renderer).
    # Requiring whitespace -- not the '-' sign -- immediately before the
    # digits skips the alias rows and selects the real positive device
    # row. The same pattern is reused for the ID extraction.
    pat = r"\s(\d+)\s+" + name
    line = ua.expect(pat)
    return re.search(pat, line).group(1)


# Pin 'ua' onto headless-safe video devices: the colorbar generator for
# capture and the null renderer for output. Call this on each instance
# before it places or answers a video call, so the call runs without a
# real camera or display.
def setup_video_devices(ua):
    cap = find_vid_dev(ua, "Colorbar generator")
    ren = find_vid_dev(ua, "Null renderer")
    ua.send("video acc cap_id " + cap)
    ua.send("video acc ren_id " + ren)
    ua.sync_stdout()


# Assert that the m=video section of the next SDP body 'ua' logs carries
# an attribute matching 'attr' (an unanchored regex, e.g. "a=sendrecv").
# 'what' names it for the failure message.
#
# Scoping the match to that one section is the whole point of this helper.
# expect() searches every line after the one it matched, with no notion of
# where a media section -- or even the message -- ends, so expecting
# "m=video" and then the attribute does NOT tie the two together: when the
# video section lacks the attribute, the search simply runs on into the
# next section, or into the following SIP message, and the audio section's
# copy of the same attribute satisfies it. Every SDP here has an audio
# section before the video one, and audio is the stream these tests leave
# untouched, so that mistake reliably passes.
#
# Hence the second expect() matches either the attribute or the start of
# the next media section, whichever comes first, and treats the latter as
# a failure: the video section ended without the attribute.
def expect_vid_sdp_attr(ua, attr, what):
    ua.expect(r"^\s*m=video [1-9]")
    line = ua.expect(r"^\s*m=|^\s*" + attr)
    if re.match(r"\s*m=", line):
        raise TestError(ua.name + ": no " + what + " in the m=video "
                        "section, it ends at: " + line.strip())


# Establish a confirmed video call from 'caller' to 'callee' and verify
# the video stream is Active on both ends. Pins headless video devices on
# both instances first. 'callee_uri' is the SIP URI the caller dials
# (normally t.inst_params[0].uri). Leaves both instances in a confirmed
# call with an active bidirectional video stream, ready for the
# scenario-specific action (hold, re-INVITE, DTMF, ...).
#
# 'codec' additionally asserts which video codec the two ends negotiated,
# e.g. codec="H264". It has to be checked here rather than by the caller
# of this helper: each endpoint logs the codec it started the stream with
# just *before* its media-active log, so by the time this function
# returns that line has already gone by.
#
# 'sync' flushes each instance's output before returning, so a test that
# goes on to drive the call is not matched against output produced during
# setup. Pass sync=False when the next thing to assert is not triggered by
# the test but happens on its own after the call is up (ICE completing,
# say): in non-telnet mode the flush consumes output up to an echoed
# marker, which could swallow such a log before the test looks for it.
def make_video_call(caller, callee, callee_uri, codec=None, sync=True):
    setup_video_devices(caller)
    setup_video_devices(callee)

    caller.send("call new " + callee_uri)
    caller.expect(const.STATE_CALLING)

    callee.expect(const.EVENT_INCOMING_CALL)
    callee.send("call answer 200")

    if codec:
        pat = r"video updated, stream #[0-9]+: " + codec + r" \(sendrecv\)"
        caller.expect(pat)
        callee.expect(pat)

    # The media-active log is emitted just before the call-state-changed-
    # to-CONFIRMED log. Wait for the video stream to go Active first (this
    # proves video negotiated), then for CONFIRMED -- which is still ahead
    # in the stream, so expect() finds it. Both are required: we must not
    # return before the dialog is CONFIRMED, otherwise a caller that
    # immediately holds would race the CONFIRMED transition and
    # pjsua_call_set_hold2() would reject the hold with PJSIP_ESESSIONSTATE
    # (telnet-mode sync_stdout() is a no-op, so it can't cover the gap).
    caller.expect(const.VID_MEDIA_ACTIVE)
    callee.expect(const.VID_MEDIA_ACTIVE)
    caller.expect(const.STATE_CONFIRMED)
    callee.expect(const.STATE_CONFIRMED)

    if sync:
        caller.sync_stdout()
        callee.sync_stdout()


# Hang up the call from 'hangup_by' and verify both endpoints disconnect
# via BYE. 'other' is the peer. The peer receives the BYE before its own
# disconnect log, so asserting it there confirms a BYE teardown (rather
# than some other path that clears the call) without racing the disconnect.
def hangup_call(hangup_by, other):
    hangup_by.send("call hangup")
    other.expect("BYE sips?:")
    hangup_by.expect(const.STATE_DISCONNECTED)
    other.expect(const.STATE_DISCONNECTED)
