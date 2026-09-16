#!/bin/bash
#
# Build PJSIP as a multi-platform XCFramework for binary distribution.
#
# TLS comes from Apple's Network framework, so the artifact links no OpenSSL
# and has no DTLS-SRTP. Codecs with patent or copyleft encumbrance are left
# out; Opus is built from a pinned source release and bundled. See
# config_site.h in this directory for the full contract.
#
# Every symbol that is not part of the public pj API is made local before the
# final archive is written, so that an application linking this framework
# alongside a WebRTC based SDK does not hit duplicate definitions of libsrtp,
# libyuv, the WebRTC AEC or Opus.
#
# Requires Xcode and cmake (cmake is used for Opus only). The first run
# downloads the Opus release tarball into $OUTDIR/src and checks it against a
# pinned checksum; later runs reuse it.
#
# The tree is distcleaned and rebuilt once per architecture, so a full run
# takes roughly an hour. An existing pjlib/include/pj/config_site.h is backed
# up and restored on exit.
#
# Usage:
#   build/apple/build-xcframework.sh
#
# Environment:
#   OUTDIR                    output directory (default: <pjdir>/out)
#   SLICES                    slices to build
#                             (default: "ios-device ios-simulator macos")
#   IOS_DEPLOYMENT_TARGET     default 15.0; below 14.0 is not honoured for the
#                             simulator by the current Xcode SDK
#   MACOS_DEPLOYMENT_TARGET   default 11.0
#   OPUS_PREFIX               use a prebuilt Opus at this prefix instead of
#                             building one
#   NO_OPUS                   set to 1 to build without Opus
#   CODESIGN_ID               sign and verify the framework with this identity
#   JOBS                      parallel jobs (default: number of CPUs)
#   VERSION                   release version (default: PJ_VERSION from
#                             version.mak); sets the podspec version and the
#                             URLs in both generated manifests
#   RELEASE_BASE              base URL the manifests point at
#                             (default: the pjproject GitHub releases URL)
#
set -euo pipefail

SELF_DIR=$(cd "$(dirname "$0")" && pwd)
PJDIR=$(cd "$SELF_DIR/../.." && pwd)

OUTDIR=${OUTDIR:-$PJDIR/out}
STAGE=$OUTDIR/stage
DIST=$OUTDIR/dist
SRCDIR=$OUTDIR/src
NAME=PJSIP

SLICES=${SLICES:-"ios-device ios-simulator macos"}
# Anything below 14.0 is not honoured for the simulator: clang records a floor
# of 14.0 with the current Xcode SDK no matter what is asked for, so a lower
# value only makes the slice advertise a target its objects do not meet.
IOS_DEPLOYMENT_TARGET=${IOS_DEPLOYMENT_TARGET:-15.0}
MACOS_DEPLOYMENT_TARGET=${MACOS_DEPLOYMENT_TARGET:-11.0}
OPUS_PREFIX=${OPUS_PREFIX:-}
NO_OPUS=${NO_OPUS:-}
CODESIGN_ID=${CODESIGN_ID:-}
SITE_EXISTED=
PARTIAL_BUILD=
JOBS=${JOBS:-$(sysctl -n hw.ncpu)}
RELEASE_BASE=${RELEASE_BASE:-https://github.com/pjsip/pjproject/releases/download}

OPUS_VERSION=1.6.1
OPUS_SHA256=6ffcb593207be92584df15b32466ed64bbec99109f007c82205f0194572411a1
OPUS_URL=https://downloads.xiph.org/releases/opus/opus-$OPUS_VERSION.tar.gz

XCODE_DEV=$(xcode-select -p)
SITE=$PJDIR/pjlib/include/pj/config_site.h
BACKUP=$OUTDIR/config_site.h.orig

CONFIGURE_OPTS=(
    --disable-darwin-ssl
    --disable-opencore-amr
    --disable-bcg729
    --disable-g7221-codec
    --disable-silk
    --disable-lyra
    --disable-sdl
    --disable-ffmpeg
    --disable-openh264
    --disable-vpx
    --disable-libsamplerate
)

# Symbols that stay exported: the C API, the pj namespace including its
# vtables and typeinfo, and std:: template instantiations, which are weak and
# meant to be shared. Everything else is demoted, so a C++ library bundled
# later cannot leak its symbols merely by being mangled.
KEEP_SYMBOLS='^_pj|^_PJ|^__Z[A-Za-z]*2pj|^__Z[A-Za-z]*St[0-9]|^__ZSt'

die() { echo "error: $*" >&2; exit 1; }

# Below 14.0 clang records 14.0 in simulator objects regardless, so a lower
# value would only make the generated manifests advertise compatibility the
# artifact does not have.
case $IOS_DEPLOYMENT_TARGET in
[0-9]*)
    if [ "${IOS_DEPLOYMENT_TARGET%%.*}" -lt 14 ]; then
        die "IOS_DEPLOYMENT_TARGET=$IOS_DEPLOYMENT_TARGET is below the 14.0 floor that the current Xcode SDK enforces for the simulator"
    fi
    ;;
