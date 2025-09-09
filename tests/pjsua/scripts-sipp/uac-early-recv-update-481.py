#
import inc_const as const

PJSUA = ["--null-audio --max-calls=1 --auto-answer=183"]

PJSUA_EXPECTS = [[0, const.STATE_EARLY, "U"],
                 [0, const.STATE_DISCONNECTED, ""]]
