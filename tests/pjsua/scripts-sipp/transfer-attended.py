# $Id$
#
import inc_const as const

PJSUA = ["--null-audio",    # UA0
	 "--null-audio",    # UA1
	 "--null-audio"	    # UA2
	]

PJSUA_EXPECTS = [
		 # A calls B
		 [0, "", "m"],
		 [0, "", "$PJSUA_URI[1]"],
		 [0, const.STATE_CALLING, ""],
		 [1, const.EVENT_INCOMING_CALL, "a"],
		 [1, "", "200"],
		 [0, const.STATE_CONFIRMED, ""],
		 [1, const.STATE_CONFIRMED, ""],

		 # B holds A
		 [1, "", "H"],
		 [0, const.MEDIA_HOLD, ""],
		 [1, const.MEDIA_HOLD, ""],

		 # B calls C
		 [1, "", "m"],
		 [1, "", "$PJSUA_URI[2]"],
		 [1, const.STATE_CALLING, ""],
		 [2, const.EVENT_INCOMING_CALL, "a"],
		 [2, "", "200"],
		 [1, const.STATE_CONFIRMED, ""],
		 [2, const.STATE_CONFIRMED, ""],

		 # B holds C
		 [1, "", "]"],
		 [1, "", "H"],
		 [2, const.MEDIA_HOLD, ""],
		 [1, const.MEDIA_HOLD, ""],
		 [1, "", "]"],

		 # B transfer A to C
		 [1, "", "X"],
		 [1, "", "1"],
		 [0, "Call .* is being transfered", ""],
		 [1, "Subscription state .* ACCEPTED", ""],
		 [0, const.STATE_CALLING, ""],
		 [2, "Call .* is being replaced", ""],
		 [1, "call transfered successfully", ""],
		 [0, const.MEDIA_ACTIVE, ""],
		 [2, const.MEDIA_ACTIVE, ""],
		 [1, const.STATE_DISCONNECTED, ""]
		]
