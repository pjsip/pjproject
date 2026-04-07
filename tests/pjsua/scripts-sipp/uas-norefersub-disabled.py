#
# Test that "norefersub" is absent from the Supported header when pjsua is
# started with --no-supported-norefersub (sets pjsua_config.no_refer_sub = PJ_FALSE).
# SIPp verifies the Supported header in the INVITE does NOT contain
# "norefersub" and fails the test if it does.
#
import inc_const as const

PJSUA = ["--null-audio --max-calls=1 --no-tcp --no-supported-norefersub $SIPP_URI"]

PJSUA_EXPECTS = [
		 [0, const.STATE_CONFIRMED,     ""],
		 [0, const.STATE_DISCONNECTED,  ""]
		]
