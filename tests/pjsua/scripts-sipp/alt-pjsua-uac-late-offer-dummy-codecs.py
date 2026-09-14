#
# Test driver for alt-pjsua-uac-late-offer.xml
#
# alt_pjsua acts as UAC and sends an SDP-less INVITE to SIPp (UAS). SIPp
# returns an SDP offer in its 200 OK, which alt_pjsua answers in the ACK. SIPp
# then terminates the dialog with BYE. The call should reach STATE_CONFIRMED.
# This scenario tests the handling of late SDP offers with dummy codecs.
#
# Run with alt_pjsua:
#   cd tests/pjsua && python3 run.py \
#       --exe ../../pjsip-apps/src/3rdparty_media_sample/alt_pjsua \
#       mod_sipp.py scripts-sipp/alt-pjsua-uac-late-offer-dummy-codecs.xml
#
import inc_const as const

# No URI in the startup arguments: cli_main() must first configure the telnet
# log redirect. toggle_sdp_offer makes the outgoing INVITE carry no SDP offer.
PJSUA = ["--null-audio --max-calls=1 --no-tcp --dummy-codecs"]

PJSUA_CLI_EXPECTS = [
    [0, "", "toggle_sdp_offer"],
    [0, "", "call new $SIPP_URI"],
    # Leave the call running so SIPp can terminate it with BYE.
    [0, const.STATE_CONFIRMED, ""],
]
