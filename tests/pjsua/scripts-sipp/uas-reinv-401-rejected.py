#
# Driver for uas-reinv-401-rejected.xml. pjsua dials SIPp and presses
# 'v' to send a re-INVITE. SIPp challenges it and rejects the retry with
# credentials. SIPp's re-INVITE that follows must get a 200, not 491.
#
import inc_const as const

PJSUA = ["--null-audio --max-calls=1 --no-tcp --id=sip:a@127.0.0.1 "
         "--username=a --password=wrong --realm=* $SIPP_URI"]

PJSUA_EXPECTS = [[0, const.STATE_CONFIRMED, "v"]]
