# $Id$
#
import inc_const as const

PJSUA = ["--null-audio --max-calls=1 --id=sip:pjsua@localhost --username=pjsua --realm=* $SIPP_URI"]

PJSUA_EXPECTS = [[0, "ACK sip:proxy@.* SIP/2\.0", ""],
		 [0, const.STATE_CONFIRMED, "h"]
 		 ]

PJSUA_CLI_EXPECTS = [[0, "ACK sip:proxy@.* SIP/2\.0", ""],
		 [0, const.STATE_CONFIRMED, "call hangup"]
 		 ]
