#!/bin/bash
#
# Build PJSIP as an Android AAR for binary distribution.
#
# The artifact carries the pjsua2 bindings -- the generated org.pjsip.pjsua2
# classes and one libpjsua2.so per ABI -- which is how Android applications
# consume PJSIP. It carries no headers: the Java API is the interface, and the
# only native symbols the library exports are the JNI entry points.
#
# OpenSSL, Opus and Oboe are each built from a pinned release and linked in
# statically -- none is bundled in third_party, and Android has no system
# OpenSSL while PJSIP's TLS transport is native. With the C++ runtime static
# as well, the AAR carries exactly one native library per ABI and depends on
# nothing at run time but the NDK's own.
#
# Environment:
#   ANDROID_NDK_ROOT          required
#   ANDROID_HOME              Android SDK, for android.jar (or ANDROID_SDK_ROOT)
#   OUTDIR                    build and output tree (default: $PJDIR/out-android)
#   ABIS                      ABIs to build (default: all four)
#   ANDROID_API               minimum API level (default: 23)
#   JOBS                      parallelism (default: CPU count)
#
set -euo pipefail

SELF_DIR=$(cd "$(dirname "$0")" && pwd)
PJDIR=$(cd "$SELF_DIR/../.." && pwd)

OUTDIR=${OUTDIR:-$PJDIR/out-android}
STAGE=$OUTDIR/stage
DIST=$OUTDIR/dist
SRCDIR=$OUTDIR/src
DEPS=$OUTDIR/deps

ABIS=${ABIS:-"arm64-v8a armeabi-v7a x86_64 x86"}
ANDROID_API=${ANDROID_API:-23}
JOBS=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}

ARTIFACT_ID=pjsua2
GROUP_ID=org.pjsip

# OpenSSL 3.5 is the LTS branch, supported to 2030. A bundled TLS stack is a
# standing respin obligation, so the longer-supported branch is the one to be
# on.
OPENSSL_VERSION=3.5.8
OPENSSL_SHA256=a8f84a39918ec6415ce765d9b429d313ba97b8143169c172e734b9514464f5b2
OPENSSL_URL=https://github.com/openssl/openssl/releases/download/openssl-$OPENSSL_VERSION/openssl-$OPENSSL_VERSION.tar.gz

# Same release the Apple distribution pins, so the two ship the same codec.
OPUS_VERSION=1.6.1
OPUS_SHA256=6ffcb593207be92584df15b32466ed64bbec99109f007c82205f0194572411a1
OPUS_URL=https://downloads.xiph.org/releases/opus/opus-$OPUS_VERSION.tar.gz

# Built from source and linked in statically. The published AAR is a Prefab
# module: it carries liboboe.so nowhere but under prefab/, which only a
# consumer doing its own native build can use. Declaring it as a Maven
# dependency would leave an application without the library at run time, so
# the source release is the only thing that makes a self-contained artifact.
OBOE_VERSION=1.9.0
OBOE_SHA256=e030276d25b8bdfaeb04f66646821b1ba4a2b6a580a7e84fb144a478eaecd663
OBOE_URL=https://github.com/google/oboe/archive/refs/tags/$OBOE_VERSION.tar.gz

SITE_EXISTED=

die() { echo "error: $*" >&2; exit 1; }
need() { command -v "$1" >/dev/null || die "$1 is required"; }

# The NDK's own version, so a dependency built with a different one is not
# silently reused.
ndk_revision() {
    sed -n 's/^Pkg.Revision *= *//p' "$ANDROID_NDK_ROOT/source.properties" |
        tr -d '[:space:]'
}

# A dependency prefix is reusable only if it was built from the same source
# release, for the same API level, with the same NDK. Keyed on the archive
# existing alone -- as this was -- a bumped pin or a changed ANDROID_API would
# silently link yesterday's library into today's artifact.
dep_fresh() {
    local abi=$1 name=$2 want=$3
    local stamp=$DEPS/$abi/.stamp-$name
    [ -f "$stamp" ] || return 1
    [ "$(cat "$stamp")" = "$want" ]
}

dep_stamp() {
    local abi=$1 name=$2 want=$3
    mkdir -p "$DEPS/$abi"
    printf '%s\n' "$want" >"$DEPS/$abi/.stamp-$name"
}

dep_key() {
    echo "$1 api=$ANDROID_API ndk=$(ndk_revision)"
}

