import inc_sip as sip
import inc_sdp as sdp

sdp = \
"""
v=0
o=- 0 0 IN IP4 127.0.0.1
s=pjmedia
c=IN IP4 127.0.0.1
t=0 0
m=audio 4000 RTP/AVP 0 101
a=rtcp:4382 IN IP4 192.168.0.4
a=ice-ufrag:1234
a=ice-pwd:5678
a=rtpmap:0 PCMU/8000
a=sendrecv
a=rtpmap:101 telephone-event/8000
a=fmtp:101 0-15
a=candidate:XX 1 UDP 1234 127.0.0.1 4000 typ host
a=candidate:XX 2 UDP 1234 127.0.0.1 4000 typ host
"""

args = "--null-audio --use-ice --auto-answer 200 --max-calls 1"
include = ["a=ice-mismatch"]
exclude = []

sendto_cfg = sip.SendtoCfg( "caller sends mismatched offer for comp 2", 
			    pjsua_args=args, sdp=sdp, resp_code=200, 
			    resp_inc=include, resp_exc=exclude)

