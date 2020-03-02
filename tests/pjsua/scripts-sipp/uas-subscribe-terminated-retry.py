# $Id$
#
import inc_const as const

PJSUA = ["--null-audio --max-calls=1 --id sip:pjsua@localhost --add-buddy $SIPP_URI"]

PJSUA_EXPECTS = [[0, "", "s"],
		 [0, "Subscribe presence of:", "1"],
		 [0, "Presence subscription .* is TERMINATED", ""],
		 [0, "Resubscribing .* in 5000 ms", ""]
		 ]

PJSUA_CLI_EXPECTS = [[0, "", "im sub_pre 1"],
		 [0, "Presence subscription .* is TERMINATED", ""],
		 [0, "Resubscribing .* in 5000 ms", ""]
		 ]