# macOS ships shasum, most Linux distributions the coreutils *sum tools;
# CI runs on Linux.
hash_of() {
    local alg=$1 file=$2
    case $alg in
    md5)
        if command -v md5sum >/dev/null; then md5sum "$file" | cut -d' ' -f1
        else md5 -q "$file"; fi
        ;;
    *)
        if command -v "${alg}sum" >/dev/null; then
            "${alg}sum" "$file" | cut -d' ' -f1
        else
            shasum -a "${alg#sha}" "$file" | cut -d' ' -f1
        fi
        ;;
    esac
}

sha256_of() { hash_of sha256 "$1"; }

# ##############################################################################
# Preconditions

host_tag() {
    case "$(uname -s)" in
    Darwin) echo darwin-x86_64 ;;
    Linux)  echo linux-x86_64 ;;
    *)      die "unsupported host $(uname -s)" ;;
    esac
}

# CMake puts the source include directory ahead of the binary one, so these
# would shadow the headers this build generates. An autotools build leaves
# them behind by design; `make distclean` removes them.
require_clean_tree() {
    local h found=
    for h in pjlib/include/pj/compat/os_auto.h \
             pjlib/include/pj/compat/m_auto.h \
             pjmedia/include/pjmedia/config_auto.h \
             pjmedia/include/pjmedia-codec/config_auto.h \
             pjsip/include/pjsip/sip_autoconf.h; do
        [ -f "$PJDIR/$h" ] && found="$found  $h"$'\n'
    done
    [ -z "$found" ] || die "generated headers are present in the source tree \
and would shadow this build's:
$found
run 'make distclean' first"
}

# config_site.h is not tracked by git, so a developer's own file has to come
# back afterwards.
restore_site() {
    local site=$PJDIR/pjlib/include/pj/config_site.h
    if [ "$SITE_EXISTED" = yes ]; then
        mv -f "$STAGE/config_site.h.bak" "$site"
    elif [ "$SITE_EXISTED" = no ]; then
        rm -f "$site"
    fi
    SITE_EXISTED=
}

install_site() {
    local site=$PJDIR/pjlib/include/pj/config_site.h
    mkdir -p "$STAGE"
    if [ -f "$site" ]; then
        cp -f "$site" "$STAGE/config_site.h.bak"
        SITE_EXISTED=yes
    else
        SITE_EXISTED=no
    fi
    trap restore_site EXIT
    cp -f "$SELF_DIR/config_site.h" "$site"
}

android_jar() {
    local sdk=${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}
    [ -n "$sdk" ] || die "ANDROID_HOME or ANDROID_SDK_ROOT must be set"
    # Newest platform available; android.jar is backward compatible and only
    # the helper classes compile against it.
    # Sort on the API level pulled out of the directory name, not on a field
    # of the whole path: `sort -t- -k2 -n` over the full path keys on
    # whatever follows the first hyphen anywhere in it, so an SDK at a
    # hyphenated location -- /opt/android-sdk is the common one -- gives every
    # entry the numeric key 0 and the lexical tiebreak then picks android-9
    # over android-35.
    local jar api best=-1 best_jar=
    for jar in "$sdk"/platforms/android-*/android.jar; do
        [ -f "$jar" ] || continue
        api=${jar%/android.jar}
        api=${api##*/android-}
        # Preview platforms are named for a letter rather than a number.
        case $api in
        ''|*[!0-9]*) continue ;;
        esac
        [ "$api" -gt "$best" ] && { best=$api; best_jar=$jar; }
    done
    [ -n "$best_jar" ] || die "no android.jar under $sdk/platforms"
    echo "$best_jar"
}

# ##############################################################################
# Pinned dependencies

fetch() {
    local url=$1 sha=$2 out=$3
    [ -f "$out" ] && return 0
    mkdir -p "$(dirname "$out")"
    echo "==> fetching $url"
    curl -fSL "$url" -o "$out.part"
    if [ -n "$sha" ]; then
        local got
        got=$(sha256_of "$out.part")
        [ "$got" = "$sha" ] \
            || die "$(basename "$out") does not match the pinned checksum
  expected $sha
  got      $got"
    fi
    mv -f "$out.part" "$out"
}

# Unpack a pinned tarball into a source tree known to be pristine.
#
# All three dependencies are configured out of tree, so the source stays
# clean and can be reused -- but only if nothing has ever configured it in
# place. OpenSSL in particular fails with "no such library is built" when it
# finds another configuration's residue beside its sources, so reuse is gated
# on a marker this function writes rather than on the directory existing.
extract_pristine() {
    local tarball=$1 src=$2

    [ -f "$src/.pjsip-pristine" ] && return 0
    rm -rf "$src"
    tar xzf "$tarball" -C "$(dirname "$src")"
    [ -d "$src" ] || die "$(basename "$tarball") did not unpack to $(basename "$src")"
    touch "$src/.pjsip-pristine"
}

