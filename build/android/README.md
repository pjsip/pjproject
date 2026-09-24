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
| `PJMEDIA_WITH_VIDEODEV_ANDROID` | Camera capture | the helper classes below |
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

### Java helper classes

The camera backend and Oboe both call up into Java. Those classes live in
`pjmedia/src/pjmedia-videodev/android/` and
`pjmedia/src/pjmedia-audiodev/android/`, and only the ones belonging to an
enabled backend are needed.

Building the bindings takes care of them -- see below -- so there is nothing
to do in the usual case. Building the libraries on their own does not: then
they are yours to compile into the APK alongside the native library, the way
`pjsip-apps/src/swig/java/android` does.

## The pjsua2 bindings and the AAR

`-DPJ_BUILD_SWIG_JAVA=ON` adds a `pjsua2jni` target that runs SWIG over
`pjsip-apps/src/swig/pjsua2.i` and produces the two halves of the binding:

- `libpjsua2.so`, which is what `System.loadLibrary("pjsua2")` opens
- the `org.pjsip.pjsua2` Java sources, under `PJSUA2_JAVA_OUTPUT_DIR`,
  and beside them the `org.pjsip` helper classes for whichever backends are
  enabled -- so the camera and Oboe classes above need no separate handling

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

## Binary distribution (AAR)

`build-aar.sh` produces a **distribution**, not a build of the library for
local use. If you are building PJSIP to develop against it, use the CMake or
autotools instructions above; nothing in this section is needed.

```sh
ANDROID_NDK_ROOT=... ANDROID_HOME=... ./build/android/build-aar.sh
```

Tools: `cmake`, `swig`, `curl`, and a JDK, which supplies both `javac` and the
`jar` that assembles the AAR. Deliberately not `zip`, which is missing from a
fair number of minimal images while the JDK is required anyway.

```
build-aar.sh        builds and packages everything
config_site.h       the configuration the distribution is built with
AndroidManifest.xml the manifest inside the AAR
proguard.txt        consumer keep rules, shipped inside the AAR
pom.xml.in          template for the published POM
```

Output lands in `out-android/dist`: the AAR, sources and javadoc jars, a POM
and `SHA256SUMS`. Pinned sources and per-ABI dependency builds are cached
under `out-android/src` and `out-android/deps`, so a second run only rebuilds
PJSIP itself. The cache is keyed on the source release, the API level and the
NDK revision, so changing any of them rebuilds rather than quietly linking the
previous build's libraries. `out-android/dist` is cleared on each run, so a
version change cannot leave the previous release's files to be checksummed and
published alongside the new one.

The build is out of tree. Nothing in the working tree is touched except
`pjlib/include/pj/config_site.h`, which is backed up and restored on exit. It
refuses to start if the five generated autotools headers are present, for the
shadowing reason described above; `make distclean` removes them.

### One library, no runtime dependencies

The AAR carries exactly one native library per ABI and nothing else:

```
pjsua2-<version>.aar
├── AndroidManifest.xml  minSdkVersion generated from ANDROID_API
├── classes.jar          org.pjsip.pjsua2 + the org.pjsip helpers
├── proguard.txt
├── META-INF/NOTICE      what is bundled, and under what terms
├── META-INF/licenses/   the full text of each
└── jni/<abi>/libpjsua2.so
```

OpenSSL, Opus and Oboe are each built from a pinned release and linked in
statically, and the C++ runtime is static too, so `libpjsua2.so` needs nothing
at run time but Android's own libraries. In particular there is no
`libc++_shared.so` and no `liboboe.so` to collide with an application's own
copy. Oboe has to be built from source for this: its published AAR is a Prefab
module and carries `liboboe.so` nowhere a prebuilt consumer can reach.

**The library exports only the JNI entry points** — `JNI_OnLoad` and the
`Java_org_pjsip_*` functions, 3,091 symbols in place of the 10,972 it would
otherwise expose. The rest is hidden by a version script. This matters because
the bundled third-party code (libsrtp, libyuv, the WebRTC AEC, OpenSSL) is
exactly what a WebRTC-based SDK in the same app also carries, and Android's
linker resolves such a clash by picking one definition for everybody rather
than by failing.

Both properties are checked on the built artifact, not assumed, along with
16&nbsp;KB page alignment -- which the NDK only began defaulting to in r28,
while this build accepts whatever NDK it is pointed at.

The manifest's `minSdkVersion` is generated from `ANDROID_API` rather than
written down, so a build at a higher API cannot ship native code using symbols
the manifest still says are safe to install on 23.

### Licences

An application shipping this artifact redistributes PJSIP and a set of
third-party projects in binary form, several of which require their notice to
be reproduced. The AAR carries `META-INF/NOTICE` and the full text of every
licence beside it; the POM can only name one licence, and names PJSIP's. A
missing licence file stops the build rather than producing an artifact that
cannot lawfully be redistributed.

