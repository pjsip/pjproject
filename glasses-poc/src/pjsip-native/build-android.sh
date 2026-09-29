#!/bin/sh
#
# Build PJSIP + PJSUA2 Java bindings for Meta glasses (Horizon OS / Android).
#
# Usage:
#   ANDROID_NDK_ROOT=~/android-ndk-r26d \
#   OPUS_PREFIX=~/prebuilt/opus-android \
#   ./build-android.sh [arm64-v8a|x86_64] [--video]
#
# Output:
#   pjsip-apps/src/swig/java/android/pjsua2/src/main/jniLibs/<abi>/libpjsua2.so
#   pjsip-apps/src/swig/java/android/pjsua2/src/main/java/org/pjsip/pjsua2/*.java
#
# Horizon OS is Android-based (arm64-v8a); x86_64 is for the emulator.
# Opus is not bundled with pjproject: point OPUS_PREFIX at a prebuilt
# (include/ + lib/ for the ABI), e.g. from the opus-android or
# ndk-build of xiph/opus.

set -e

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../.." && pwd)
ABI=${1:-arm64-v8a}
WITH_VIDEO=no
[ "$2" = "--video" ] && WITH_VIDEO=yes

: "${ANDROID_NDK_ROOT:?set ANDROID_NDK_ROOT}"
: "${APP_PLATFORM:=android-29}"

echo "== pjsip-native: ABI=$ABI platform=$APP_PLATFORM video=$WITH_VIDEO"

cp "$HERE/config_site.h" "$ROOT/pjlib/include/pj/config_site.h"
if [ "$WITH_VIDEO" = "yes" ]; then
    sed -i.bak '1i #define GLASSES_WITH_VIDEO 1' "$ROOT/pjlib/include/pj/config_site.h"
fi

cd "$ROOT"

CFG_OPTS="--use-ndk-cflags --enable-libwebrtc-aec3 --disable-libwebrtc \
          --disable-speex-aec --disable-speex-codec --disable-ilbc-codec \
          --disable-gsm-codec --disable-g7221-codec --disable-bcg729 \
          --disable-resample --disable-libyuv"
if [ -n "$OPUS_PREFIX" ]; then
    CFG_OPTS="$CFG_OPTS --with-opus=$OPUS_PREFIX"
else
    echo "!! OPUS_PREFIX not set: building without Opus (PCMU/PCMA only)"
fi
[ "$WITH_VIDEO" = "no" ] && CFG_OPTS="$CFG_OPTS --disable-video"

# Oboe (AAudio) backend gives lower latency than the JNI AudioTrack path;
# requires the Oboe library in the NDK sysroot. Off by default.
[ -n "$OBOE_PREFIX" ] && CFG_OPTS="$CFG_OPTS --with-oboe=$OBOE_PREFIX"

TARGET_ABI=$ABI APP_PLATFORM=$APP_PLATFORM ./configure-android $CFG_OPTS

make dep >/dev/null 2>&1 || true
make clean
make -j"$(nproc 2>/dev/null || echo 4)"

# SWIG Java bindings + libpjsua2.so into the android/pjsua2 module.
cd pjsip-apps/src/swig
make clean
make -j"$(nproc 2>/dev/null || echo 4)"

echo "== done: $(ls -la "$ROOT/pjsip-apps/src/swig/java/android/pjsua2/src/main/jniLibs/$ABI/libpjsua2.so")"
echo "   next: cd $ROOT/pjsip-apps/src/swig/java/android && ./gradlew :pjsua2:assembleRelease"
