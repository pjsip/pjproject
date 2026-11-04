#!/bin/sh
# Configure a build directory with one set of options, reconfigure it with
# another, and compare everything generated against a fresh configure with
# the second set. A difference means a value from the first configure
# survived in the cache and leaked into the second.
#
#   tests/cmake/reconfigure.sh <source dir> <work dir>

set -e
src=$(cd "$1" && pwd)
mkdir -p "$2"
work=$(cd "$2" && pwd)
common="-DPJ_SKIP_EXPERIMENTAL_NOTICE=ON"

# Two copies of the installed libsrtp2 as package roots, for the pairs that
# add, remove or move SRTP_ROOT; those pairs are skipped without libsrtp2.
roots=
make_root() {
    dir=$work/roots/$1
    incdir=$(pkg-config --variable=includedir libsrtp2)
    libdir=$(pkg-config --variable=libdir libsrtp2)
    pcfile=$(pkg-config --variable=pcfiledir libsrtp2)/libsrtp2.pc
    rm -rf "$dir"
    mkdir -p "$dir/include" "$dir/lib/pkgconfig"
    cp -R "$incdir/srtp2" "$dir/include/"
    cp -P "$libdir"/libsrtp2.* "$dir/lib/"
    sed -e "s#^prefix=.*#prefix=$dir#" \
        -e 's#^libdir=.*#libdir=${prefix}/lib#' \
        -e 's#^includedir=.*#includedir=${prefix}/include#' \
        "$pcfile" > "$dir/lib/pkgconfig/libsrtp2.pc"
}
if pkg-config --exists libsrtp2 2>/dev/null; then
    make_root srtp-a
    make_root srtp-b
    roots=$work/roots
fi

# Everything a stale value could change: the installed package description,
# the generated headers, every target's flags and link line, the target list.
snapshot() {
    dir=$1; out=$2
    rm -rf "$out"; mkdir -p "$out"
    cmake --build "$dir" --target help | sort > "$out/targets"
    for f in $(cd "$dir" && find . \( -name flags.make -o -name link.txt \
                 -o -name '*_auto.h' -o -name sip_autoconf.h \
                 -o -name PjConfig.cmake -o -name PjDeps.cmake \
                 -o -name PjFoundPaths.cmake -o -name libpjproject.pc \) \
                 -not -path './CMakeFiles/*'); do
        case $f in
        */CMakeFiles/*.dir/*)
            # the directory of a target the earlier configure had and this
            # one does not is left behind; it belongs to no generated rule
            target=$(basename "$(dirname "$f")" .dir)
            grep -qx "\.\.\. $target" "$out/targets" || continue
            ;;
        esac
        mkdir -p "$out/$(dirname "$f")"
        sed "s#$dir#BUILD#g" "$dir/$f" > "$out/$f"
    done
}

status=0
while IFS= read -r pair; do
    case $pair in ''|'#'*) continue;; esac
    case $pair in
    *@ROOTS@*)
        if [ -z "$roots" ]; then
            echo "### $pair: skipped, no libsrtp2 for the roots"
            continue
        fi
        pair=$(echo "$pair" | sed "s#@ROOTS@#$roots#g")
        ;;
    esac
    first=${pair%%|*}
    second=${pair#*|}
    echo "### $first -> $second"
    rm -rf "$work/fresh" "$work/reconf"
    cmake -S "$src" -B "$work/fresh" $common $second > /dev/null
    cmake -S "$src" -B "$work/reconf" $common $first > /dev/null
    cmake -S "$src" -B "$work/reconf" $second > /dev/null
    snapshot "$work/fresh" "$work/fresh.snap"
    snapshot "$work/reconf" "$work/reconf.snap"
    if ! diff -r "$work/fresh.snap" "$work/reconf.snap"; then
        echo "### reconfigure differs from a fresh configure"
        status=1
    fi
done <<'PAIRS'
-DPJ_WITH_CXX=ON|-DPJ_WITH_CXX=OFF
-DPJ_WITH_CXX=OFF|-DPJ_WITH_CXX=ON
-DPJ_BUILD_PJSUA2=ON|-DPJ_BUILD_PJSUA2=OFF
-DPJ_BUILD_PJSUA2=OFF|-DPJ_BUILD_PJSUA2=ON
-DPJMEDIA_WITH_VIDEO=ON|-DPJMEDIA_WITH_VIDEO=OFF
-DPJMEDIA_WITH_VIDEO=OFF|-DPJMEDIA_WITH_VIDEO=ON
-DPJMEDIA_WITH_AUDIODEV=ON|-DPJMEDIA_WITH_AUDIODEV=OFF
-DPJMEDIA_WITH_AUDIODEV=OFF|-DPJMEDIA_WITH_AUDIODEV=ON
-DPJ_DEP_RESAMPLE=none|-DPJ_DEP_RESAMPLE=bundled
-DPJ_DEP_SPEEX=none -DPJ_DEP_WEBRTC=none|-DPJ_DEP_SPEEX=bundled -DPJ_DEP_WEBRTC=bundled
-DPJMEDIA_WITH_WEBRTC_AEC3=ON|-DPJMEDIA_WITH_WEBRTC_AEC3=OFF
-DPJMEDIA_WITH_WEBRTC_AEC3=OFF|-DPJMEDIA_WITH_WEBRTC_AEC3=ON
-DPJLIB_WITH_SSL=|-DPJLIB_WITH_SSL=openssl
-DPJ_BUILD_APPS=OFF|-DPJ_BUILD_APPS=ON
# a root added (bundled -> system), removed (back to bundled), moved
|-DSRTP_ROOT=@ROOTS@/srtp-a
-DSRTP_ROOT=@ROOTS@/srtp-a|-USRTP_ROOT
-DSRTP_ROOT=@ROOTS@/srtp-a|-DSRTP_ROOT=@ROOTS@/srtp-b
PAIRS
exit $status
