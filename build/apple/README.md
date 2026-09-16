# PJSIP XCFramework for Apple platforms

Tooling that builds PJSIP as a binary `PJSIP.xcframework` covering iOS, the iOS
simulator and macOS, together with the SwiftPM and CocoaPods manifests that
distribute it.

This directory produces a **distribution**, not a build of the library for local
use. If you are building PJSIP to develop against it, use `configure-iphone` and
`make` as usual; nothing here is needed.

```
build-xcframework.sh          builds and packages everything
config_site.h                 the configuration the distribution is built with
PJSIPUmbrella.h               umbrella header for the Clang module
module.modulemap              lets Swift and Objective-C say `import PJSIP`
PrivacyInfo.xcprivacy         privacy manifest, copied into every slice
Package.swift.in              template for the SwiftPM manifest
PJSIP.podspec.in              template for the CocoaPods spec
spm/Sources/                  the linker-settings target Package.swift refers to
tests/                        verification, see "Verifying" below
```

## What the build contains

Configuration lives in `config_site.h`. Everything not named there is left at
the upstream default on purpose, so the contract is "upstream defaults plus the
deltas below".

### Transport and security

TLS comes from **Apple's Network framework** (`PJ_SSL_SOCK_IMP_APPLE`), so the
artifact links no OpenSSL at all. Two things follow from that:

- The autoconf probe on Darwin selects `PJ_SSL_SOCK_IMP_DARWIN`, the deprecated
  Secure Transport backend, and there is no `--with-ssl=apple`. The choice is
  made in `config_site.h` and `--disable-darwin-ssl` keeps autoconf from
  overriding it. `os_auto.h` is included before `config_site.h`, hence the
  `#undef` before each `#define`.
- **DTLS-SRTP is unavailable.** `transport_srtp_dtls.c` includes
  `<openssl/ssl.h>` directly and has no Apple equivalent. SDES-SRTP is
  unaffected, but there is no WebRTC interoperability.

The Apple backend also requires `PJ_IOQUEUE_IMP_SELECT`, which is the upstream
default; `config.h` has a compile-time `#error` if the two disagree.

### Codecs

Present: Opus (built from a pinned source release), G.711, G.722, GSM, Speex,
and iLBC via CoreAudio's implementation.

Excluded, for licensing rather than technical reasons:

| Codec | Why |
|---|---|
| AMR-NB, AMR-WB | patent encumbered |
| G.729 (bcg729) | LGPL; static linking would impose a relink obligation on every consumer |
| G.722.1, SILK, Lyra | disabled at configure time |

Disabling these in `config_site.h` alone is not enough — that only switches off
PJMEDIA's wrapper while `third_party/` still builds and links the library, so
the object code ends up in the archive regardless. The configure flags are what
keep it out. G.722.1 in particular has its wrapper off by default, which makes
the omission easy to miss: `--disable-g7221-codec` is what actually keeps
`libg7221codec` out.

### Video

Enabled, with **H.264 through VideoToolbox as the only video codec**. It is
hardware backed and has no third-party dependency; openH264, VPX and FFmpeg are
excluded to keep the artifact self-contained. Capture is AVFoundation, rendering
is Metal everywhere plus OpenGL ES on iOS.

`PJMEDIA_HAS_VIDEO` and `PJMEDIA_HAS_VID_TOOLBOX_CODEC` are never set by
autoconf; both are `config_site.h` decisions.

### Platforms

| Slice | Architectures | Minimum OS |
|---|---|---|
| `ios-arm64` | arm64 | iOS 15.0 |
| `ios-arm64_x86_64-simulator` | arm64, x86_64 | iOS 15.0 |
| `macos-arm64_x86_64` | arm64, x86_64 | macOS 11.0 |

Anything below iOS 14.0 is not honoured for the simulator: clang records a floor
of 14.0 with the current Xcode SDK no matter what is requested, so a lower value
would only make the slice advertise a target its own objects do not meet.

## Two things that are easy to undo by accident

### The headers are an ABI contract

PJSIP's public headers contain inline code and structures whose layout depends
on configuration macros. A consumer compiling against different values than the
binary was built with gets **silent memory corruption, not a link error**.

The distribution therefore ships its own headers with the build's configuration
frozen into them:

- `config_site.h` is copied into every slice.
- `PJ_AUTOCONF` is injected into `pj/config.h`, because the normal build passes
  it on the command line and a framework consumer has no way to do that. Without
  it `config.h` ignores the generated `os_auto.h` and fails to identify the
  target at all.
- Every `-DPJ*` macro the build passes on the command line is appended to the
  shipped `config_site.h`, read back from the build's own `CFLAGS` rather than
  listed by hand. Several public headers branch on these:
  `PJMEDIA_VIDEO_DEV_HAS_IOS_OPENGL` drives
  `PJMEDIA_VIDEO_DEV_HAS_OPENGL_ES`, and `PJMEDIA_HAS_WEBRTC_AEC`,
  `PJMEDIA_HAS_LIBYUV` and `PJMEDIA_RESAMPLE_IMP` are all header-visible.
