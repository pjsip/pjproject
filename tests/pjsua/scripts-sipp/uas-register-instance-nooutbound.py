#
# An instance ID with SIP outbound disabled, over TCP: the REGISTER carries
# +sip.instance without reg-id and does not offer outbound. SIPp checks the
# REGISTER, see the XML scenario.
#
PJSUA = ['--null-audio --outb-disable '
         '--instance-id="<urn:uuid:00000000-0000-1000-8000-000a95a0e128>"']

# Add the account only once SIPp listens: a start-up REGISTER would be
# refused, and an "rr" racing it could be lost to PJSIP_EBUSY.
PJSUA_EXPECTS = [[0, "", "+a sip:pjsua@127.0.0.1 "
                         "sip:127.0.0.1:$SIPP_PORT;transport=tcp * pjsua pjsua"]]

SIPP_TRANSPORT = "t1"
