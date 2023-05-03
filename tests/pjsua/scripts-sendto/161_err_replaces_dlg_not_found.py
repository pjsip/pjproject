import inc_sip as sip
import inc_sdp as sdp

sdp = \
"""
v=0
o=- 0 0 IN IP4 127.0.0.1
s=pjmedia
c=IN IP4 127.0.0.1
t=0 0
m=audio 4000 RTP/AVP 0
"""

pjsua_args = "--null-audio --auto-answer 200"
extra_headers = "Replaces: abcd;from_tag=1\r\n"
include = []
exclude = []

sendto_cfg = sip.SendtoCfg("Replaced dialog not found", pjsua_args, sdp, 481,
			   extra_headers=extra_headers,
			   resp_inc=include, resp_exc=exclude) 

