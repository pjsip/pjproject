# $Id$
#
import inc_const as const

PJSUA = ["--null-audio --max-calls=1 --use-100rel"]

PJSUA_EXPECTS = [[0, const.EVENT_INCOMING_CALL, "a"],
		 [0, "", "183"],
		 [0, "Response msg 200/PRACK", ""],
		 [0, "Content-Type: application/sdp", "a"],
		 [0, "", "200"],
		 [0, const.STATE_CONFIRMED, ""],
		 [0, const.STATE_DISCONNECTED, ""]
		 ]

PJSUA_CLI_EXPECTS = [[0, const.EVENT_INCOMING_CALL, "call answer 183"],
		 [0, "Response msg 200/PRACK", ""],
		 [0, "Content-Type: application/sdp", "call answer 200"],
		 [0, const.STATE_CONFIRMED, ""],
		 [0, const.STATE_DISCONNECTED, ""]
		 ]