The texts are collected by sweeping each bundled component's directory rather
than from a list of files, because a bundled project can carry sub-components
under their own terms: `webrtc_aec3` alone compiles Abseil, the Ooura FFT,
RNNoise and PFFFT. A hand-kept list goes stale the next time one is added --
which is exactly how the first version of this shipped one licence for the
whole AEC3 archive. PFFFT states its terms in the head of its source file
rather than in a licence file, so those are lifted out verbatim.

The NOTICE lists what was actually collected instead of restating each
component's terms, because a hand-written summary is how it came to claim a
BSD grant for iLBC without citing one.

iLBC is the one component whose licence does not sit beside its sources.
`third_party/ilbc` is the RFC 3951 reference implementation and states only
*"Copyright (C) The Internet Society (2004). All Rights Reserved"*. That code
was relicensed 3-clause BSD in 2011 after Google acquired Global IP Solutions
and has been distributed on those terms as part of WebRTC ever since, so the
WebRTC licence this tree already carries is what ships for it.

### What the build contains

Upstream defaults apply except where named here or passed as a CMake option in
`build-aar.sh`, which is the single place the configuration is written down.

| | |
|---|---|
| Audio codecs | Opus, G.711, G.722, GSM, Speex, iLBC, L16 |
| Via MediaCodec | the platform's own audio and video codecs |
| Video | MediaCodec, camera capture, OpenGL ES renderer |
| Audio devices | Oboe, and the Java device |
| Echo cancellation | WebRTC AEC3 |
| Resampling | Speex's resampler -- see below |
| Security | SRTP, and TLS over the bundled OpenSSL |

TLS is why OpenSSL is bundled at all: Android has no system OpenSSL and
PJSIP's TLS transport is native, so Conscrypt cannot serve it. Bundling it
means **this artifact has to be rebuilt and republished on OpenSSL security
releases**. The 3.5 LTS branch is pinned to keep that to a minimum.

Because OpenSSL is present, DTLS-SRTP works, and with it WebRTC
interoperability. The Apple distribution has no equivalent — it uses Apple's
Network framework and `transport_srtp_dtls.c` is OpenSSL-only.

### What is excluded, and why

Excluded for licensing rather than for any technical reason:

| Component | Why |
|---|---|
| AMR-NB, AMR-WB (opencore) | patent encumbered |
| G.729 (bcg729) | LGPL; static linking would impose a relink obligation on every consumer |
| libresample | LGPL 2.1, for exactly that reason. The only exclusion here that is not a codec, and the easiest to ship by accident, because it is the upstream default for `PJMEDIA_WITH_RESAMPLE`; the build sets `speex` instead |
| G.722.1 | licence encumbered, and its wrapper is off by default so the omission is easy to miss |
| SILK | disabled at configure time |
| Lyra | disabled at configure time |

Turning these off in `config_site.h` alone is not enough: that only switches
off PJMEDIA's wrapper while `third_party/` still builds and links the library,
so the object code ends up in the artifact regardless. The configure-time
switches in `build-aar.sh` are what keep it out, and `verify_config` asserts
each one before the build starts.

Two things that look like leaks in a symbol dump but are not: Opus contains
its own SILK layer, and pjsua2 exposes a `CodecLyraConfig` class whether or
not Lyra is built. Neither is the corresponding codec.

**AMR is a judgement call, not a clean exclusion.** The `opencore` AMR
implementations are excluded as above, but the MediaCodec wrapper still offers
AMR-NB and AMR-WB through the *platform's* codecs (`OMX.google.amrnb.*`),
which is the upstream default. No AMR code is shipped — the device provides
it — so this is the same position as any Android app that opens a MediaCodec
for `audio/3gpp`. Set `PJMEDIA_HAS_AND_MEDIA_AMRNB` and
`PJMEDIA_HAS_AND_MEDIA_AMRWB` to 0 in `config_site.h` to drop them.

### Publishing

The script stops at artifacts that are ready to sign; it does not upload, and
two one-time account steps have to happen before an upload script would have
anything to be tested against.

**Claim the namespace.** On [central.sonatype.com](https://central.sonatype.com)
-- not the retired oss.sonatype.org -- open *View Namespaces*, *Add Namespace*,
and enter `org.pjsip`. Copy the verification key it assigns, publish it as a
DNS TXT record on the apex of `pjsip.org` (the namespace is checked against
that exact domain), then press *Verify Namespace*. Verification usually
completes in minutes and creates an organization with the verifying account as
its administrator -- so verify from an account the project controls, not a
personal one, or the ability to publish ends up tied to one individual.

**Create a release signing key.** Every deployed file needs a detached GPG
signature beside it, from a key published to a public keyserver. It wants the
same treatment as the account: an organization key with a documented
successor.

What the script already produces, per Central's requirements: the AAR, a
sources jar, a javadoc jar, a POM carrying the name, description, URL, licence,
developer and SCM fields it insists on, and `.md5`/`.sha1` beside every file
with `.sha256`/`.sha512` as extras. The one thing still missing is a `.asc`
signature per file, which needs the release key above.
