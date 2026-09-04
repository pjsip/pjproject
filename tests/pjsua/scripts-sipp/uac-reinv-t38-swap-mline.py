#
import inc_const as const

# The re-INVITE turns media slot 0 from audio into T.38 (image/udptl), which
# pjsua does not manage, and adds the audio as slot 1. The audio stream that
# was on slot 0 must be stopped and the slot reported as unmanaged (type
# unknown, status None), not as audio; the audio must be active on slot 1.

PJSUA = ["--null-audio --max-calls=1 --auto-answer=200 --no-tcp"]

PJSUA_EXPECTS = [[0, const.MEDIA_ACTIVE, ""],
                 [0, r"Call [0-9]+ media 0 \[type=unknown\], status is None", ""],
                 [0, r"Call [0-9]+ media 1 \[type=audio\], status is Active", ""],
                 [0, const.STATE_DISCONNECTED, ""]
                ]
