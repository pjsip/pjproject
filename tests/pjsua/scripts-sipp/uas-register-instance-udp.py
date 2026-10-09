#
# pjsua never offers SIP outbound over UDP, so +sip.instance appears here
# only because an instance ID is configured. SIPp checks the REGISTER, see
# the XML scenario.
#
PJSUA = ['--null-audio --id=sip:pjsua@127.0.0.1 --registrar=$SIPP_URI '
         '--realm=* --username=pjsua --password=pjsua '
         '--instance-id="<urn:uuid:00000000-0000-1000-8000-000a95a0e128>"']

PJSUA_EXPECTS = []
