#
# Driver for uas-reinv-hold-401.xml. pjsua dials SIPp and presses 'H';
# SIPp challenges the hold re-INVITE and pjsua has no credentials, so
# the hold fails. SIPp then checks that pjsua answers its sendrecv
# re-INVITE with sendrecv: the failed hold must leave the call unheld.
#
import inc_const as const

PJSUA = ["--null-audio --max-calls=1 --no-tcp $SIPP_URI"]

PJSUA_EXPECTS = [[0, const.STATE_CONFIRMED, "H"]]
