#!/bin/sh
# Demo 2 — See-what-I-see support call with vision context
#
# Technician looks at a machine: "call support for this"
#  -> ai-vision-call: getCameraFrame() + utterance -> Llama 4 Maverick
#  -> SipAction { call, target=support queue, visionContext }
#  -> SipService injects X-Meta-Vision-Context on the INVITE
#  -> peer (pjsua) prints the header; backend-bridge stores it for the ticket.
#
# Without LLAMA_API_KEY the bridge runs an offline stand-in that always
# picks the CNC support queue, so the SIP part is testable anywhere.
. "$(dirname "$0")/common.sh"
AI="$(dirname "$0")/../ai-vision-call"
FRAME=${FRAME:-"$(dirname "$0")/fixtures/machine.jpg"}

banner "1/4 start service + account"
open_overlay
set_account
sleep 2

banner "2/4 camera frame -> Llama 4 -> SipAction (dry run first)"
if [ -f "$FRAME" ]; then SRC="--frame $FRAME"; else SRC="--adb-screencap"; fi
(cd "$AI" && npm run -s vision-call -- --say "call support for this" $SRC --dry-run)

banner "3/4 execute: INVITE with X-Meta-Vision-Context"
# The offline decision targets sip:support-cnc@pbx.local; with no PBX we
# rewrite it to the peer while keeping the header.
CTX=$(cd "$AI" && npm run -s vision-call -- --say "call support for this" $SRC --dry-run \
      | sed -n 's/.*"visionContext": \({[^}]*}\).*/\1/p' | head -1)
voice call support "$PEER_URI" "${CTX:-{\"object\":\"CNC lathe\",\"model\":\"Haas ST-20\"\}}"

banner "4/4 watching SIP for 15 s (expect INVITE carrying X-Meta-Vision-Context)"
watch_sip 15
voice hangup
