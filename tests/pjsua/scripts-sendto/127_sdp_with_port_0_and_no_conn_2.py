import inc_sip as sip
import inc_sdp as sdp

# All media are disabled and have no connection info
sdp = \
"""
v=0
o=- 0 0 IN IP4 127.0.0.1
s=-
t=0 0
m=audio 0 RTP/AVP 0
"""

pjsua_args = "--null-audio --auto-answer 200"
extra_headers = ""
include = []
exclude = []

sendto_cfg = sip.SendtoCfg("SDP with only disabled media without connection info", pjsua_args, sdp, 488,
			   extra_headers=extra_headers,
			   resp_inc=include, resp_exc=exclude)
