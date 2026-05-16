#
# Test that "norefersub" is absent from the Supported header when pjsua is
# started with --no-supported-norefersub (sets pjsua_config.no_refer_sub = PJ_FALSE).
# SIPp (UAC) sends OPTIONS to pjsua (UAS) and verifies the Supported header
# in the 200 OK does NOT contain "norefersub", failing if it does.
#
import inc_const as const

PJSUA = ["--null-audio --no-tcp --no-supported-norefersub"]

PJSUA_EXPECTS = []