openssl_target() {
    case $1 in
    arm64-v8a)   echo android-arm64 ;;
    armeabi-v7a) echo android-arm ;;
    x86_64)      echo android-x86_64 ;;
    x86)         echo android-x86 ;;
    *)           die "unknown ABI $1" ;;
    esac
}

build_openssl() {
    local abi=$1 prefix=$DEPS/$abi
    local src=$SRCDIR/openssl-$OPENSSL_VERSION
    local bld=$STAGE/$abi/openssl
    local key
    key=$(dep_key "openssl-$OPENSSL_VERSION")

    [ -f "$prefix/lib/libssl.a" ] && dep_fresh "$abi" openssl "$key" && return 0

    fetch "$OPENSSL_URL" "$OPENSSL_SHA256" \
          "$SRCDIR/openssl-$OPENSSL_VERSION.tar.gz"
    extract_pristine "$SRCDIR/openssl-$OPENSSL_VERSION.tar.gz" "$src"

    echo "==> building OpenSSL $OPENSSL_VERSION for $abi"
    rm -rf "$bld"
    mkdir -p "$bld"
    # OpenSSL configures in its own build directory but reads the source tree
    # beside it, so it is invoked from there with an explicit --prefix.
    (
        cd "$bld"
        PATH="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/$(host_tag)/bin:$PATH" \
        "$src/Configure" "$(openssl_target "$abi")" \
            -D__ANDROID_API__="$ANDROID_API" \
            no-shared no-tests no-docs no-apps \
            --prefix="$prefix" --libdir=lib
        PATH="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/$(host_tag)/bin:$PATH" \
        make -j "$JOBS"
        PATH="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/$(host_tag)/bin:$PATH" \
        make install_sw
    ) >"$STAGE/$abi-openssl.log" 2>&1 \
        || { tail -20 "$STAGE/$abi-openssl.log"; die "OpenSSL build failed for $abi"; }

    [ -f "$prefix/lib/libssl.a" ] || die "OpenSSL produced no static library for $abi"
    dep_stamp "$abi" openssl "$key"
}

build_opus() {
    local abi=$1 prefix=$DEPS/$abi
    local src=$SRCDIR/opus-$OPUS_VERSION
    local bld=$STAGE/$abi/opus
    local key
    key=$(dep_key "opus-$OPUS_VERSION")

    [ -f "$prefix/lib/libopus.a" ] && dep_fresh "$abi" opus "$key" && return 0

    fetch "$OPUS_URL" "$OPUS_SHA256" "$SRCDIR/opus-$OPUS_VERSION.tar.gz"
    extract_pristine "$SRCDIR/opus-$OPUS_VERSION.tar.gz" "$src"

    echo "==> building Opus $OPUS_VERSION for $abi"
    rm -rf "$bld"
    cmake -S "$src" -B "$bld" \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI="$abi" \
        -DANDROID_PLATFORM="android-$ANDROID_API" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$prefix" \
        -DOPUS_BUILD_SHARED_LIBRARY=OFF \
        -DOPUS_BUILD_PROGRAMS=OFF \
        -DOPUS_BUILD_TESTING=OFF \
        -DBUILD_TESTING=OFF \
        >"$STAGE/$abi-opus.log" 2>&1
    cmake --build "$bld" --target install -j "$JOBS" \
        >>"$STAGE/$abi-opus.log" 2>&1 \
        || { tail -20 "$STAGE/$abi-opus.log"; die "Opus build failed for $abi"; }

    [ -f "$prefix/lib/libopus.a" ] || die "Opus produced no static library for $abi"
    dep_stamp "$abi" opus "$key"
}

