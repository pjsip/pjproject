#
# Driver for uas-reinv-hold-401-retried.xml. pjsua dials SIPp and presses
# 'H'; SIPp challenges the hold re-INVITE, pjsua retries with credentials
# and SIPp accepts it. SIPp then checks that pjsua answers its sendrecv
# re-INVITE with sendonly: the call must be on hold.
#
import inc_const as const

PJSUA = ["--null-audio --max-calls=1 --no-tcp --id=sip:a@127.0.0.1 "
         "--username=a --password=a --realm=* $SIPP_URI"]

PJSUA_EXPECTS = [[0, const.STATE_CONFIRMED, "H"]]
