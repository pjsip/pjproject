# Building PJSIP for Android with CMake

The NDK ships a CMake toolchain file, so no wrapper script is needed:

```sh
cmake -B build-android \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-23 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-android -j
```

Repeat per ABI. `arm64-v8a`, `armeabi-v7a`, `x86_64` and `x86` are all built by
CI; each needs its own build directory.

This is an alternative to `./configure-android && make`, not a replacement for
it. Both can build the libraries and the pjsua2 bindings; the autotools build
additionally covers the desktop Java, Python and C# bindings, which CMake does
not.

## Minimum API level

**23**, matching `minSdkVersion` in
`pjsip-apps/src/swig/java/android/pjsua2/build.gradle`. Do not raise the floor
to work around a build error without checking what it is hiding: bionic gates
functions on the API level, so a higher `ANDROID_PLATFORM` can make a
configure check succeed that would fail on a device the library claims to
support. `getifaddrs()`, for example, only exists from API 24, and
`backtrace()` from API 33.

## Backends

These are on by default and are what distinguishes an Android build. Each is
verified by CI, because a missing Find module does not fail a build -- it
silently drops the backend.

| Option | Backend | Needs |
|---|---|---|
| `PJMEDIA_WITH_AUDIODEV_OBOE` | Oboe audio | an Oboe SDK, see below |
| `PJMEDIA_WITH_AUDIODEV_JNI` | Java audio device | -- |
| `PJMEDIA_WITH_VIDEODEV_ANDROID` | Camera capture | the Java classes below |
| `PJMEDIA_WITH_VIDEODEV_OPENGL` | OpenGL ES renderer | NDK `GLESv2`, `EGL` |
| `PJMEDIA_WITH_ANDROID_MEDIACODEC_CODEC` | MediaCodec audio and video codecs | NDK `mediandk` |

### Oboe

Oboe is not bundled. Point the build at an unpacked `oboe-<version>.aar` from
[Google's Maven repository](https://maven.google.com):

```sh
curl -sSLfO https://dl.google.com/dl/android/maven2/com/google/oboe/oboe/1.9.0/oboe-1.9.0.aar
unzip -q oboe-1.9.0.aar -d oboe
cmake -B build-android ... -DOboe_ROOT=$PWD/oboe
```

The AAR's Prefab layout is the same one `./aconfigure --with-oboe=<prefix>`
expects, so a prefix that works for one works for the other. Without it the
backend switches itself off and the build falls back to the JNI audio device.

### Camera capture

`android_dev.c` drives the camera from Java. The classes it calls live in
`pjmedia/src/pjmedia-videodev/android/` and are **not** part of the CMake
build -- compile them into the APK alongside the native library, as
`pjsip-apps/src/swig/java/android` does.

## The pjsua2 bindings and the AAR

`-DPJ_BUILD_SWIG_JAVA=ON` adds a `pjsua2jni` target that runs SWIG over
`pjsip-apps/src/swig/pjsua2.i` and produces the two halves of the binding:

- `libpjsua2.so`, which is what `System.loadLibrary("pjsua2")` opens
- the `org.pjsip.pjsua2` Java sources, under `PJSUA2_JAVA_OUTPUT_DIR`,
  together with the `org.pjsip` camera and audio-device helper classes for
  whichever backends are enabled

It is off by default: SWIG is a build dependency nothing else here needs.

To build the AAR, let Gradle drive CMake rather than running it yourself:

```sh
cd pjsip-apps/src/swig/java/android
./gradlew -PpjBuildWithCMake=true :pjsua2:assembleRelease
```

That configures CMake once per ABI, so one command produces all four. Gradle
packages `libc++_shared.so` itself, which the Makefile workflow has to copy by
hand.

`-PpjBuildWithCMake=true` is required; without it the module behaves as it
always has and takes `libpjsua2.so` and the Java classes prebuilt out of
`pjsua2/src/main`, where `make -C pjsip-apps/src/swig/java` puts them. The two
cannot both be active -- each would supply its own copy of `libc++_shared.so`
and of every generated class -- so pick one and run `./gradlew clean` when
switching.

Prerequisites beyond the NDK and SDK: **SWIG 4.0+**, and **Ninja**, which the
Android Gradle plugin requires for CMake projects. The SDK-managed CMake ships
one, but this project needs CMake 3.28 or newer, above what the SDK provides,
so a CMake from elsewhere on `PATH` is used and Ninja has to be installed
separately.

## Finding dependencies outside the NDK

The NDK toolchain file sets `CMAKE_FIND_ROOT_PATH_MODE_*` to `ONLY`, which
confines `find_package` to the sysroot. A dependency built separately for
Android is therefore invisible until its prefix is added to the search roots,
and `<Package>_ROOT` alone is not enough:

```sh
cmake -B build-android ... \
  -DCMAKE_FIND_ROOT_PATH="$OPENSSL_PREFIX;$OPENH264_PREFIX" \
  -DOPENSSL_ROOT_DIR="$OPENSSL_PREFIX"
```

`Oboe_ROOT` is the exception: `cmake/FindOboe.cmake` lifts the filter itself,
because an unpacked AAR is never inside a sysroot.

## Things the Android build does not have

- **No TLS unless you supply OpenSSL.** Android has no system OpenSSL; build
  one for the NDK and point the search roots at it as above. Otherwise the
  build silently comes out with `PJLIB_WITH_SSL` empty.
- **No desktop Java, Python or C# bindings.** Those are still built by
  `pjsip-apps/src/swig/*/Makefile`; they need a JDK probe, javac steps and
  sample runners that no CMake consumer is asking for.
- **No video codecs beyond MediaCodec.** OpenH264 and VPX are not bundled and
  have no Android packages to find. Build them for the NDK and add their
  prefixes to `CMAKE_FIND_ROOT_PATH`.