build_oboe() {
    local abi=$1 prefix=$DEPS/$abi
    local src=$SRCDIR/oboe-$OBOE_VERSION
    local bld=$STAGE/$abi/oboe
    local key
    key=$(dep_key "oboe-$OBOE_VERSION")

    [ -f "$prefix/lib/$abi/liboboe.a" ] && dep_fresh "$abi" oboe "$key" && return 0

    fetch "$OBOE_URL" "$OBOE_SHA256" "$SRCDIR/oboe-$OBOE_VERSION.tar.gz"
    extract_pristine "$SRCDIR/oboe-$OBOE_VERSION.tar.gz" "$src"

    echo "==> building Oboe $OBOE_VERSION for $abi"
    rm -rf "$bld"
    cmake -S "$src" -B "$bld" \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI="$abi" \
        -DANDROID_PLATFORM="android-$ANDROID_API" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$prefix" \
        -DBUILD_SHARED_LIBS=OFF \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        >"$STAGE/$abi-oboe.log" 2>&1
    cmake --build "$bld" --target install -j "$JOBS" \
        >>"$STAGE/$abi-oboe.log" 2>&1 \
        || { tail -20 "$STAGE/$abi-oboe.log"; die "Oboe build failed for $abi"; }

    [ -f "$prefix/lib/$abi/liboboe.a" ] \
        || die "Oboe produced no static library for $abi"
    dep_stamp "$abi" oboe "$key"
}

# ##############################################################################
# The library

# One ABI: configure, build the JNI library, strip it into place.
#
# The configuration is here rather than in config_site.h wherever CMake has an
# option for it, so that this script is the single place the distribution's
# contents are written down.
build_abi() {
    local abi=$1
    local bld=$STAGE/$abi/pjsip
    local prefix=$DEPS/$abi

    echo "==> building PJSIP for $abi"
    rm -rf "$bld"
    cmake -S "$PJDIR" -B "$bld" \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI="$abi" \
        -DANDROID_PLATFORM="android-$ANDROID_API" \
        -DANDROID_STL=c++_static \
        -DCMAKE_BUILD_TYPE=Release \
        -DPJ_SKIP_EXPERIMENTAL_NOTICE=ON \
        -DBUILD_TESTING=OFF \
        -DPJ_BUILD_SWIG_JAVA=ON \
        -DPJSUA2_JAVA_OUTPUT_DIR="$STAGE/java" \
        \
        -DCMAKE_FIND_ROOT_PATH="$prefix" \
        -DOPENSSL_ROOT_DIR="$prefix" \
        -DOboe_ROOT="$prefix" \
        \
        -DPJMEDIA_WITH_VIDEO=ON \
        -DPJMEDIA_WITH_SRTP=ON \
        -DPJMEDIA_WITH_RESAMPLE=speex \
        -DPJMEDIA_WITH_OPUS_CODEC=ON \
        -DPJMEDIA_WITH_ANDROID_MEDIACODEC_CODEC=ON \
        -DPJMEDIA_WITH_AUDIODEV_OBOE=ON \
        -DPJMEDIA_WITH_VIDEODEV_ANDROID=ON \
        -DPJMEDIA_WITH_VIDEODEV_OPENGL=ON \
        \
        -DPJMEDIA_WITH_OPENCORE_AMRNB_CODEC=OFF \
        -DPJMEDIA_WITH_OPENCORE_AMRWB_CODEC=OFF \
        -DPJMEDIA_WITH_G7221_CODEC=OFF \
        -DPJMEDIA_WITH_SILK_CODEC=OFF \
        -DPJMEDIA_WITH_BCG729_CODEC=OFF \
        -DPJMEDIA_WITH_LYRA_CODEC=OFF \
        -DPJMEDIA_WITH_OPEN_H264_CODEC=OFF \
        -DPJMEDIA_WITH_VPX_CODEC=OFF \
        >"$STAGE/$abi-cmake.log" 2>&1 \
        || { tail -30 "$STAGE/$abi-cmake.log"; die "configure failed for $abi"; }

    verify_config "$abi" "$bld"

    cmake --build "$bld" --target pjsua2jni -j "$JOBS" \
        >"$STAGE/$abi-build.log" 2>&1 \
        || { tail -30 "$STAGE/$abi-build.log"; die "build failed for $abi"; }

    local so=$bld/pjsip-apps/src/swig/libpjsua2.so
    [ -f "$so" ] || die "no libpjsua2.so for $abi"

    mkdir -p "$STAGE/jni/$abi"
    "$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/$(host_tag)/bin/llvm-strip" \
        --strip-unneeded -o "$STAGE/jni/$abi/libpjsua2.so" "$so"

    verify_artifact "$abi"
}

