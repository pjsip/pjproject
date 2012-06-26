# $Id$
#
import inc_const as const

PJSUA = ["--null-audio",    # UA0 @ port 5060
	 "--null-audio",    # UA1 @ port 5062
	 "--null-audio"	    # UA2 @ port 5064
	]

PJSUA_EXPECTS = [
		 # A calls B
		 [0, "", "m"],
		 [0, "", "sip:localhost:5062"],
		 [1, const.EVENT_INCOMING_CALL, "a"],
		 [1, "", "200"],
		 [0, const.STATE_CONFIRMED, ""],
		 [1, const.STATE_CONFIRMED, ""],

		 # B transfer A to C
		 [1, "", "x"],
		 [1, "", "sip:localhost:5064"],
		 [0, const.STATE_CALLING, ""],
		 [2, const.EVENT_INCOMING_CALL, "a"],
		 [2, "", "200"],
		 [0, const.MEDIA_ACTIVE, ""],
		 [2, const.MEDIA_ACTIVE, ""],
		 [1, "call transfered successfully", ""],
		 [1, "", " "],
		 [1, "have 0 active call", ""],
		]
