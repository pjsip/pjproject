#
# Test that "norefersub" is present in the Supported header (default behavior).
# pjsua_config.no_refer_sub defaults to PJ_TRUE, so pjsua advertises
# "norefersub" per RFC 4488.  SIPp verifies the Supported header in the INVITE.
#
import inc_const as const

PJSUA = ["--null-audio --max-calls=1 --no-tcp $SIPP_URI"]

PJSUA_EXPECTS = [
		 [0, const.STATE_CONFIRMED,     ""],
		 [0, const.STATE_DISCONNECTED,  ""]
		]
