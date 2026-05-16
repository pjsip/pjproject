#
# Test that "norefersub" is present in the Supported header (default behavior).
# pjsua_config.no_refer_sub defaults to PJ_TRUE, so pjsua advertises
# "norefersub" per RFC 4488.  SIPp (UAC) sends OPTIONS to pjsua (UAS) and
# verifies the Supported header in the 200 OK response.
#
import inc_const as const

PJSUA = ["--null-audio --no-tcp"]

PJSUA_EXPECTS = []
