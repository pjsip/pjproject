#
import inc_const as const

PJSUA = ["--null-audio --max-calls=1 --auto-answer=200 --no-tcp --srtp-secure 0 --use-srtp 1 --srtp-keying=0"]

PJSUA_EXPECTS = [[0, "SRTP uses keying method DTLS-SRTP", ""]]