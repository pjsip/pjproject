# pjsip-native — PJSIP for Meta glasses

`build-android.sh` runs the repo's own `configure-android` with a glasses
`config_site.h`, then builds the PJSUA2 SWIG bindings. Result is the
`:pjsua2` Gradle module the glasses client depends on.

## Profile

| Setting | Value | Why |
|---|---|---|
| Clock rate | 16 kHz mono | Glasses mic/speaker class; no 48 k resample |
| Codec | Opus 16 kbps VBR, 20 ms, complexity 4, FEC 10 % | ~10 kB/s on tether; survives Wi-Fi loss bursts |
| Echo canceller | WebRTC AEC3 + NS + AGC, tail 200 ms | Open-ear speaker a few cm from mics |
| Transport | TLS, keepalive 90 s | Radio sleeps between keepalives; big INVITEs (vision header) don't fragment |
| Registration | 600 s expiry | Fewer wakeups |
| ICE/TURN | TURN over TLS 443 | Corporate Wi-Fi and carrier NAT on tether |
| Video | off unless `--video` | Only VR Glasses passthrough needs it |
| Calls | max 2 | Glasses UI is one-call-at-a-time; call waiting rejects with 486 |

## Horizon OS notes

- Horizon OS ships an Android 12L+ userland; `APP_PLATFORM=android-29`
  is the floor that still runs on a plain AVD for testing.
- The JNI audio backend (`PJMEDIA_AUDIO_DEV_HAS_ANDROID_JNI`) uses
  AudioRecord/AudioTrack and follows whatever route `AudioManager` set.
  `AudioRouter.kt` sets the route before the first call.
- Hardware AEC: Horizon OS exposes the platform AEC through the
  `VOICE_COMMUNICATION` audio source. PJSUA2 tries that first; AEC3 is
  the software fallback (`PJMEDIA_ECHO_USE_SW_ECHO` not set).
- On-device AI without leaving native code: `pjmedia_ai_port`
  (`pjmedia/include/pjmedia/ai_port.h`) accepts a custom backend
  implementing `pjmedia_ai_backend_op`. `ai_port_openai.c` is the
  reference; a Llama backend would speak the Llama API's realtime
  WebSocket the same way.
