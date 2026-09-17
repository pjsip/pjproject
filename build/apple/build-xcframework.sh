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
# Requires Xcode and cmake. The first run downloads the Opus release tarball
# into $OUTDIR/src and checks it against a pinned checksum; later runs reuse
# it.
#
# Builds out of tree, one configure per slice with both architectures in a
# single pass, so nothing in the working tree is disturbed except
# pjlib/include/pj/config_site.h, which is backed up and restored on exit.
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

# The distribution's configuration. Everything CMake has an option for is set
# here rather than in config_site.h, so the build is described in one place.
CMAKE_OPTS=(
    -DPJ_SKIP_EXPERIMENTAL_NOTICE=ON
    -DBUILD_TESTING=OFF
    # the sample-app staging writes into the source tree; a distribution build
    # must not be influenced by, or leave behind, anything there
    -DPJ_IOS_SAMPLE_LIBS=OFF

    # TLS from Apple's Network framework, which also rules out DTLS-SRTP
    -DPJLIB_WITH_SSL=apple
    -DPJLIB_WITH_IOQUEUE=select

    # audio
    -DPJMEDIA_WITH_AUDIODEV_COREAUDIO=ON
    -DPJMEDIA_WITH_SPEEX_AEC=OFF
    -DPJMEDIA_WITH_ILBC_CODEC=ON
    -DPJMEDIA_WITH_L16_CODEC=OFF

    # video: capture from AVFoundation, render with Metal and OpenGL ES,
    # H.264 through VideoToolbox
    -DPJMEDIA_WITH_VIDEO=ON
    -DPJMEDIA_WITH_VIDEODEV_DARWIN=ON
    -DPJMEDIA_WITH_VIDEODEV_METAL=ON
    -DPJMEDIA_WITH_VIDEODEV_OPENGL=ON
    -DPJMEDIA_WITH_VID_TOOLBOX_CODEC=ON

    # codecs excluded for licensing reasons, and backends with external
    # dependencies that would not be self-contained
    -DPJMEDIA_WITH_OPENCORE_AMRNB_CODEC=OFF
    -DPJMEDIA_WITH_OPENCORE_AMRWB_CODEC=OFF
    -DPJMEDIA_WITH_BCG729_CODEC=OFF
    -DPJMEDIA_WITH_G7221_CODEC=OFF
    -DPJMEDIA_WITH_SILK_CODEC=OFF
    -DPJMEDIA_WITH_LYRA_CODEC=OFF
    -DPJMEDIA_WITH_OPEN_H264_CODEC=OFF
    -DPJMEDIA_WITH_VPX_CODEC=OFF
    -DPJMEDIA_WITH_FFMPEG=OFF
    -DPJMEDIA_WITH_VIDEODEV_SDL=OFF
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
    ios-simulator)  echo "arm64;x86_64" ;;
    macos)          echo "arm64;x86_64" ;;
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

# The triple for compile-time probes against a slice; the first architecture
# stands for the slice. The one machine value that does differ across Apple's
# 64-bit little-endian pairs is dispatched on the compiler's own architecture
# macro rather than frozen (see dispatch_machine_header), so the probe cannot
# pin the wrong architecture's.
slice_target() {
    local slice=$1 min arch
    min=$(slice_min_version "$slice")
    arch=$(slice_archs "$slice" | cut -d';' -f1)
    case $slice in
    ios-device)     echo "$arch-apple-ios$min" ;;
    ios-simulator)  echo "$arch-apple-ios$min-simulator" ;;
    macos)          echo "$arch-apple-macos$min" ;;
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

# CMake puts the source include directory ahead of the binary one, so a
# generated header left in the tree -- by an autotools build, or by the iOS
# sample staging -- silently overrides the one this build generates. A
# distribution must not inherit whatever configured the tree last.
STALE_HEADERS="
    pjlib/include/pj/compat/os_auto.h
    pjlib/include/pj/compat/m_auto.h
    pjmedia/include/pjmedia/config_auto.h
    pjmedia/include/pjmedia-codec/config_auto.h
    pjsip/include/pjsip/sip_autoconf.h
