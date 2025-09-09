# $Id$
#
import inc_const as const

PJSUA = ["--null-audio --max-calls=1 --no-tcp $SIPP_URI"]

PJSUA_EXPECTS = [[0, const.STATE_EARLY, ""],
		 [0, "487/INVITE", ""],
		 [0, const.STATE_DISCONNECTED, ""]
		]

PJSUA_CLI_EXPECTS = [[0, const.STATE_EARLY, ""],
		     [0, "487/INVITE", ""],
		     [0, const.STATE_DISCONNECTED, ""]
		    ]