*)  die "IOS_DEPLOYMENT_TARGET must be a version number" ;;
esac

slice_archs() {
    case $1 in
    ios-device)     echo "arm64" ;;
    ios-simulator)  echo "arm64 x86_64" ;;
    macos)          echo "arm64 x86_64" ;;
    *)              die "unknown slice '$1'" ;;
    esac
}

slice_sdk() {
    case $1 in
    ios-device)     echo "iphoneos" ;;
    ios-simulator)  echo "iphonesimulator" ;;
    macos)          echo "macosx" ;;
    esac
}

# Platform name as ld(1) spells it in -platform_version.
slice_ld_platform() {
    case $1 in
    ios-device)     echo "ios" ;;
    ios-simulator)  echo "ios-simulator" ;;
    macos)          echo "macos" ;;
    esac
}

slice_min_version() {
    case $1 in
    ios-device|ios-simulator)   echo "$IOS_DEPLOYMENT_TARGET" ;;
    macos)                      echo "$MACOS_DEPLOYMENT_TARGET" ;;
    esac
}

slice_target() {
    local slice=$1 arch=$2 min
    min=$(slice_min_version "$slice")
    case $slice in
    ios-device)     echo "$arch-apple-ios$min" ;;
    ios-simulator)  echo "$arch-apple-ios$min-simulator" ;;
    macos)          echo "$arch-apple-macos$min" ;;
    esac
}

arch_macro() {
    case $1 in
    arm64)  echo "__arm64__" ;;
    x86_64) echo "__x86_64__" ;;
    *)      die "no predefined macro known for arch '$1'" ;;
    esac
}

# The checkout may or may not have had its own config_site.h. Either restore
# it, or remove ours so that an ordinary source build afterwards is not
# silently configured for the distribution.
restore_site() {
    if [ -n "$SITE_EXISTED" ]; then
        [ -f "$BACKUP" ] && mv -f "$BACKUP" "$SITE"
    else
        rm -f "$SITE"
    fi
}

clean_tree() {
    if [ -f "$PJDIR/build.mak" ]; then
        make -C "$PJDIR" distclean >/dev/null 2>&1 || true
    fi
    rm -f "$PJDIR/build.mak" "$PJDIR/config.status"
    mkdir -p "$PJDIR"/pjlib/lib "$PJDIR"/pjlib-util/lib "$PJDIR"/pjnath/lib \
             "$PJDIR"/pjmedia/lib "$PJDIR"/pjsip/lib "$PJDIR"/third_party/lib
}

fetch_opus() {
    local tarball=$SRCDIR/opus-$OPUS_VERSION.tar.gz

    [ -d "$SRCDIR/opus-$OPUS_VERSION" ] && return
    mkdir -p "$SRCDIR"
    if [ ! -f "$tarball" ]; then
        echo "==> fetching $OPUS_URL"
        curl -fSL "$OPUS_URL" -o "$tarball"
    fi
    echo "$OPUS_SHA256  $tarball" | shasum -a 256 -c - >/dev/null \
        || die "opus-$OPUS_VERSION.tar.gz does not match the pinned checksum"
    tar xzf "$tarball" -C "$SRCDIR"
}