- Every macro that decides the layout of a public structure is pinned to the
  value the binary was built with. The set is found by scanning the shipped
  headers for macros used as array dimensions — `PJSIP_MAX_MODULE`,
  `PJSIP_MAX_URL_SIZE`, `PJMEDIA_MAX_SDP_FMT` and around sixty more — plus any
  macro those values refer to. These keep their header defaults, so pinning
  them changes nothing except that a consumer's conflicting `-D` is now
  overridden rather than silently obeyed. Without it, an application could
  define one on its own command line and compile against structures of a
  different size than the library contains.

These values differ per platform — iOS gets the OpenGL ES renderer, macOS does
not — so they are frozen per slice, not once.

Consequence for users: **an application cannot override these settings.** One
that needs different values has to build PJSIP from source.

### Non-API symbols are hidden

Before the final archive is written, every defined symbol that is not part of
the public pj API is demoted to a local symbol, via two `ld -r` passes with a
deny list derived from the merged object itself.

Without this the archive exports around 1,200 third-party symbols —
`srtp_cipher_encrypt`, `ARGBDetect`, `WebRtcAec_CreateAec`, `opus_encoder_create`
and so on. libsrtp, libyuv and the WebRTC AEC are exactly what every WebRTC
based SDK bundles, so an application linking this framework alongside one would
fail on duplicate symbols.

Exported symbols are narrowed to the C API, the `pj` namespace (including its
vtables and typeinfo, which consumers subclassing `pj::Account` need), and
`std::` template instantiations, which are weak and meant to be shared.
Everything else is demoted — including pjsua2's own global-namespace helper
classes, and any C++ library bundled later, which would otherwise leak wholesale
simply by being mangled. The deny list is computed, not written down, so a
library added later cannot leak symbols by being forgotten.

`-keep_private_externs` is deliberately not used: it would preserve the hidden
symbols as private externs, which still collide. Debug info is stripped as well,
because DWARF refers to object files by build-time paths that do not exist for
any consumer and the linker warns once per missing file.

## Building

Requires Xcode and `cmake` (cmake is used only for Opus), plus network access on
the first run to fetch the pinned Opus tarball into `$OUTDIR/src`, which later
runs reuse.

```sh
./build/apple/build-xcframework.sh
```

The script is **destructive to the working tree by design**: it runs
`make distclean` and reconfigures once per architecture, five times for a default
build, so a full run takes roughly an hour. An existing
`pjlib/include/pj/config_site.h` is backed up and restored on exit, but
configured state and built objects are not.

Per architecture it distcleans, builds Opus, configures and runs `make lib`,
merges every static library into one relocatable object while demoting non-API
symbols, strips debug info, and stages that architecture's headers with the
build's macros frozen in. Per slice the architectures are lipo'd together and
their header trees reconciled. Finally the slices become an XCFramework, gain the
privacy manifest, and are zipped and checksummed.

| Variable | Default | Effect |
|---|---|---|
| `SLICES` | all three | `ios-device`, `ios-simulator`, `macos` |
| `IOS_DEPLOYMENT_TARGET` | `15.0` | below 14.0 is not honoured for the simulator |
| `MACOS_DEPLOYMENT_TARGET` | `11.0` | |
| `CODESIGN_ID` | unset | signs the framework and verifies the signature |
| `VERSION` | from `version.mak` | podspec version and the URLs in both manifests |
| `RELEASE_BASE` | pjproject releases | base URL the manifests point at |
| `OPUS_PREFIX` | unset | use a prebuilt Opus instead of building one |
| `NO_OPUS` | unset | build without Opus; for iteration only, and no manifests are generated since they would not describe the artifact |
| `OUTDIR` | `out/` | where staging and output go |
| `JOBS` | CPU count | parallel compile jobs |

A partial `SLICES` run still produces a valid XCFramework with fewer slices,
which is useful while iterating (`SLICES=macos` is about ten minutes) and never
appropriate for a release.

Output:

| Path | Contents |
|---|---|
| `out/dist/PJSIP.xcframework` | the framework, with a privacy manifest per slice |
| `out/dist/PJSIP.xcframework.zip` | the release artifact; its sha256 is printed at the end |
| `out/dist/Package.swift` | copy to the repository root before tagging |
| `out/dist/PJSIP.podspec` | upload to the release |
| `out/stage/` | per-slice intermediates, including each architecture's `hidden-symbols.txt` |
| `out/src/` | cached Opus tarball and source |

The build aborts rather than producing a questionable artifact if configure did
not pick up Opus, if a slice's architectures disagree on any generated header
beyond the two known per-architecture ones, if the build macros cannot be frozen
into the shipped headers, or if the Opus tarball does not match its pinned
checksum.

## Verifying

```sh
./build/apple/tests/verify-xcframework.sh              # all tiers
./build/apple/tests/verify-xcframework.sh --tier 1     # seconds
./build/apple/tests/verify-xcframework.sh --device "iPhone 16"
```

It takes a path to a framework, defaulting to `out/dist/PJSIP.xcframework`, and
exits non-zero on any failure. The framework lists are parsed out of
`Package.swift.in` rather than restated, so a framework the shipped manifest
forgets to declare fails here rather than in someone else's project.

