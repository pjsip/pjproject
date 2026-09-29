# Glasses-First AI Receptionist & Field Calling — PJSIP POC

Make Meta AI Glasses / VR Glasses a **native SIP endpoint** with PJSUA2:
voice trigger → SIP INVITE, hand gestures for call control, raw PCM tapped
into Llama 4 Maverick for transcription, translation, vision context and
after-call agent work. Architecture: [`docs/glasses-architecture.md`](docs/glasses-architecture.md).

```
glasses-poc/
├── docs/glasses-architecture.md   diagram, call flows, battery/network notes
├── src/glasses-client/            Horizon OS / Android app scaffold (Kotlin)
│   └── .../sip/SipService.kt        foreground service owning the PJSUA2 Endpoint
│   └── .../sip/SipServiceCommand.kt intent facade (pjsip-android pattern)
│   └── .../sip/PcmTapPort.kt        AudioMediaPort tapping raw PCM for AI
│   └── .../voice/                   Muse / Voice SDK intent -> call
│   └── .../gesture/                 Hand Tracking -> answer/mute/transfer
│   └── .../overlay/                 Passthrough overlay (status, caller ID, transcript)
│   └── .../horizon/                 tiny interfaces shaped to v207 SDK APIs
├── src/pjsip-native/              configure-android + SWIG build, glasses config_site.h
├── src/ai-vision-call/            getCameraFrame() + getAudioPCM() -> STT -> Llama 4 -> SipAction
├── src/backend-bridge/            node: SIP <-> WhatsApp Business Calling / PSTN / CRM
└── src/demo-scenarios/            3 runnable demos (adb + pjsua/linphone peer)
```

## Testing without physical glasses

Horizon OS is Android-based, so the whole SIP path runs on a stock Android
emulator. Hand tracking and passthrough are stubbed behind interfaces with
adb-driven debug hooks; the Meta XR Simulator adds real hand-tracking
input when you have the v207 SDK installed.

### 0. Prerequisites

| Tool | Purpose |
|---|---|
| Android Studio + AVD (API 33+, x86_64 or arm64) | Horizon OS stand-in |
| Android NDK r26+ | build libpjsua2.so |
| SWIG 4.x, JDK 17 | PJSUA2 Java bindings |
| Node 20+ | ai-vision-call, backend-bridge |
| A SIP peer: `pjsua` from this repo (built by `make`), or Linphone desktop | remote party |
| Optional: Meta XR Simulator (v207 SDK) | real Hand Tracking / Passthrough input |
| Optional: `LLAMA_API_KEY` | live Llama 4 Maverick; otherwise an offline stand-in runs |

### 1. Build PJSIP + PJSUA2 for the emulator ABI

```sh
export ANDROID_NDK_ROOT=~/Android/Sdk/ndk/26.3.11579264
export OPUS_PREFIX=~/prebuilt/opus-android/x86_64     # optional, see pjsip-native/README.md
src/pjsip-native/build-android.sh x86_64               # arm64-v8a for real glasses
```

This drops `libpjsua2.so` and the `org.pjsip.pjsua2` Java sources into
`pjsip-apps/src/swig/java/android/pjsua2/`.

### 2. Build and install the glasses client

Add the module to the existing Android Gradle project so it can depend on
`:pjsua2`:

```sh
cd pjsip-apps/src/swig/java/android
ln -s ../../../../../glasses-poc/src/glasses-client glasses-client
grep -q glasses-client settings.gradle || echo "include ':glasses-client'" >> settings.gradle
./gradlew :glasses-client:installDebug
adb shell pm grant org.pjsip.glasses android.permission.RECORD_AUDIO
adb shell pm grant org.pjsip.glasses android.permission.POST_NOTIFICATIONS
```

### 3. Start a remote SIP peer on the host

`pjsua` is the most reproducible peer (auto-answer, plays a file, prints
every header including `X-Meta-Vision-Context`):

```sh
cd pjsip-apps/bin
./pjsua-x86_64-unknown-linux-gnu --null-audio --auto-answer 200 --local-port 5080 \
    --id sip:pjsua@10.0.2.2 --log-level 4
```

