# $Id$
#
import inc_const as const

PJSUA = ["--null-audio --extra-audio --max-calls=1 --no-tcp $SIPP_URI"]

# Send hold after remote holds (double hold)
PJSUA_EXPECTS = [[0, const.MEDIA_HOLD, ""],
		 [0, "ACK sip:", "H"]
		]