# Opus is not bundled in third_party, so it is built here against exactly the
# same deployment target and architecture as the slice it goes into.
build_opus() {
    local slice=$1 arch=$2 prefix=$3
    local src=$SRCDIR/opus-$OPUS_VERSION
    local bld=$STAGE/$slice/$arch/opus-build
    local args=()

    command -v cmake >/dev/null || die "cmake is required to build Opus"
    fetch_opus

    args=(-DCMAKE_BUILD_TYPE=Release
          -DCMAKE_INSTALL_PREFIX="$prefix"
          -DCMAKE_OSX_ARCHITECTURES="$arch"
          -DCMAKE_OSX_DEPLOYMENT_TARGET="$(slice_min_version "$slice")"
          -DCMAKE_OSX_SYSROOT="$(slice_sdk "$slice")"
          -DOPUS_BUILD_SHARED_LIBRARY=OFF
          -DOPUS_BUILD_PROGRAMS=OFF
          -DOPUS_BUILD_TESTING=OFF
          -DBUILD_TESTING=OFF)
    case $slice in
    ios-device|ios-simulator)
        args+=(-DCMAKE_SYSTEM_NAME=iOS)
        ;;
    esac

    rm -rf "$bld" "$prefix"
    cmake -S "$src" -B "$bld" "${args[@]}" >/dev/null
    cmake --build "$bld" --target install -j "$JOBS" >/dev/null
    [ -f "$prefix/lib/libopus.a" ] || die "Opus build produced no static library"
}

# The set of static libraries the build produced, straight from build.mak so
# that enabled and disabled third party libraries are always accounted for.
merged_libs() {
    local mk=$STAGE/libs.mak
    {
        echo "include $PJDIR/build.mak"
        echo "print:"
        printf '\t@echo $(APP_LIBXX_FILES)\n'
    } > "$mk"
    make -s -f "$mk" print
}

# Merge every input archive into one relocatable object, demoting anything
# that is not public pj API to a local symbol, and wrap the result in a static
# library. The deny list is derived from the merged object itself rather than
# from a list of prefixes, so a newly bundled library cannot leak symbols by
# being forgotten here. -keep_private_externs is deliberately not used: it
# would preserve the hidden symbols as private externs, which still collide.
prelink_archive() {
    local dest=$1 slice=$2 arch=$3
    shift 3
    local libs=("$@")
    local ldflags sdkv

    sdkv=$(xcrun --sdk "$(slice_sdk "$slice")" --show-sdk-version)
    ldflags=(-r -arch "$arch"
             -platform_version "$(slice_ld_platform "$slice")" \
                 "$(slice_min_version "$slice")" "$sdkv"
             -all_load)

    ld "${ldflags[@]}" -o "$dest/pass1.o" "${libs[@]}"
    nm -m "$dest/pass1.o" \
        | grep -v undefined \
        | grep -E '\) (external|weak external) ' \
        | awk '{print $NF}' \
        | grep -vE "$KEEP_SYMBOLS" \
        | sort -u > "$dest/hidden-symbols.txt"
    echo "    hiding $(wc -l < "$dest/hidden-symbols.txt" | tr -d ' ') non-API symbols"

    ld "${ldflags[@]}" -unexported_symbols_list "$dest/hidden-symbols.txt" \
       -o "$dest/prelink.o" "${libs[@]}"
    libtool -static -no_warning_for_no_symbols \
            -o "$dest/libpjproject.a" "$dest/prelink.o"

    # Drop DWARF: it refers to object files by their build-time paths, which
    # do not exist for anyone consuming the framework, and the linker warns
    # once per missing file. The symbol table is kept, so backtraces still
    # name functions.
    strip -S "$dest/libpjproject.a"

    rm -f "$dest/pass1.o" "$dest/prelink.o"
}

# The normal build passes -DPJ_AUTOCONF on the command line; a consumer of
# the framework has no way to do that, and without it config.h ignores the
# generated os_auto.h/m_auto.h and fails to identify the target. The shipped
# headers therefore carry the switch themselves.
freeze_autoconf() {
    local hdr=$1
    awk '
        { print }
        /^#define __PJ_CONFIG_H__/ && !done {
            print ""
            print "/* Frozen by build-xcframework.sh: these headers are generated"
            print " * for one target and always use the autoconf configuration."
            print " */"
            print "#ifndef PJ_AUTOCONF"
            print "#  define PJ_AUTOCONF 1"
            print "#endif"
            done = 1
        }
    ' "$hdr" > "$hdr.tmp"
    mv -f "$hdr.tmp" "$hdr"
}

