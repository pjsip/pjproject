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

		 # B transfer A to C
		 [1, "", "x"],
		 [1, "", "$PJSUA_URI[2]"],
		 [0, const.STATE_CALLING, ""],
		 [2, const.EVENT_INCOMING_CALL, "a"],
		 [2, "", "200"],
		 [0, const.MEDIA_ACTIVE, ""],
		 [2, const.MEDIA_ACTIVE, ""],
		 [1, "call transferred successfully", ""],
		 [1, const.STATE_DISCONNECTED, ""]
		]

PJSUA_CLI_EXPECTS = [
		 # A calls B
		 [0, "", "call new $PJSUA_URI[1]"],
		 [0, const.STATE_CALLING, ""],
		 [1, const.EVENT_INCOMING_CALL, "call answer 200"],
		 [0, const.STATE_CONFIRMED, ""],
		 [1, const.STATE_CONFIRMED, ""],

		 # B transfer A to C
		 [1, "", "call transfer $PJSUA_URI[2]"],
		 [0, const.STATE_CALLING, ""],
		 [2, const.EVENT_INCOMING_CALL, "call answer 200"],
		 [0, const.MEDIA_ACTIVE, ""],
		 [2, const.MEDIA_ACTIVE, ""],
		 [1, "call transferred successfully", ""],
		 [1, const.STATE_DISCONNECTED, ""]
		]
