#
# Driver for uas-reinv-unhold-401.xml. pjsua dials SIPp, presses 'H' and,
# once the call is on hold, 'v'. SIPp challenges the unhold re-INVITE and
# pjsua has no credentials, so the unhold fails. SIPp then checks that
# pjsua answers its sendrecv re-INVITE with sendonly: the call must stay
# on hold.
#
import inc_const as const

PJSUA = ["--null-audio --max-calls=1 --no-tcp $SIPP_URI"]

PJSUA_EXPECTS = [[0, const.STATE_CONFIRMED, "H"],
                 [0, const.MEDIA_HOLD, "v"]]