# No Opus headers are shipped: no public pjmedia header includes them, and
# an opus/ directory here would shadow the consumer's own Opus headers
# through the framework's header search path.
# Every -DPJ* macro the build passes on the command line. pjmedia's public
# headers branch on a number of them (PJMEDIA_VIDEO_DEV_HAS_IOS_OPENGL drives
# PJMEDIA_VIDEO_DEV_HAS_OPENGL_ES, PJMEDIA_HAS_WEBRTC_AEC and
# PJMEDIA_RESAMPLE_IMP among others), so a consumer that does not see the same
# values compiles against a different view of the library than was built.
cflags_macros() {
    local mk=$STAGE/cflags.mak
    {
        echo "include $PJDIR/build.mak"
        echo "include $PJDIR/pjmedia/build/os-auto.mak"
        echo "print:"
        printf '\t@echo $(CFLAGS)\n'
    } > "$mk"
    make -s -f "$mk" print 2>/dev/null | tr ' ' '\n' | grep '^-DPJ' | sort -u \
        | sed 's/^-D//' | awk -F= '{ if (NF>1) { v=$2; for(i=3;i<=NF;i++) v=v"="$i; print $1"\t"v } else print $1"\t1" }'
}

# Macros that decide public structure layout. Anything used as an array
# dimension in a public header is ABI visible: a consumer that overrides one
# gets structures of a different size than the binary was built with, and no
# link error. These keep their header defaults, so freezing them changes
# nothing except that a conflicting consumer -D is now overridden rather than
# silently obeyed.
# Macros that decide public structure layout. Two sources, unioned:
# anything pjproject itself declares overridable with the #ifndef/#define
# idiom (which is precisely the set a consumer might try to set, and includes
# switches like PJMEDIA_HAS_RTCP_XR that add conditional struct members), and
# anything used as an array dimension in a public header. Both keep their
# built values, so pinning them changes nothing except that a conflicting
# consumer -D is overridden rather than silently obeyed.
abi_macros() {
    local hdr=$1 sdk=$2 target=$3
    local probe=$STAGE/abi-probe.c dm=$STAGE/abi-macros.txt

    echo '#include <pjsua.h>' > "$probe"
    # Fatal on purpose: an empty set here would leave the layout macros
    # unfrozen while cflags_macros still produced output, so the non-empty
    # check downstream would pass and ship unpinned headers.
    xcrun -sdk "$sdk" clang -E -dM -target "$target" -I"$hdr" "$probe" > "$dm" \
        || die "cannot preprocess the staged headers for $target"

    python3 - "$dm" "$hdr" <<'PYEOF'
import os, re, sys

macros, dm, hdr = {}, sys.argv[1], sys.argv[2]
function_like = set()
for line in open(dm):
    m = re.match(r"#define ([A-Z][A-Z0-9_]*)\(", line)
    if m:
        function_like.add(m.group(1))
        continue
    m = re.match(r"#define ([A-Z][A-Z0-9_]*) (.*)", line.rstrip("\n"))
    if m:
        macros[m.group(1)] = m.group(2)

wanted = set()
for root, _, files in os.walk(hdr):
    for name in files:
        if not name.endswith((".h", ".hpp")):
            continue
        text = open(os.path.join(root, name), errors="ignore").read()
        # declared overridable by the project itself
        for n in re.findall(r"^#\s*ifndef\s+(PJ[A-Z0-9_]*)\s*$", text, re.M):
            if re.search(r"^#\s*define\s+%s\b" % re.escape(n), text, re.M):
                wanted.add(n)
        # sizes a public structure
        wanted.update(re.findall(r"\[([A-Z][A-Z0-9_]{3,})\]", text))

# A pinned value may name another macro; pin those too, or the result can
# still be changed by overriding the one left alone.
seen, queue = set(), [n for n in wanted if n in macros]
while queue:
    name = queue.pop()
    if name in seen:
        continue
    seen.add(name)
    for ref in re.findall(r"[A-Z][A-Z0-9_]{3,}", macros[name]):
        if ref in macros and ref not in seen:
            queue.append(ref)

# A body that invokes a function-like macro cannot be pinned: pre-defining the
# name skips the #ifndef block that also defines its helper, leaving the body
# calling something undefined. Such macros build strings or read runtime
# config and never decide a layout. sizeof() is not a macro call.
def pinnable(body):
    return not re.search(r"\b(?!sizeof\b)[A-Za-z_]\w*\s*\(", body)

out = [n for n in sorted(seen)
       if n not in function_like and macros[n].strip() and pinnable(macros[n])]
if not out:
    sys.exit("no layout macros found in the staged headers")
for name in out:
    print("%s\t%s" % (name, macros[name]))
PYEOF
}

