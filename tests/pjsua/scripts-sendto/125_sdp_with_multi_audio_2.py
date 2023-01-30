import inc_sip as sip
import inc_sdp as sdp

# Multiple m=audio, one of them is bad
sdp = \
"""
v=0
o=- 0 0 IN IP4 127.0.0.1
s=-
c=IN IP4 127.0.0.1
t=0 0
m=audio 4000 UNKNOWN 0
m=audio 5000 RTP/AVP 0
"""

pjsua_args = "--null-audio --auto-answer 200"
extra_headers = ""
include = ["Content-Type: application/sdp",	# response must include SDP
	   "m=audio 0 UNKNOWN[\\s\\S]+m=audio [1-9]+[0-9]* RTP/AVP"
	   ]
exclude = []

sendto_cfg = sip.SendtoCfg("Audio and bad audio", pjsua_args, sdp, 200,
			   extra_headers=extra_headers,
			   resp_inc=include, resp_exc=exclude) 