1. **Headers, symbols and linking.** Per slice, compiled with no `-D` flags at
   all — which is itself the test that the build's macros were frozen into the
   headers. Asserts the configuration, compiles the Clang module and the public
   headers, checks that a consumer `-D` cannot move the ABI, links the archive
   beside an object defining the same third-party symbols a WebRTC SDK would,
   and confirms nothing outside the pj API is exported.
2. **A SwiftPM consumer that runs**, using the linker settings copied verbatim
   from the shipped manifest.
3. **A real iOS app on a booted simulator.** The only tier that runs the
   framework in an app process: it creates a UDP transport and a TLS transport —
   the check that the Apple backend can actually open a listener rather than
   merely link — starts pjsua, and enumerates audio and video devices. Its
   `Info.plist` carries the microphone and camera usage descriptions, keeping
   that consumer requirement under test. Set `SIP_DOMAIN`, `SIP_USER` and
   `SIP_PASS` (as `SIMCTL_CHILD_*`) to also register against a real server.

## Publishing a release

1. **Set the version** in `version.mak`. It is the source for the podspec
   version and both release URLs.
2. **Run a clean build, signed.**
   ```sh
   rm -rf out/stage out/dist
   CODESIGN_ID="Developer ID Application: ..." ./build/apple/build-xcframework.sh
   ```
3. **Verify** with `./build/apple/tests/verify-xcframework.sh`. All tiers should
   pass before anything is tagged.
4. **Commit the SwiftPM manifest.** The package lives at the root of pjproject,
   so the SwiftPM version *is* the pjproject release tag.
   ```sh
   cp out/dist/Package.swift Package.swift
   git add Package.swift && git commit -m "Apple XCFramework <version>"
   ```
   Commit it, but do not tag yet.

   > This file must be regenerated for **every** future tag, without exception.
   > SwiftPM resolves to the newest tag in a consumer's range and does not fall
   > back: a tag lacking this manifest fails every SwiftPM consumer outright,
   > including ones pinned to an earlier version who changed nothing. A tag
   > carrying a *stale* manifest is quieter and worse — its URL and checksum
   > still agree with each other, so it resolves cleanly and serves the previous
   > release's binaries under the new version number.

5. **Publish the release.** Create the GitHub release as a **draft** with the
   tag name set and the target set to the branch just committed to, upload
   `PJSIP.xcframework.zip` and `PJSIP.podspec` from `out/dist/`, then publish.
   GitHub creates the tag on publish, so tag and assets appear together — any
   gap between them is a window in which SwiftPM consumers resolve the tag and
   fail to download.

   Then verify from outside, which is the only step that exercises the real
   download and checksum:
   ```sh
   mkdir /tmp/pjsip-check && cd /tmp/pjsip-check
   swift package init --type executable
   # add pjproject as a dependency and the PJSIP product, then
   swift build
   ```
6. **Serve the podspec yourself.** There is deliberately no `pod trunk push`.
   CocoaPods Trunk, the central registry that makes a bare `pod 'PJSIP'`
   resolve, becomes permanently read-only on 2 December 2026, so a name claimed
   now would stop accepting new versions within a release or two. The podspec
   uploaded in step 5 is the distribution.

## Consuming it

**Swift Package Manager** — add the pjproject repository URL in Xcode, or:

```swift
.package(url: "https://github.com/pjsip/pjproject", from: "2.17.0")
```

Every required system framework is linked for you. SwiftPM clones the full
pjproject history to read the manifest, so the first resolve is slower than the
download size suggests.

**CocoaPods** — this pod is not in the CocoaPods index; point at the spec
directly. Version ranges do not resolve against a self-hosted spec, so upgrading
means changing the URL. Note that a bare `pod 'pjsip'` pulls an unrelated
third-party pod last updated in 2019.

```ruby
pod 'PJSIP', :podspec =>
  'https://github.com/pjsip/pjproject/releases/download/<version>/PJSIP.podspec'
```

**Manually** — unzip, drag the framework in, set it to **Do Not Embed** (it is a
static library), and add the frameworks yourself: AVFoundation, AudioToolbox,
CoreGraphics, CoreMedia, CoreVideo, Foundation, Metal, MetalKit, Network,
QuartzCore, Security, VideoToolbox, plus OpenGLES and UIKit on iOS or AppKit and
CoreAudio on macOS, and `-lc++`.

On every route, add `NSMicrophoneUsageDescription` and
`NSCameraUsageDescription` to your `Info.plist`, plus the `audio` and `voip`
background modes for calls that continue in the background. Swift and
Objective-C use the C API (`import PJSIP`, `#include <pjsua.h>`); pjsua2 is C++
and is reachable only from Objective-C++ or C++ sources.

## Known limitations

- **No DTLS-SRTP**, so no WebRTC interoperability.
- **H.264 is the only video codec.**
- **No AMR or G.729**, for the licensing reasons above.
- **pjsua2 is not usable from Swift.**
- **Compile-time configuration is fixed**; an app needing different values must
  build from source.