# Writes the collected macros into the shipped config_site.h at its marker.
freeze_macros() {
    local site=$1 macros=$2

    [ -s "$macros" ] || die "no macros collected to freeze into $site"
    grep -q "appends the frozen build macros" "$site" \
        || die "no freeze marker in $site: the shipped config_site.h would not carry the build's configuration"

    awk -F'\t' '
        NR == FNR { name[FNR] = $1; value[FNR] = $2; n = FNR; next }
        /^\/\* build-xcframework\.sh appends the frozen build macros/ {
            print "/* Frozen by build-xcframework.sh from this build: the macros"
            print " * it was compiled with, and every macro that decides the"
            print " * layout of a public structure. Overriding any of these in a"
            print " * consuming project does not change the binary, so they are"
            print " * pinned here rather than left to the consumer. */"
            for (i = 1; i <= n; i++) {
                printf "#undef  %s\n", name[i]
                printf "#define %s %s\n", name[i], value[i]
            }
            next
        }
        { print }
    ' "$macros" "$site" > "$site.tmp"
    mv -f "$site.tmp" "$site"
}

stage_headers() {
    local hdr=$1 sdk=$2 target=$3 d
    rm -rf "$hdr"
    mkdir -p "$hdr"
    for d in pjlib pjlib-util pjnath pjmedia pjsip; do
        rsync -a "$PJDIR/$d/include/" "$hdr/"
    done
    cp "$SELF_DIR/PJSIPUmbrella.h" "$hdr/PJSIPUmbrella.h"
    cp "$SELF_DIR/module.modulemap" "$hdr/module.modulemap"
    freeze_autoconf "$hdr/pj/config.h"

    { cflags_macros; abi_macros "$hdr" "$sdk" "$target"; } \
        | sort -u -t"$(printf '\t')" -k1,1 > "$STAGE/frozen-macros.txt"
    freeze_macros "$hdr/pj/config_site.h" "$STAGE/frozen-macros.txt"
}

build_arch() {
    local slice=$1 arch=$2
    local dest=$STAGE/$slice/$arch
    local opus_prefix=$dest/opus
    local opts=("${CONFIGURE_OPTS[@]}")
    local libs

    echo "==> building $slice / $arch"
    clean_tree
    rm -rf "$dest"
    mkdir -p "$dest"

    if [ -n "$NO_OPUS" ]; then
        opts+=(--disable-opus)
        opus_prefix=
    elif [ -n "$OPUS_PREFIX" ]; then
        opus_prefix=$OPUS_PREFIX
        opts+=(--with-opus="$opus_prefix")
    else
        build_opus "$slice" "$arch" "$opus_prefix"
        opts+=(--with-opus="$opus_prefix")
    fi

    (
        cd "$PJDIR"
        unset CFLAGS LDFLAGS CC CXX CPP AR AR_FLAGS RANLIB
        case $slice in
        ios-device)
            export DEVPATH=$XCODE_DEV/Platforms/iPhoneOS.platform/Developer
            export ARCH="-arch $arch"
            export MIN_IOS="-miphoneos-version-min=$IOS_DEPLOYMENT_TARGET"
            ./configure-iphone "${opts[@]}"
            ;;
        ios-simulator)
            export DEVPATH=$XCODE_DEV/Platforms/iPhoneSimulator.platform/Developer
            export ARCH="-arch $arch"
            export MIN_IOS="-mios-simulator-version-min=$IOS_DEPLOYMENT_TARGET"
            ./configure-iphone "${opts[@]}"
            ;;
        macos)
            export CFLAGS="-O2 -Wno-unused-label -arch $arch -mmacosx-version-min=$MACOS_DEPLOYMENT_TARGET"
            export LDFLAGS="-O2 -arch $arch -mmacosx-version-min=$MACOS_DEPLOYMENT_TARGET"
            ./aconfigure --host="$arch-apple-darwin" "${opts[@]}"
            ;;
        esac
    )

    if [ -z "$NO_OPUS" ]; then
        grep -q "PJMEDIA_HAS_OPUS_CODEC 1" \
            "$PJDIR/pjmedia/include/pjmedia-codec/config_auto.h" \
            || die "configure did not pick up Opus for $slice/$arch"
    fi

    make -C "$PJDIR" lib -j"$JOBS"

    libs=$(merged_libs)
    if [ -n "$opus_prefix" ]; then
        libs="$libs $opus_prefix/lib/libopus.a"
    fi
    prelink_archive "$dest" "$slice" "$arch" $libs
    stage_headers "$dest/Headers" "$(slice_sdk "$slice")" "$(slice_target "$slice" "$arch")"
}

