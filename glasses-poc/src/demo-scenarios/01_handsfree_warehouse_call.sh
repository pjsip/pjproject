#!/bin/sh
# Demo 1 — Hands-free warehouse call
#
# "Hey Meta, call warehouse"  ->  Muse intent  ->  VoiceTriggerReceiver
#  -> ContactResolver ("warehouse" -> SIP URI)  ->  SipService  ->  INVITE
#
# Peer: `pjsua --null-audio --auto-answer 200 --local-port 5080 --id sip:pjsua@<host>`
# (or linphone on 5062). The resolver's demo directory maps "pjsua" and
# "linphone" to the host machine; "warehouse" maps to a PBX extension, so
# we pass slot_uri to override it when there is no PBX.
. "$(dirname "$0")/common.sh"

banner "1/4 start service + account"
open_overlay
set_account
sleep 2

banner "2/4 voice trigger: 'call warehouse'"
voice call warehouse "$PEER_URI"

banner "3/4 watching SIP for 15 s (expect INVITE -> 200 OK, CONFIRMED)"
watch_sip 15

banner "4/4 voice trigger: 'hang up'"
voice hangup
watch_sip 5
echo "done. Battery note: no wake lock was taken; the foreground service + TLS keepalive is the only background cost."
