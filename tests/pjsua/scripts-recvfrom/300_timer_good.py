import inc_sip as sip
import inc_sdp as sdp

# INVITE session using session timer

pjsua = "--null-audio sip:127.0.0.1:$PORT --use-timer 2 --timer-min-se 100 --timer-se 2000 --no-tcp"

req = sip.RecvfromTransaction("INVITE with session timer", 200,
				include=["Session-Expires:\s*2000", "Min-SE:\s*100"], 
				exclude=[],
				resp_hdr=["Session-Expires: 1000;refresher=uac"]
			  	)

recvfrom_cfg = sip.RecvfromCfg("INVITE session using session timer",
			       pjsua, [req])

