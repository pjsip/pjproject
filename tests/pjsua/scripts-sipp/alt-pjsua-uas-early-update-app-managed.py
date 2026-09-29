import inc_const as const

PJSUA = [
    "--null-audio --max-calls=1 --no-tcp "
    "--id sip:pjsua@localhost:$SIPP_PORT "
    "--acc-media-app-managed --sdp-passthrough --auto-answer=183 "
    "--custom-sdp \"v=0\\r\\no=- 1 1 IN IP4 127.0.0.1\\r\\n"
    "s=-\\r\\nc=IN IP4 127.0.0.1\\r\\nt=0 0\\r\\n"
    "m=audio 4000 RTP/AVP 0\\r\\n\""
]

PJSUA_EXPECTS = [
    [0, const.STATE_EARLY, ""],
    [0, const.STATE_DISCONNECTED, ""],
]
