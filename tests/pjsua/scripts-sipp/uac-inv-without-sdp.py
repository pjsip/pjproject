# $Id$
#
import inc_const as const

PJSUA = ["--null-audio --max-calls=1"]

PJSUA_EXPECTS = [[0, const.EVENT_INCOMING_CALL, "a"],
		 [0, "", "200"],
		 [0, const.MEDIA_ACTIVE, ""],
		 [0, const.STATE_CONFIRMED, "h"]
		 ]
