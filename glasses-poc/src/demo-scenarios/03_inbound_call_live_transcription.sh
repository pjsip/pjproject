#!/bin/sh
# Demo 3 — Glasses answer an inbound SIP call, live transcript in passthrough
#
# pjsua/linphone calls the glasses  ->  GlassesAccount.onIncomingCall (180)
#  -> overlay shows "Incoming: <CRM name>"  ->  palm-up gesture answers
#  -> PcmTapPort streams near/far PCM on :7788  ->  ai-vision-call STT
#  -> EVENT_TRANSCRIPT broadcast  ->  overlay appends lines
#  -> pinch mutes, fist hangs up  ->  after-call summary posted to bridge.
. "$(dirname "$0")/common.sh"
AI="$(dirname "$0")/../ai-vision-call"
GLASSES_PORT=${GLASSES_PORT:-5060}

banner "1/5 start service, account, overlay; forward SIP + PCM tap ports"
open_overlay
set_account
adb forward tcp:7788 tcp:7788 >/dev/null
adb forward tcp:$GLASSES_PORT tcp:$GLASSES_PORT >/dev/null   # emulator only
sleep 2

banner "2/5 call the glasses from the peer"
echo "   In another terminal:"
echo "   pjsua --null-audio --play-file demo.wav --local-port 5080 sip:glasses@127.0.0.1:$GLASSES_PORT"
echo "   (or linphone: call sip:glasses@127.0.0.1:$GLASSES_PORT)"
printf "   press enter once it rings... "; read _

banner "3/5 palm-up gesture answers"
gesture PALM_UP
sleep 1

banner "4/5 live transcription for 20 s (overlay updates via EVENT_TRANSCRIPT)"
(cd "$AI" && timeout 20 npm run -s transcribe -- --host 127.0.0.1 --port 7788 --after-call) || true

banner "5/5 pinch = mute, fist = hang up"
gesture PINCH; sleep 1; gesture FIST
watch_sip 5
