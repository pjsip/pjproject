#!/bin/sh
# Shared helpers for the demo scripts. Requires: adb, a running emulator/
# glasses with the Glasses SIP app installed, and a SIP peer (pjsua from
# this repo, or linphone). See ../../README.md.

PKG=org.pjsip.glasses
SVC="$PKG/.sip.SipService"
A="$PKG.action"

# Peer defaults: host machine as seen from the Android emulator.
PEER_HOST=${PEER_HOST:-10.0.2.2}
PEER_PORT=${PEER_PORT:-5080}
PEER_URI=${PEER_URI:-sip:pjsua@$PEER_HOST:$PEER_PORT}

# Registrar is optional for the demos: with no PBX we call the peer by IP.
ACC_ID=${ACC_ID:-"Glasses <sip:glasses@$PEER_HOST>"}
ACC_REG=${ACC_REG:-""}

svc() { adb shell am start-foreground-service -n "$SVC" "$@"; }

set_account() {
    if [ -n "$ACC_REG" ]; then
        svc -a "$A.SET_ACCOUNT" --es acc_id_uri "$ACC_ID" --es acc_registrar "$ACC_REG" \
            --es acc_user "${ACC_USER:-glasses}" --es acc_pass "${ACC_PASS:-secret}" \
            ${TURN_SERVER:+--es turn_server "$TURN_SERVER" --es turn_user "$TURN_USER" --es turn_pass "$TURN_PASS"}
    else
        # Local account without registration (Account created, registerOnAdd
        # is a no-op without a registrar): still lets us place/receive calls.
        svc -a "$A.SET_ACCOUNT" --es acc_id_uri "$ACC_ID" --es acc_user glasses --es acc_pass none
    fi
}

voice() {  # voice "call" warehouse [slot_uri] [vision_ctx]
    adb shell am broadcast -p "$PKG" -a "$PKG.action.VOICE_INTENT" --es intent "$1" \
        ${2:+--es slot_name "$2"} ${3:+--es slot_uri "$3"} ${4:+--ez vision true --es vision_ctx "$4"}
}

gesture() { adb shell am broadcast -p "$PKG" -a "$PKG.action.GESTURE" --es gesture "$1"; }

open_overlay() { adb shell am start -n "$PKG/.overlay.PassthroughOverlayActivity" >/dev/null; }

watch_sip() {  # tail PJSIP logs for N seconds
    timeout "${1:-15}" adb logcat -s pjsua2 SipService VoiceTrigger PJSIP 2>/dev/null | \
        grep -E "INVITE|SIP/2.0 (1|2|4|5)|Call state|X-Meta|registered|error" || true
}

banner() { printf '\n== %s ==\n' "$*"; }
