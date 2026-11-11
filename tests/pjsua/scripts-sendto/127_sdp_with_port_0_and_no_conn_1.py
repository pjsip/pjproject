import inc_sip as sip
import inc_sdp as sdp

# Disabled media without connection info, see also #1625
sdp = \
"""
v=0
o=- 0 0 IN IP4 127.0.0.1
s=-
t=0 0
m=audio 0 RTP/AVP 0
m=audio 5000 RTP/AVP 0
c=IN IP4 127.0.0.1
"""

pjsua_args = "--null-audio --auto-answer 200"
extra_headers = ""
include = ["Content-Type: application/sdp",	# response must include SDP
	   "m=audio 0 RTP/AVP[\\s\\S]+m=audio [1-9]+[0-9]* RTP/AVP"
	   ]
exclude = []

sendto_cfg = sip.SendtoCfg("SDP disabled media without connection info", pjsua_args, sdp, 200,
			   extra_headers=extra_headers,
			   resp_inc=include, resp_exc=exclude)
