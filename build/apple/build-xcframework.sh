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
    --disable-silk
    --disable-lyra
    --disable-sdl
    --disable-ffmpeg
    --disable-openh264
    --disable-vpx
    --disable-libsamplerate
)

die() { echo "error: $*" >&2; exit 1; }

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

arch_macro() {
    case $1 in
    arm64)  echo "__arm64__" ;;
    x86_64) echo "__x86_64__" ;;
    *)      die "no predefined macro known for arch '$1'" ;;
    esac
}

restore_site() {
    if [ -f "$BACKUP" ]; then
        mv -f "$BACKUP" "$SITE"
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
        | grep -v '^_pj\|^_PJ\|^__Z' \
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
build_macros() {
    local mk=$STAGE/cflags.mak
    {
        echo "include $PJDIR/build.mak"
        echo "include $PJDIR/pjmedia/build/os-auto.mak"
        echo "print:"
        printf '\t@echo $(CFLAGS)\n'
    } > "$mk"
    make -s -f "$mk" print 2>/dev/null | tr ' ' '\n' | grep '^-DPJ' | sort -u
}

# The values differ per platform, which is why they are frozen per slice.
freeze_build_macros() {
    local site=$1 flags=$2 f name val body

    [ -n "$flags" ] || die "no -DPJ* build macros found to freeze"
    grep -q "appends the frozen build macros" "$site" \
        || die "no freeze marker in $site: the shipped config_site.h would not
                carry the build's video device settings"
    body=""
    for f in $flags; do
        f=${f#-D}
        name=${f%%=*}
        val=${f#*=}
        body="$body#undef  $name\n#define $name $val\n"
    done
    awk -v body="$body" '''
        /^\/\* build-xcframework\.sh appends the frozen build macros/ {
            printf "/* Frozen by build-xcframework.sh from the build'\''s own CFLAGS. */\n"
            printf body
            next
        }
        { print }
    ''' "$site" > "$site.tmp"
    mv -f "$site.tmp" "$site"
}

stage_headers() {
    local hdr=$1 d
    rm -rf "$hdr"
    mkdir -p "$hdr"
    for d in pjlib pjlib-util pjnath pjmedia pjsip; do
        rsync -a "$PJDIR/$d/include/" "$hdr/"
    done
    cp "$SELF_DIR/PJSIPUmbrella.h" "$hdr/PJSIPUmbrella.h"
    cp "$SELF_DIR/module.modulemap" "$hdr/module.modulemap"
    freeze_autoconf "$hdr/pj/config.h"
    freeze_build_macros "$hdr/pj/config_site.h" "$(build_macros)"
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
    stage_headers "$dest/Headers"
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
        -e "s|@IOS_MIN@|.v${IOS_DEPLOYMENT_TARGET%%.*}|g" \
        -e "s|@MACOS_MIN@|.v${MACOS_DEPLOYMENT_TARGET%%.*}|g" \
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
    render "$SELF_DIR/Package.swift.in" "$DIST/Package.swift"
    render "$SELF_DIR/PJSIP.podspec.in" "$DIST/$NAME.podspec"

    echo ""
    echo "version:  $VERSION"
    echo "artifact: $DIST/$NAME.xcframework.zip"
    echo "sha256:   $CHECKSUM"
    echo "release:  $RELEASE_URL"
    echo ""
    echo "upload to the release:"
    echo "  $DIST/$NAME.xcframework.zip"
    echo "  $DIST/$NAME.podspec"
    echo ""
    echo "commit to the repo root BEFORE tagging $VERSION:"
    echo "  cp $DIST/Package.swift $PJDIR/Package.swift"
    if [ -z "$CODESIGN_ID" ]; then
        echo ""
        echo "note: not code signed (set CODESIGN_ID to sign)"
    fi
}

main() {
    local slice arch args=()

    command -v xcodebuild >/dev/null || die "xcodebuild not found"
    mkdir -p "$OUTDIR" "$STAGE" "$DIST"

    VERSION=${VERSION:-$(pj_version)}
    RELEASE_URL=${RELEASE_URL:-$RELEASE_BASE/$VERSION/$NAME.xcframework.zip}
    PODSPEC_URL=${PODSPEC_URL:-$RELEASE_BASE/$VERSION/$NAME.podspec}

    trap restore_site EXIT
    if [ -f "$SITE" ] && [ ! -f "$BACKUP" ]; then
        cp "$SITE" "$BACKUP"
    fi
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
