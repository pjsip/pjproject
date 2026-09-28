#
# pjsua offers SIP outbound only over TCP/TLS, hence the TCP registrar URI
# and SIPP_TRANSPORT. SIPp checks the REGISTERs, see the XML scenario.
#
PJSUA = ['--null-audio --id=sip:pjsua@127.0.0.1 --registrar="sip:127.0.0.1:$SIPP_PORT;transport=tcp"']

# The REGISTER at start-up is refused, SIPp is not running yet. Register
# again now that it is, rather than wait for the retry.
PJSUA_EXPECTS = [[0, "", "rr"]]

SIPP_TRANSPORT = "t1"
