import inc_sip as sip
import inc_sdp as sdp

sdp = \
"""
v=0
o=- 0 0 IN IP4 127.0.0.1
s=-
c=IN IP4 127.0.0.1
t=0 0
m=audio 5000 RTP/AVP 0
m=xapplicationx 4000 RTP/AVP 100
a=rtpmap:100 myapp/80000
"""

pjsua_args = "--null-audio --auto-answer 200"
extra_headers = ""
include = ["Content-Type: application/sdp",	# response must include SDP
	   "m=audio [1-9]+[0-9]* RTP/AVP[\\s\\S]+m=xapplicationx 0 RTP/AVP"
	   ]
exclude = []

sendto_cfg = sip.SendtoCfg("Mixed audio and unknown", pjsua_args, sdp, 200,
			   extra_headers=extra_headers,
			   resp_inc=include, resp_exc=exclude) 