"

require_clean_tree() {
    local h found=
    for h in $STALE_HEADERS; do
        [ -f "$PJDIR/$h" ] && found="$found $h"
    done
    [ -z "$found" ] && return 0
    die "generated headers are present in the source tree and would shadow
                this build:$found
                remove them (a 'make distclean' does), then run again"
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
# Opus is not bundled in third_party, so it is built here against exactly the
# same architectures and deployment target as the slice it goes into.
build_opus() {
    local slice=$1 prefix=$2
    local src=$SRCDIR/opus-$OPUS_VERSION
    local bld=$STAGE/$slice/opus-build
    local args=()

    command -v cmake >/dev/null || die "cmake is required"
    fetch_opus

    args=(-DCMAKE_BUILD_TYPE=Release
          -DCMAKE_INSTALL_PREFIX="$prefix"
          -DCMAKE_OSX_ARCHITECTURES="$(slice_archs "$slice")"
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

# Every static library the slice built, plus Opus. CMake writes them beside
# their targets, so the build tree itself is the list.
slice_libs() {
    local bld=$1 prefix=$2
    find "$bld" -name '*.a' -not -path '*/CMakeFiles/*' | sort
    # an if, not a &&: with NO_OPUS there is no prefix, and the test would be
    # the function's exit status
    if [ -n "$prefix" ]; then
        echo "$prefix/lib/libopus.a"
    fi
}

# The release version, straight from the tree rather than a second copy.
pj_version() {
    local mk=$STAGE/version.mak.tmp
    {
        echo "include $PJDIR/version.mak"
        echo "print:"
        printf '\t@echo $(PJ_VERSION)\n'
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
    local dest=$1 slice=$2
    shift 2
    local libs=("$@")
    local sdkv plat min arch thin=()

    sdkv=$(xcrun --sdk "$(slice_sdk "$slice")" --show-sdk-version)
    plat=$(slice_ld_platform "$slice")
    min=$(slice_min_version "$slice")

    # ld -r takes one architecture at a time, so a fat slice is prelinked once
    # per architecture and the results recombined. The build itself still runs
    # only once, which is where the time goes.
    for arch in $(echo "$(slice_archs "$slice")" | tr ';' ' '); do
        local ldflags=(-r -arch "$arch" -platform_version "$plat" "$min" "$sdkv" -all_load)

        ld "${ldflags[@]}" -o "$dest/pass1-$arch.o" "${libs[@]}"
        nm -m "$dest/pass1-$arch.o" \
            | grep -v undefined \
            | grep -E '\) (external|weak external) ' \
            | awk '{print $NF}' \
            | grep -vE "$KEEP_SYMBOLS" \
            | sort -u > "$dest/hidden-symbols-$arch.txt"
        echo "    $arch: hiding $(wc -l < "$dest/hidden-symbols-$arch.txt" | tr -d ' ') non-API symbols"

        ld "${ldflags[@]}" -unexported_symbols_list "$dest/hidden-symbols-$arch.txt" \
           -o "$dest/prelink-$arch.o" "${libs[@]}"
        libtool -static -no_warning_for_no_symbols \
                -o "$dest/libpjproject-$arch.a" "$dest/prelink-$arch.o"

        # DWARF refers to object files by their build-time paths, which do not
        # exist for anyone consuming the framework, and the linker warns once
        # per missing file. The symbol table is kept, so backtraces still name
        # functions.
        strip -S "$dest/libpjproject-$arch.a"
        thin+=("$dest/libpjproject-$arch.a")
        rm -f "$dest/pass1-$arch.o" "$dest/prelink-$arch.o"
    done

    if [ ${#thin[@]} -eq 1 ]; then
        mv "${thin[0]}" "$dest/libpjproject.a"
    else
        lipo -create "${thin[@]}" -output "$dest/libpjproject.a"
        rm -f "${thin[@]}"
    fi
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

# Every -DPJ* macro the build passes on the command line. pjmedia's public
# headers branch on a number of them (PJMEDIA_VIDEO_DEV_HAS_IOS_OPENGL drives
# PJMEDIA_VIDEO_DEV_HAS_OPENGL_ES, PJMEDIA_HAS_WEBRTC_AEC and
# PJMEDIA_RESAMPLE_IMP among others), so a consumer that does not see the same
# values compiles against a different view of the library than was built.
# Every -DPJ* macro the build puts on the compile line. pjmedia's public
# headers branch on a number of them, so a consumer that does not see the same
# values compiles against a different view of the library than was built.
cflags_macros() {
    local bld=$1
    find "$bld" -name flags.make -exec cat {} + 2>/dev/null \
        | tr ' ' '\n' | grep '^-DPJ' | sort -u | sed 's/^-D//' \
        | awk -F= '{ if (NF>1) { v=$2; for(i=3;i<=NF;i++) v=v"="$i; print $1"\t"v } else print $1"\t1" }'
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
    local hdr=$1 sdk=$2 target=$3 cflags=$4
    local probe=$STAGE/abi-probe.c dm=$STAGE/abi-macros.txt
    local dflags=()

    # With the build's own -D flags, otherwise every macro whose value derives
    # from one of them resolves to something the build never used.
    while IFS=$'\t' read -r name value; do
        [ -n "$name" ] && dflags+=("-D$name=$value")
    done < "$cflags"
    printf '%s\n' "${dflags[@]}" > "$STAGE/abi-dflags.txt"

    echo '#include <pjsua.h>' > "$probe"
    # Fatal on purpose: an empty set here would leave the layout macros
    # unfrozen while cflags_macros still produced output, so the non-empty
    # check downstream would pass and ship unpinned headers.
    xcrun -sdk "$sdk" clang -E -dM -target "$target" -I"$hdr" \
        "${dflags[@]}" "$probe" > "$dm" \
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

# A macro is only safe to pin when every guard block that offers it a default
# offers nothing else, and no header branches on whether it is defined at all.
# Those properties have to hold across all the headers, because the same macro
# is often given a default in one header and, in the generated config_auto.h,
# inside a block that defines its siblings too -- pin it and the siblings
# vanish with the block.
candidates, unsafe = set(), set()
for root, _, files in os.walk(hdr):
    for fname in files:
        if not fname.endswith((".h", ".hpp")):
            continue
        text = open(os.path.join(root, fname), errors="ignore").read()
        lines = text.splitlines()

        for i, line in enumerate(lines):
            m = re.match(r"^#\s*ifndef\s+(PJ[A-Z0-9_]*)\s*$", line)
            if not m:
                continue
            name = m.group(1)

            depth, defined_here, j = 1, [], i + 1
            while j < len(lines) and depth:
                l = lines[j]
                if re.match(r"^#\s*if", l):
                    depth += 1
                elif re.match(r"^#\s*endif", l):
                    depth -= 1
                d = re.match(r"^#\s*(?:define|cmakedefine01)\s+([A-Za-z_][A-Za-z0-9_]*)", l)
                if d and depth:
                    defined_here.append(d.group(1))
                j += 1

            if defined_here.count(name) == 1 and len(defined_here) == 1:
                candidates.add(name)
            else:
                unsafe.add(name)
                unsafe.update(defined_here)

        # A bare defined(X) branch changes the moment X is defined at all,
        # whatever its value. "defined(X) && X != 0" is a value test wearing a
        # guard, and is safe to pin, so only flag lines that ask the former
        # without also reading the value.
        for l in lines:
            for name in re.findall(r"defined\s*\(\s*(PJ[A-Z0-9_]*)\s*\)", l):
                rest = re.sub(r"defined\s*\(\s*%s\s*\)" % re.escape(name), "", l)
                if not re.search(r"\b%s\b" % re.escape(name), rest):
                    unsafe.add(name)

        # sizes a public structure
        candidates.update(re.findall(r"\[([A-Z][A-Z0-9_]{3,})\]", text))

wanted = candidates - unsafe

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

# Marker the dispatch below carries, so its loss is detectable downstream.
MACHINE_DISPATCH_MARK="/* PJSIP: machine name chosen per architecture */"

# The compiler macro and the pjlib machine macro for an Apple architecture.
arch_machine_macros() {
    case $1 in
    arm64|arm64e)   echo "__aarch64__ PJ_M_ARM64" ;;
    x86_64)         echo "__x86_64__ PJ_M_X86_64" ;;
    *)              die "no machine macros known for architecture '$1'" ;;
    esac
}

# CMake configures once per slice, so pj_detect_arch() sees only the first of
# CMAKE_OSX_ARCHITECTURES and the m_auto.h it generates names that one
# architecture for the whole fat slice -- an x86_64 half would otherwise
# report arm64 to its own sources and to consumers. Everything else in the
# file is genuinely common to Apple's 64-bit pairs, so instead of configuring
# once per architecture the machine name becomes a dispatch on the compiler's
# own architecture macro. Applied to the build tree before compiling, so the
# library and the headers staged from it cannot disagree.
dispatch_machine_header() {
    local hdr=$1 slice=$2 arch macros kw="#if" body=$1.dispatch

    # The two remaining machine values have to be common to the slice.
    # Endianness already is: on Darwin the header derives it from the
    # compiler's own __BIG_ENDIAN__ rather than from the configure run.
    grep -q "^#define PJ_HAS_FLOATING_POINT 1$" "$hdr" \
        || die "$hdr does not report hardware floating point; the slice's
                architectures no longer share this header"
    grep -q "^#ifdef __BIG_ENDIAN__$" "$hdr" \
        || die "$hdr no longer derives endianness from the compiler; it would
                carry only the first architecture's"

    : > "$body"
    for arch in $(echo "$(slice_archs "$slice")" | tr ';' ' '); do
        macros=$(arch_machine_macros "$arch")
        printf '%s defined(%s)\n#   define %-14s 1\n#   define %-14s "%s"\n' \
               "$kw" "${macros%% *}" "${macros##* }" PJ_M_NAME "$arch" >> "$body"
        kw="#elif"
    done
    printf '#else\n#   error "this PJSIP build was not made for this architecture"\n#endif\n' \
        >> "$body"

    awk -v mark="$MACHINE_DISPATCH_MARK" -v body="$body" '
        /^#define PJ_M_NAME / {
            print mark
            while ((getline line < body) > 0) print line
            close(body)
            next
        }
        { print }
    ' "$hdr" > "$hdr.tmp" && mv "$hdr.tmp" "$hdr"
    rm -f "$body"

    grep -qF "$MACHINE_DISPATCH_MARK" "$hdr" \
        || die "$hdr has no PJ_M_NAME to make per-architecture"
}

# No Opus headers are shipped: no public pjmedia header includes them, and
# an opus/ directory here would shadow the consumer's own Opus headers
# through the framework's header search path.
stage_headers() {
    local hdr=$1 bld=$2 sdk=$3 target=$4 d h
    rm -rf "$hdr"
    mkdir -p "$hdr"
    for d in pjlib pjlib-util pjnath pjmedia pjsip; do
        rsync -a "$PJDIR/$d/include/" "$hdr/"
    done

    # the generated headers live in the build tree, so overlay them
    for h in $STALE_HEADERS; do
        [ -f "$bld/$h" ] || die "the build generated no $h"
        mkdir -p "$hdr/$(dirname "${h#*/include/}")"
        cp "$bld/$h" "$hdr/${h#*/include/}"
    done

    grep -qF "$MACHINE_DISPATCH_MARK" "$hdr/pj/compat/m_auto.h" \
        || die "the staged m_auto.h names a single architecture; the build
                regenerated it after dispatch_machine_header ran"

    cp "$SELF_DIR/PJSIPUmbrella.h" "$hdr/PJSIPUmbrella.h"
    cp "$SELF_DIR/module.modulemap" "$hdr/module.modulemap"
    freeze_autoconf "$hdr/pj/config.h"

    cflags_macros "$bld" > "$STAGE/cflags-macros.txt"
    # cflags first and first-wins on the name, so the value the build actually
    # used beats the header default for any macro that appears in both.
    { cat "$STAGE/cflags-macros.txt"
      abi_macros "$hdr" "$sdk" "$target" "$STAGE/cflags-macros.txt"; } \
        | awk -F'\t' '!seen[$1]++' > "$STAGE/frozen-macros.txt"

    # Pinning a value must never change one. Most configuration macros are
    # "#ifndef X / #define X <default>", but some invert that and others are
    # derived from macros we also pin, so freezing them would reconfigure the
    # library rather than record it. Rather than try to recognise every such
    # idiom, freeze, compare the fully preprocessed macro set against the
    # build's, drop whatever moved, and repeat until it is provably a no-op.
    local pristine=$STAGE/config_site.pristine.h
    local after=$STAGE/abi-macros-after.txt
    local dflags=() changed pass
    cp "$hdr/pj/config_site.h" "$pristine"
    while read -r f; do [ -n "$f" ] && dflags+=("$f"); done < "$STAGE/abi-dflags.txt"

    for pass in 1 2 3 4 5; do
        cp "$pristine" "$hdr/pj/config_site.h"
        freeze_macros "$hdr/pj/config_site.h" "$STAGE/frozen-macros.txt"

        xcrun -sdk "$sdk" clang -E -dM -target "$target" -I"$hdr" \
            "${dflags[@]}" "$STAGE/abi-probe.c" > "$after" 2>/dev/null \
            || die "cannot re-check the staged headers for $target"

        changed=$(diff <(grep '^#define PJ' "$STAGE/abi-macros.txt" | sort) \
                       <(grep '^#define PJ' "$after" | sort) \
                  | grep '^[<>]' | awk '{print $3}' | sort -u || true)
        [ -z "$changed" ] && break

        echo "    pass $pass: not pinning $(echo $changed | wc -w | tr -d ' ')" \
             "macro(s) whose value the pinning would change"
        grep -vF -x -f <(echo "$changed") -w "$STAGE/frozen-macros.txt" \
            > "$STAGE/frozen-macros.next" 2>/dev/null || true
        awk -F'\t' 'NR==FNR{drop[$1];next} !($1 in drop)' \
            <(echo "$changed" | tr ' ' '\n') "$STAGE/frozen-macros.txt" \
            > "$STAGE/frozen-macros.next"
        mv "$STAGE/frozen-macros.next" "$STAGE/frozen-macros.txt"
    done

    [ -z "$changed" ] || die "could not reach a configuration-preserving freeze;
                these still move: $(echo $changed)"
}

# One configure and build per slice, with every architecture of that slice in
# the same pass, so there is no lipo step and no per-architecture header
# reconciliation to do afterwards.
build_slice() {
    local slice=$1
    local dest=$STAGE/$slice
    local bld=$dest/build
    local opus_prefix=$dest/opus
    local opts=("${CMAKE_OPTS[@]}")
    local libs lib

    echo "==> building $slice ($(slice_archs "$slice"))"
    rm -rf "$dest"
    mkdir -p "$dest"

    if [ -n "$NO_OPUS" ]; then
        opts+=(-DPJMEDIA_WITH_OPUS_CODEC=OFF)
        opus_prefix=
    else
        [ -n "$OPUS_PREFIX" ] && opus_prefix=$OPUS_PREFIX || build_opus "$slice" "$opus_prefix"
        # CMAKE_FIND_ROOT_PATH as well as the prefix: an iOS build searches
        # only inside the sysroot by default, so a prefix outside it is
        # ignored and Opus would be silently dropped.
        opts+=(-DPJMEDIA_WITH_OPUS_CODEC=ON
               -DCMAKE_PREFIX_PATH="$opus_prefix"
               -DCMAKE_FIND_ROOT_PATH="$opus_prefix")
    fi

    opts+=(-DCMAKE_OSX_ARCHITECTURES="$(slice_archs "$slice")"
           -DCMAKE_OSX_SYSROOT="$(slice_sdk "$slice")"
           -DCMAKE_OSX_DEPLOYMENT_TARGET="$(slice_min_version "$slice")"
           -DCMAKE_BUILD_TYPE=Release)
    case $slice in
    ios-device|ios-simulator) opts+=(-DCMAKE_SYSTEM_NAME=iOS) ;;
    esac

    cmake -S "$PJDIR" -B "$bld" "${opts[@]}" > "$dest/configure.log" 2>&1 \
        || { sed 's/^/    /' "$dest/configure.log" | tail -15; die "configure failed for $slice"; }

    # The options above are what the artifact claims to be; a silently
    # downgraded one would ship a framework that does not match its manifest.
    local opt
    local checks=(PJLIB_WITH_SSL:apple PJMEDIA_WITH_VIDEO:ON
                  PJMEDIA_WITH_VID_TOOLBOX_CODEC:ON PJMEDIA_WITH_VIDEODEV_DARWIN:ON
                  PJMEDIA_WITH_VIDEODEV_METAL:ON PJMEDIA_WITH_AUDIODEV_COREAUDIO:ON)
    # the OpenGL ES renderer is iOS only; macOS renders with Metal alone
    case $slice in
    ios-device|ios-simulator) checks+=(PJMEDIA_WITH_VIDEODEV_OPENGL:ON) ;;
    esac
    for opt in "${checks[@]}"; do
        grep -q "^${opt%%:*}:[A-Z]*=${opt##*:}$" "$bld/CMakeCache.txt" \
            || die "$slice: ${opt%%:*} did not stay ${opt##*:}; see $dest/configure.log"
    done
    if [ -z "$NO_OPUS" ]; then
        grep -q "^PJMEDIA_WITH_OPUS_CODEC:BOOL=ON$" "$bld/CMakeCache.txt" \
            || die "$slice: configure did not pick up Opus"
    fi

    dispatch_machine_header "$bld/pjlib/include/pj/compat/m_auto.h" "$slice"

    cmake --build "$bld" -j "$JOBS" > "$dest/build.log" 2>&1 \
        || { grep -i "error" "$dest/build.log" | head -10 | sed 's/^/    /'; die "build failed for $slice"; }

    # one array element per line, so a path containing spaces survives
    libs=()
    while IFS= read -r lib; do
        [ -n "$lib" ] && libs+=("$lib")
    done < <(slice_libs "$bld" "$opus_prefix")
    [ ${#libs[@]} -gt 0 ] || die "$slice: the build produced no static library"
    prelink_archive "$dest" "$slice" "${libs[@]}"
    stage_headers "$dest/Headers" "$bld" "$(slice_sdk "$slice")" "$(slice_target "$slice")"
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
    if [ -n "$PARTIAL_BUILD" ] || [ -n "$NO_OPUS" ]; then
        # The manifests describe the full distribution and carry one checksum
        # for it; emitting them here would publish metadata that does not
        # match the artifact.
        rm -f "$DIST/Package.swift" "$DIST/$NAME.podspec"
        echo "note: this is an iteration build, so no manifests were generated"
    else
        render "$SELF_DIR/Package.swift.in" "$DIST/Package.swift"
        render "$SELF_DIR/PJSIP.podspec.in" "$DIST/$NAME.podspec"
    fi

    echo ""
    echo "version:  $VERSION"
    echo "artifact: $DIST/$NAME.xcframework.zip"
    echo "sha256:   $CHECKSUM"
    if [ -f "$DIST/Package.swift" ]; then
        echo "release:  $RELEASE_URL"
        echo ""
        echo "upload to the release:"
        echo "  $DIST/$NAME.xcframework.zip"
        echo "  $DIST/$NAME.podspec"
        echo ""
        echo "commit to the repo root BEFORE tagging $VERSION:"
        echo "  cp $DIST/Package.swift $PJDIR/Package.swift"
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

    require_clean_tree

    for slice in $SLICES; do
        build_slice "$slice"
        args+=(-library "$STAGE/$slice/libpjproject.a"
               -headers "$STAGE/$slice/Headers")
    done

    package "${args[@]}"
}

main "$@"
