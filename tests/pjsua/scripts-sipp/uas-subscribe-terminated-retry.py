# $Id$
#
import inc_const as const

PJSUA = ["--null-audio --max-calls=1 --id sip:pjsua@localhost:6000 --add-buddy sip:sipp@localhost:6000"]

PJSUA_EXPECTS = [[0, "", "s"],
		 [0, "Subscribe presence of:", "1"],
		 [0, "Presence subscription .* is TERMINATED", ""],
		 [0, "Resubscribing .* in 5000 ms", ""]
		 ]
