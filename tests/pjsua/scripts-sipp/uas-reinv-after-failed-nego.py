# $Id$
#
import inc_const as const

PJSUA = ["--null-audio --max-calls=1 --use-timer=3 --timer-se=90 --no-tcp $SIPP_URI"]

PJSUA_EXPECTS = [[0, const.STATE_CONFIRMED, "v"],
                 [0, "SDP rejected for test", ""],
                 [0, "Audio updated", "", 50] # wait for session timer refresh about 45 secs
                ]