# A dropped backend does not fail a build, and neither does a codec that was
# meant to be excluded quietly coming back, so check what was configured
# rather than trusting the options to have taken.
verify_config() {
    local abi=$1 bld=$2 opt value fail=0

    for opt in PJLIB_WITH_SSL:openssl \
               PJMEDIA_WITH_OPUS_CODEC:ON \
               PJMEDIA_WITH_RESAMPLE:speex \
               PJMEDIA_WITH_SRTP:ON \
               PJMEDIA_WITH_VIDEO:ON \
               PJMEDIA_WITH_AUDIODEV_OBOE:ON \
               PJMEDIA_WITH_VIDEODEV_ANDROID:ON \
               PJMEDIA_WITH_VIDEODEV_OPENGL:ON \
               PJMEDIA_WITH_ANDROID_MEDIACODEC_CODEC:ON \
               PJMEDIA_WITH_ILBC_CODEC:ON; do
        value=$(cmake -L -N "$bld" | sed -n "s/^${opt%%:*}:[^=]*=//p")
        [ "$value" = "${opt##*:}" ] || {
            echo "error: $abi: ${opt%%:*} is '${value:-unset}', expected ${opt##*:}" >&2
            fail=1
        }
    done

    # Excluded for licensing. Disabling the wrapper is not enough on its own
    # -- third_party still builds and links the library -- so these are the
    # configure-time switches that keep the object code out entirely.
    #
    # The resampler is Speex's for the same reason: the bundled libresample
    # is LGPL 2.1, and static linking it would put the relink obligation on
    # every consumer, which is exactly why bcg729 is excluded below.
    for opt in PJMEDIA_WITH_OPENCORE_AMRNB_CODEC \
               PJMEDIA_WITH_OPENCORE_AMRWB_CODEC \
               PJMEDIA_WITH_G7221_CODEC \
               PJMEDIA_WITH_SILK_CODEC \
               PJMEDIA_WITH_BCG729_CODEC \
               PJMEDIA_WITH_LYRA_CODEC; do
        value=$(cmake -L -N "$bld" | sed -n "s/^${opt}:[^=]*=//p")
        case "$value" in
        ON|1|TRUE|YES)
            echo "error: $abi: $opt is enabled but is excluded for licensing" >&2
            fail=1
            ;;
        esac
    done

    [ "$fail" -eq 0 ] || die "configuration check failed for $abi"
}

# What the shipped library must look like, checked on the artifact itself
# rather than on the build that produced it.
#
# Deliberately not a symbol-name search for excluded codecs: those match far
# more than they mean. Opus contains its own SILK layer, and the pjsua2 API
# exposes a CodecLyraConfig whether or not Lyra is built, so grepping for
# "silk" or "lyra" reports both in a build that has neither. The configure
# check in verify_config is what keeps excluded code out; this checks the
# properties a consumer can actually observe.
verify_artifact() {
    local abi=$1
    local so=$STAGE/jni/$abi/libpjsua2.so
    local ndk_bin=$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/$(host_tag)/bin
    local fail=0 n lib

    # The exported set has to be exactly JNI_OnLoad plus the entry points, and
    # all three parts of that are worth asserting separately. Counting "things
    # that are not Java_org_pjsip" and demanding one would also accept a
    # library exporting JNI_OnLoad and nothing else, or one exporting some
    # unrelated symbol alongside the entry points.
    local exported
    exported=$("$ndk_bin/llvm-nm" -D --defined-only "$so" | awk '{print $NF}')

    # A case, not `grep -q`: grep exits on the first match, echo then takes
    # SIGPIPE, and pipefail turns a successful match into a failed pipeline.
    case $'\n'"$exported"$'\n' in
    *$'\n'JNI_OnLoad$'\n'*) ;;
    *)
        echo "error: $abi: JNI_OnLoad is not exported" >&2
        fail=1
        ;;
    esac

    n=$(echo "$exported" | grep -c '^Java_org_pjsip_' || true)
    [ "$n" -ge 1000 ] || {
        echo "error: $abi: only $n JNI entry points exported, expected the full binding" >&2
        fail=1
    }

    local extra
    # JNI_On* rather than JNI_OnLoad, to match what pjsua2jni.map exports:
    # the map is a pattern so that JNI_OnUnload is allowed in a configuration
    # that defines one, and a stricter check here would fail a library the map
    # deliberately permits.
    extra=$(echo "$exported" | grep -v '^Java_org_pjsip_' | grep -vE '^JNI_On' || true)
    [ -z "$extra" ] || {
        echo "error: $abi: unexpected exported symbols:" >&2
        echo "$extra" | sed 's/^/  /' >&2
        fail=1
    }

    # Android 15 requires a 16 KB page size, and Google Play requires it of
    # new uploads. The NDK only began defaulting to it in r28, and this build
    # accepts whatever NDK it is pointed at, so check the property rather than
    # the toolchain version: every loadable segment must be aligned to at
    # least 0x4000.
    local align loads=0
    for align in $("$ndk_bin/llvm-readelf" -l "$so" |
                   awk '$1 == "LOAD" { print $NF }'); do
        loads=$((loads + 1))
        [ "$((align))" -ge 16384 ] || {
            echo "error: $abi: a LOAD segment is aligned to $align, need 0x4000" >&2
            echo "  the NDK in use may predate r28; 16 KB alignment is required" >&2
            fail=1
            break
        }
    done
    # A loop over nothing succeeds. Without this the check reports alignment
    # it never looked at, which is the opposite of its purpose.
    [ "$loads" -gt 0 ] || {
        echo "error: $abi: no LOAD segments found, cannot verify alignment" >&2
        fail=1
    }

    # Every dependency has to be part of Android itself. Anything else --
    # liboboe.so, libc++_shared.so -- is a library the AAR does not ship and
    # the application would fail to load at run time.
    for lib in $("$ndk_bin/llvm-readelf" -d "$so" |
                 sed -n 's/.*Shared library: \[\(.*\)\]/\1/p'); do
        case $lib in
        libc.so|libm.so|libdl.so|liblog.so|libandroid.so|libOpenSLES.so|\
        libmediandk.so|libGLESv2.so|libEGL.so) ;;
        *)
            echo "error: $abi: links $lib, which the AAR does not ship" >&2
            fail=1
            ;;
        esac
    done

    [ "$fail" -eq 0 ] || die "artifact check failed for $abi"
}

