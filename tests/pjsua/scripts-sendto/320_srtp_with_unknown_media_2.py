import inc_sip as sip
import inc_sdp as sdp

sdp = \
"""
v=0
o=- 0 0 IN IP4 127.0.0.1
s=-
c=IN IP4 127.0.0.1
t=0 0
m=xapplicationx 4000 RTP/AVP 100
a=rtpmap:100 myapp/80000
m=audio 5000 RTP/AVP 0
a=crypto:1 aes_cm_128_hmac_sha1_80 inline:WnD7c1ksDGs+dIefCEo8omPg4uO8DYIinNGL5yxQ
"""

pjsua_args = "--null-audio --auto-answer 200 --use-srtp 1 --srtp-secure 0"
extra_headers = ""
include = ["Content-Type: application/sdp",	# response must include SDP
	   "m=xapplicationx 0 RTP/AVP[\\s\\S]+m=audio [1-9]+[0-9]* RTP/AVP[\\s\\S]+a=crypto"
	   ]
exclude = []

sendto_cfg = sip.SendtoCfg("Unknown media and SRTP audio", pjsua_args, sdp, 200,
			   extra_headers=extra_headers,
			   resp_inc=include, resp_exc=exclude) 

