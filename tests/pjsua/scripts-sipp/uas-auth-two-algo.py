# $Id$
#
import inc_const as const

PJSUA = ["--null-audio --max-calls=1 --id=sip:a@localhost --username=a --realm=* --registrar=$SIPP_URI"]

PJSUA_EXPECTS = [[0, "registration success", ""]]
