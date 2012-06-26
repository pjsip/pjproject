# $Id$
#
import inc_const as const

PJSUA = ["--null-audio --max-calls=1 sip:localhost:6000"]

PJSUA_EXPECTS = [[0, const.STATE_CONFIRMED, "U"]]