# ##############################################################################
# Packaging

# The generated Java is identical for every ABI -- build_abi points them all at
# one directory -- so it is compiled once.
make_classes_jar() {
    local jar
    jar=$(android_jar)
    local classes=$STAGE/classes
    local srcs=$STAGE/sources.txt

    echo "==> compiling $ARTIFACT_ID classes"
    rm -rf "$classes"
    mkdir -p "$classes"
    find "$STAGE/java" -name '*.java' >"$srcs"
    [ -s "$srcs" ] || die "no generated Java sources"

    # android.jar goes on the classpath, not the bootclasspath. It carries no
    # java.lang.invoke.LambdaMetafactory -- Android desugars lambdas and method
    # references at dex time rather than resolving them at run time -- so a
    # bootclasspath build fails on the method reference in PjAudioDevInfo.
    # --release 8 supplies the Java 8 platform API instead, which is what the
    # invokedynamic these compile to expects, and the consumer's D8 desugars
    # them exactly as it would for sources built by the Gradle plugin.
    javac -nowarn --release 8 -encoding UTF-8 \
          -classpath "$jar" \
          -d "$classes" "@$srcs" 2>"$STAGE/javac.log" \
        || { cat "$STAGE/javac.log"; die "javac failed"; }

    (cd "$classes" && jar cf "$STAGE/classes.jar" .)
    (cd "$STAGE/java" && jar cf "$STAGE/$ARTIFACT_ID-sources.jar" .)
}

make_javadoc_jar() {
    local jar
    jar=$(android_jar)
    echo "==> generating javadoc"
    rm -rf "$STAGE/javadoc"
    # SWIG carries the Doxygen comments across, and they do not all survive as
    # valid javadoc; the jar is a Maven Central requirement, not documentation
    # anybody reads from here, so doclint is off and failures are not fatal.
    javadoc -quiet -Xdoclint:none -encoding UTF-8 \
            --release 8 -classpath "$jar" \
            -d "$STAGE/javadoc" \
            $(find "$STAGE/java" -name '*.java') \
            >"$STAGE/javadoc.log" 2>&1 || true
    [ -f "$STAGE/javadoc/index.html" ] \
        || echo "warning: javadoc produced no index; jar will be near-empty" >&2
    (cd "$STAGE/javadoc" && jar cf "$STAGE/$ARTIFACT_ID-javadoc.jar" .)
}

pj_version() {
    local mk=$STAGE/version.mak.tmp
    {
        echo "include $PJDIR/version.mak"
        echo "print:"
        printf '\t@echo $(PJ_VERSION)\n'
    } >"$mk"
    make -s -f "$mk" print
}

