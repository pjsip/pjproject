#
import inc_const as const

# A re-INVITE with a new Contact that pjsua accepts updates the remote
# target, a later one that pjsua rejects with 488 must not (RFC 6141
# section 4.6). SIPp checks the Request-URI of the BYE that pjsua sends.

PJSUA = ["--null-audio --max-calls=1 --auto-answer=200 --no-tcp"]

PJSUA_EXPECTS = [[0, const.MEDIA_ACTIVE, ""],
                 [0, "Response msg 488/INVITE", "h"],
                 [0, const.STATE_DISCONNECTED, ""]
                ]

PJSUA_CLI_EXPECTS = [[0, const.MEDIA_ACTIVE, ""],
                     [0, "Response msg 488/INVITE", "call hangup"],
                     [0, const.STATE_DISCONNECTED, ""]
                    ]
