#
import inc_const as const

# Re-INVITEs offering only T.38 (image/udptl), which pjsua does not manage,
# must be rejected with 488 by a plain pjsua (SIPp asserts the 488), leaving
# the audio session untouched until the BYE.

PJSUA = ["--null-audio --max-calls=1 --auto-answer=200 --no-tcp"]

PJSUA_EXPECTS = [[0, const.MEDIA_ACTIVE, ""],
                 [0, const.STATE_DISCONNECTED, ""]
                ]