Or Linphone: Settings → SIP account → *use without account*, listen on
UDP 5062, auto-answer on. From the emulator the host is `10.0.2.2`.

### 4. Run the demos

```sh
cd src/demo-scenarios
./01_handsfree_warehouse_call.sh              # voice intent -> INVITE -> CONFIRMED -> hangup
./02_see_what_i_see_support_call.sh           # camera frame -> Llama -> INVITE + X-Meta-Vision-Context
./03_inbound_call_live_transcription.sh       # peer calls glasses; palm-up answers; live transcript
```

Environment knobs: `PEER_URI` (default `sip:pjsua@10.0.2.2:5080`),
`ACC_REG`/`ACC_USER`/`ACC_PASS` to register with a real PBX,
`TURN_SERVER`/`TURN_USER`/`TURN_PASS` for the edge TURN path,
`FRAME=path.jpg` for demo 2.

Manual equivalents of what the scripts do:

```sh
# voice intent (what Muse would send)
adb shell am broadcast -p org.pjsip.glasses -a org.pjsip.glasses.action.VOICE_INTENT \
    --es intent call --es slot_name warehouse --es slot_uri sip:pjsua@10.0.2.2:5080
# hand gesture (what the Hand Tracking API would produce)
adb shell am broadcast -p org.pjsip.glasses -a org.pjsip.glasses.action.GESTURE --es gesture PALM_UP
# transcript line into the overlay (what ai-vision-call pushes)
adb shell am broadcast -p org.pjsip.glasses -a org.pjsip.glasses.event.TRANSCRIPT --es side far --es text "hello"
```

### 5. AI bridge and backend bridge

```sh
cd src/backend-bridge && npm start                    # :8787, CRM stub + WhatsApp/PSTN routing
cd src/ai-vision-call && npm i && npm run typecheck
npm run vision-call -- --say "call support for this" --frame ../demo-scenarios/fixtures/machine.jpg --dry-run
adb forward tcp:7788 tcp:7788 && npm run transcribe -- --host 127.0.0.1 --port 7788
```

With `LLAMA_API_KEY` set the bridge calls Llama 4 Maverick
(`LLAMA_MODEL`, `LLAMA_API_BASE` override the defaults); without it a
deterministic stand-in produces the same `SipAction` shape so the SIP path
can be tested offline. `STT_URL` points at any Whisper-style HTTP STT.

### With the Meta XR Simulator / real VR Glasses

1. Install the v207 SDK and register the app in the Meta Horizon developer hub.
2. Replace `NoopHandTracking` with an adapter over the Hand Tracking API
   that emits `HandGestureEvent`s; replace `NoCamera` with the Passthrough
   Camera API; render `PassthroughOverlayActivity` as a panel over the
   passthrough layer.
3. Build for `arm64-v8a` and sideload. Everything under `sip/` is unchanged.

## What is real vs. scaffold

- **Real, builds against this repo**: PJSUA2 usage in `sip/` (Endpoint,
  Account, Call, AudioMediaPort, CallOpParam headers, ICE/TURN config,
  Opus/EC settings), `pjsip-native/config_site.h`, `build-android.sh`
  (uses `configure-android` + the SWIG Makefile as-is).
- **Scaffold / interfaces only**: v207 Hand Tracking, Passthrough, Voice
  SDK adapters (`horizon/`), Muse intent contract, WhatsApp Calling API
  call in `backend-bridge`, STT provider, TTS re-injection for translation.
- **Known trade-off**: `PcmTapPort` copies frames through SWIG's
  `ByteVector`. Fine for a POC; production taps in C++ or uses
  `pjmedia_ai_port` with a Llama backend so only text crosses JNI.

## Battery and network (summary; details in the architecture doc)

- TLS + 90 s keepalive, 600 s registration, no UDP NAT keepalive.
- Opus 16 kHz / 16 kbps / complexity 4; sound device auto-closes 2 s after a call.
- ICE + TURN-over-TLS 443 so tether ⇄ Wi-Fi handoffs re-INVITE instead of drop.
- Vision context capped at 1 KB in-band; larger context by `ctx_id` via the bridge.