# Every licence covering something linked into the shipped library.
#
# An application shipping this artifact redistributes PJSIP and a set of
# third-party projects in binary form, several of which require their notice
# to be reproduced. The POM can only name one licence, and names PJSIP's, so
# the texts travel inside the AAR.
#
# Collected by sweeping each component's directory rather than from a list of
# files, because a bundled project can carry sub-components with their own
# terms: webrtc_aec3 alone compiles Abseil, the Ooura FFT, RNNoise weights and
# PFFFT, each licensed separately. A hand-kept list silently goes stale the
# next time one of those is added.
stage_licenses() {
    local root=$1
    local dir=$root/META-INF/licenses
    local comp name src rel dest found

    mkdir -p "$dir"

    # Named files, for components where a whole source tree is unpacked but
    # only the library is linked. Sweeping those pulls in build tooling and
    # sample apps -- OpenSSL's tree alone carries a Perl module's licence and
    # a copyright.pm -- and listing things that are not in the binary is its
    # own kind of inaccuracy.
    # iLBC is the one entry whose licence does not live beside its sources.
    # third_party/ilbc is the RFC 3951 reference code and states only
    # "Copyright (C) The Internet Society (2004). All Rights Reserved". That
    # implementation was relicensed 3-clause BSD in 2011, after Google
    # acquired Global IP Solutions, and is distributed on those terms as part
    # of WebRTC -- whose licence this tree already carries, and which is what
    # ships for it. NOTICE records the provenance.
    local files="
pjsip|$PJDIR/COPYING
openssl|$SRCDIR/openssl-$OPENSSL_VERSION/LICENSE.txt
opus|$SRCDIR/opus-$OPUS_VERSION/COPYING
oboe|$SRCDIR/oboe-$OBOE_VERSION/LICENSE
ilbc|$PJDIR/third_party/webrtc/LICENSE
"
    for comp in $files; do
        name=${comp%%|*}
        src=${comp#*|}
        [ -f "$src" ] || die "no licence file for $name at $src"
        cp "$src" "$dir/LICENSE.$name"
    done

    # Swept directories, for the bundled trees: there the tree is what gets
    # compiled, and sub-components carry their own terms.
    local roots="
libsrtp|$PJDIR/third_party/srtp
libyuv|$PJDIR/third_party/yuv
webrtc|$PJDIR/third_party/webrtc
webrtc_aec3|$PJDIR/third_party/webrtc_aec3
speex|$PJDIR/third_party/speex
gsm|$PJDIR/third_party/gsm
"

    for comp in $roots; do
        name=${comp%%|*}
        src=${comp#*|}
        [ -d "$src" ] || die "no licence source for $name at $src"

        found=0
        while IFS= read -r rel; do
            [ -n "$rel" ] || continue
            # Name each by where it sits, so a sub-component's licence is
            # distinguishable from its parent's.
            dest=$(printf '%s' "${rel%/*}" | tr '/' '-')
            case $rel in
            */*) dest="LICENSE.$name-${dest##*-}" ;;
            *)   dest="LICENSE.$name" ;;
            esac
            # Two files in one directory (LICENSE and LICENSE_THIRD_PARTY)
            # must not overwrite each other.
            case ${rel##*/} in
            *THIRD_PARTY*) dest="$dest-third-party" ;;
            esac
            cp "$src/$rel" "$dir/$dest"
            found=1
        done <<EOF
$(cd "$src" && find . \( -iname 'LICENSE*' -o -iname 'COPYING*' -o -iname 'COPYRIGHT*' \) \
    -type f ! -name '*.c' ! -name '*.h' ! -name '*.cc' ! -name '*.pm' |
    sed 's|^\./||' | sort)
EOF
        [ "$found" -eq 1 ] || die "no licence file found under $src for $name"
    done

    # PFFFT states its terms in the head of its only source file rather than
    # in a licence file, and requires binary redistributions to reproduce
    # them. Lift the comment block out verbatim.
    local pffft=$PJDIR/third_party/webrtc_aec3/src/third_party/pffft/src/pffft.c
    if [ -f "$pffft" ]; then
        sed -n '1,/^   PFFFT : a Pretty Fast FFT\./p' "$pffft" |
            sed '$d' >"$dir/LICENSE.webrtc_aec3-pffft"
        [ -s "$dir/LICENSE.webrtc_aec3-pffft" ] \
            || die "could not extract the PFFFT licence from $pffft"
    fi

    # The NOTICE lists what was actually collected rather than restating each
    # component's terms. Claiming terms by hand is how this file came to say
    # iLBC carried a BSD grant that its sources do not contain.
    {
        cat "$SELF_DIR/NOTICE"
        echo
        echo "Licence texts included in this artifact"
        echo "---------------------------------------"
        echo
        (cd "$dir" && ls | sed 's|^|  META-INF/licenses/|')
    } >"$root/META-INF/NOTICE"
}