# Headers autoconf generates per architecture. m_auto.h differs in PJ_M_NAME
# (and PJ_HAS_PENTIUM), os_auto.h in PJ_OS_NAME; neither affects ABI, and both
# are resolved at compile time in a fat slice.
PER_ARCH_HEADERS="pj/compat/m_auto.h pj/compat/os_auto.h"

# Merge the per-architecture builds of one slice into a single fat archive
# plus one header tree. Only the headers listed above may differ between
# architectures, and those are turned into dispatching headers; anything else
# differing is a bug in this script's assumptions and must not be papered over.
fuse_slice() {
    local slice=$1
    local dest=$STAGE/$slice
    local archs first a h guard inputs=() excludes=()
    read -r -a archs <<< "$(slice_archs "$slice")"
    first=${archs[0]}

    if [ ${#archs[@]} -eq 1 ]; then
        rm -rf "$dest/Headers"
        mv "$dest/$first/libpjproject.a" "$dest/libpjproject.a"
        mv "$dest/$first/Headers" "$dest/Headers"
        return
    fi

    for a in "${archs[@]}"; do
        inputs+=("$dest/$a/libpjproject.a")
    done
    lipo -create "${inputs[@]}" -output "$dest/libpjproject.a"

    for h in $PER_ARCH_HEADERS; do
        excludes+=(-x "$(basename "$h")")
    done
    for a in "${archs[@]:1}"; do
        diff -r "${excludes[@]}" "$dest/$first/Headers" "$dest/$a/Headers" \
            || die "generated headers differ between $first and $a in $slice"
    done

    # Built before the first architecture's tree is moved into place, since
    # that move is what makes $dest/$first/Headers disappear.
    local tmpd=$dest/.dispatch
    rm -rf "$tmpd"
    mkdir -p "$tmpd"
    for h in $PER_ARCH_HEADERS; do
        {
            echo "/* Generated by build-xcframework.sh for a multi-architecture slice. */"
            for a in "${archs[@]}"; do
                guard=$(arch_macro "$a")
                echo "#if defined($guard)"
                cat "$dest/$a/Headers/$h"
                echo "#endif"
            done
        } > "$tmpd/$(basename "$h")"
    done

    rm -rf "$dest/Headers"
    mv "$dest/$first/Headers" "$dest/Headers"
    for h in $PER_ARCH_HEADERS; do
        mv -f "$tmpd/$(basename "$h")" "$dest/Headers/$h"
    done
    rmdir "$tmpd"
}

pj_version() {
    local mk=$STAGE/version.mak.tmp
    {
        echo "include $PJDIR/version.mak"
        echo "print:"
        printf '\t@echo $(PJ_VERSION)\n'
    } > "$mk"
    make -s -f "$mk" print
}

render() {
    sed -e "s|@URL@|$RELEASE_URL|g" \
        -e "s|@PODSPEC_URL@|$PODSPEC_URL|g" \
        -e "s|@CHECKSUM@|$CHECKSUM|g" \
        -e "s|@VERSION@|$VERSION|g" \
        -e "s|@IOS_MIN@|\"$IOS_DEPLOYMENT_TARGET\"|g" \
        -e "s|@MACOS_MIN@|\"$MACOS_DEPLOYMENT_TARGET\"|g" \
        -e "s|@IOS_MIN_RAW@|$IOS_DEPLOYMENT_TARGET|g" \
        -e "s|@MACOS_MIN_RAW@|$MACOS_DEPLOYMENT_TARGET|g" \
        "$1" > "$2"
}

package() {
    local slice

    rm -rf "$DIST/$NAME.xcframework"
    xcodebuild -create-xcframework "$@" -output "$DIST/$NAME.xcframework"

    # -create-xcframework accepts only -library and -headers, so the privacy
    # manifest is placed alongside each slice's library afterwards.
    for slice in "$DIST/$NAME.xcframework"/*/; do
        [ -f "$slice/libpjproject.a" ] || continue
        cp "$SELF_DIR/PrivacyInfo.xcprivacy" "$slice/PrivacyInfo.xcprivacy"
    done

    if [ -n "$CODESIGN_ID" ]; then
        codesign --timestamp --force --sign "$CODESIGN_ID" \
                 "$DIST/$NAME.xcframework"
        codesign --verify --strict "$DIST/$NAME.xcframework"
    fi

    rm -f "$DIST/$NAME.xcframework.zip"
    ditto -c -k --keepParent "$DIST/$NAME.xcframework" \
          "$DIST/$NAME.xcframework.zip"

    # Identical to `swift package compute-checksum` on the same file.
    CHECKSUM=$(shasum -a 256 "$DIST/$NAME.xcframework.zip" | cut -d' ' -f1)

    rm -rf "$DIST/spm"
    if [ -n "$PARTIAL_BUILD" ]; then
        # The manifests declare both platforms and one checksum for the whole
        # artifact; a subset build would advertise slices that are not there.
        rm -f "$DIST/Package.swift" "$DIST/$NAME.podspec"
        echo "note: SLICES was a subset, so no manifests were generated;" \
             "this artifact is for iteration, not release"
    elif [ -n "$NO_OPUS" ]; then
        # The manifests describe a distribution that includes Opus, and the
        # verifier requires it. Emitting them for a NO_OPUS build would
        # publish metadata that does not match the artifact.
        rm -f "$DIST/Package.swift" "$DIST/$NAME.podspec"
        echo "note: NO_OPUS is set, so no manifests were generated;" \
             "this artifact is for iteration, not release"
    else
        render "$SELF_DIR/Package.swift.in" "$DIST/Package.swift"
        render "$SELF_DIR/PJSIP.podspec.in" "$DIST/$NAME.podspec"
    fi

    echo ""
    echo "version:  $VERSION"
    echo "artifact: $DIST/$NAME.xcframework.zip"
    echo "sha256:   $CHECKSUM"
    echo "release:  $RELEASE_URL"
    echo ""
    if [ -f "$DIST/Package.swift" ]; then
        echo "upload to the release:"
        echo "  $DIST/$NAME.xcframework.zip"
        echo "  $DIST/$NAME.podspec"
        echo ""
        echo "commit to the repo root BEFORE tagging $VERSION:"
        echo "  cp $DIST/Package.swift $PJDIR/Package.swift"
    else
        echo "artifact only; not a release build"
    fi
    if [ -z "$CODESIGN_ID" ]; then
        echo ""
        echo "note: not code signed (set CODESIGN_ID to sign)"
    fi
}

main() {
    local slice arch args=()

    command -v xcodebuild >/dev/null || die "xcodebuild not found"
    mkdir -p "$OUTDIR" "$STAGE" "$DIST"

    for slice in ios-device ios-simulator macos; do
        case " $SLICES " in
        *" $slice "*) ;;
        *) PARTIAL_BUILD=1 ;;
        esac
    done

    VERSION=${VERSION:-$(pj_version)}
    RELEASE_URL=${RELEASE_URL:-$RELEASE_BASE/$VERSION/$NAME.xcframework.zip}
    PODSPEC_URL=${PODSPEC_URL:-$RELEASE_BASE/$VERSION/$NAME.podspec}

    # A backup left behind by an earlier interrupted run is stale: it would be
    # restored over whatever the checkout has now.
    rm -f "$BACKUP"
    if [ -f "$SITE" ]; then
        SITE_EXISTED=1
        cp "$SITE" "$BACKUP"
    fi
    trap restore_site EXIT
    cp "$SELF_DIR/config_site.h" "$SITE"

    for slice in $SLICES; do
        for arch in $(slice_archs "$slice"); do
            build_arch "$slice" "$arch"
        done
        fuse_slice "$slice"
        args+=(-library "$STAGE/$slice/libpjproject.a"
               -headers "$STAGE/$slice/Headers")
    done

    package "${args[@]}"
}

main "$@"
