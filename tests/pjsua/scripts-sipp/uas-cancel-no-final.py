# $Id$
#
import inc_const as const

PJSUA = ["--null-audio --max-calls=1 --no-tcp $SIPP_URI"]

PJSUA_EXPECTS = [[0, const.STATE_EARLY, "h"]]

PJSUA_CLI_EXPECTS = [[0, const.STATE_EARLY, "call hangup"]]