# An AAR is a zip with a fixed layout, so it is assembled here rather than by
# Gradle. That keeps the distribution independent of the Android Gradle plugin
# version, which pins its own NDK and CMake and would otherwise decide what
# this artifact is built with.
#
# Assembled with jar rather than zip: a JDK is required anyway, for javac, and
# zip is missing from a fair number of minimal images. --no-manifest because
# jar would otherwise add a META-INF/MANIFEST.MF that does not belong in an
# AAR.
package_aar() {
    local version=$1
    local root=$STAGE/aar
    local abi

    echo "==> packaging AAR"
    rm -rf "$root"
    mkdir -p "$root/jni"
    # The manifest's minimum has to be the one the libraries were built for.
    sed -e "s|@MIN_SDK@|$ANDROID_API|g" \
        "$SELF_DIR/AndroidManifest.xml.in" >"$root/AndroidManifest.xml"
    cp "$STAGE/classes.jar" "$root/classes.jar"
    cp "$SELF_DIR/proguard.txt" "$root/proguard.txt"
    # Mandatory in the AAR format, and some AGP versions reject an AAR that
    # has none. This library declares no resources, so it is empty -- which is
    # what AGP itself emits here too.
    : >"$root/R.txt"
    stage_licenses "$root"
    for abi in $ABIS; do
        mkdir -p "$root/jni/$abi"
        cp "$STAGE/jni/$abi/libpjsua2.so" "$root/jni/$abi/"
    done

    # Cleared, not merely created: leftovers from a previous version would be
    # picked up by the checksum pass below and published alongside this one.
    # Only the output goes -- the dependency caches live elsewhere.
    rm -rf "$DIST"
    mkdir -p "$DIST"
    jar --create --no-manifest \
        --file "$DIST/$ARTIFACT_ID-$version.aar" -C "$root" .
    cp "$STAGE/$ARTIFACT_ID-sources.jar" "$DIST/$ARTIFACT_ID-$version-sources.jar"
    cp "$STAGE/$ARTIFACT_ID-javadoc.jar" "$DIST/$ARTIFACT_ID-$version-javadoc.jar"
}

# Maven Central requires a POM with name, description, url, licences,
# developers and scm. The artifact has no dependencies: everything it needs is
# inside the one library.
write_pom() {
    local version=$1
    sed -e "s|@GROUP_ID@|$GROUP_ID|g" \
        -e "s|@ARTIFACT_ID@|$ARTIFACT_ID|g" \
        -e "s|@VERSION@|$version|g" \
        "$SELF_DIR/pom.xml.in" >"$DIST/$ARTIFACT_ID-$version.pom"
}

# Maven Central requires an .md5 and a .sha1 beside every deployed file, each
# holding just the hex digest; .sha256 and .sha512 are accepted as extras.
# Signature files are exempt, and the checksums themselves are not signed.
checksums() {
    local f alg
    (
        cd "$DIST"
        rm -f ./*.md5 ./*.sha1 ./*.sha256 ./*.sha512 SHA256SUMS
        for f in *.aar *.jar *.pom; do
            for alg in md5 sha1 sha256 sha512; do
                hash_of "$alg" "$f" >"$f.$alg"
            done
        done
        # A single digest list as well, for release notes and for anyone
        # checking a download by hand.
        for f in *.aar *.jar *.pom; do
            echo "$(sha256_of "$f")  $f" >>SHA256SUMS
        done
    )
}

# ##############################################################################

main() {
    local abi version

    need cmake
    need swig
    need javac
    need jar
    need curl
    [ -n "${ANDROID_NDK_ROOT:-}" ] || die "ANDROID_NDK_ROOT must be set"
    [ -d "$ANDROID_NDK_ROOT" ] || die "ANDROID_NDK_ROOT does not exist"

    require_clean_tree
    mkdir -p "$STAGE" "$DIST" "$SRCDIR" "$DEPS"
    install_site

    rm -rf "$STAGE/java"
    for abi in $ABIS; do
        build_openssl "$abi"
        build_opus "$abi"
        build_oboe "$abi"
        build_abi "$abi"
    done

    make_classes_jar
    make_javadoc_jar

    version=$(pj_version)
    package_aar "$version"
    write_pom "$version"
    checksums

    restore_site
    trap - EXIT

    echo
    echo "==> $DIST"
    ls -la "$DIST"
}

main "$@"
