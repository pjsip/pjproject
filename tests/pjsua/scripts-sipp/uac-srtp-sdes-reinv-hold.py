#
import inc_const as const

# SDES-keyed SRTP call is put on old-style (RFC 2543, c=0.0.0.0) hold and
# then resumed with a freshly generated SDES key, on the same dialog. This
# exercises pjsua's SRTP session restart path (as UAS) rather than just
# the initial keying negotiation covered by uac-srtp-sdes.py.

PJSUA = ["--null-audio --max-calls=1 --auto-answer=200 --no-tcp --srtp-secure 0 --use-srtp 2 --srtp-keying=0"]

PJSUA_EXPECTS = [[0, "SRTP uses keying method SDES", ""],
                 [0, const.MEDIA_ACTIVE, ""],
                 [0, "SRTP uses keying method SDES", ""],
                 [0, const.MEDIA_HOLD, ""],
                 [0, "SRTP uses keying method SDES", ""],
                 [0, const.MEDIA_ACTIVE, ""]
                ]
