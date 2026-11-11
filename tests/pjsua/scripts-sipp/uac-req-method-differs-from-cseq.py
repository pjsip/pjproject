#
import inc_const as const

# The INVITE is rejected by the call, so the INVITE transaction stays to
# absorb the ACK, and the requests that follow match it.

PJSUA = ["--null-audio --max-calls=1 --auto-answer=486 --no-tcp"]

PJSUA_EXPECTS = [[0, const.STATE_DISCONNECTED, ""]]
