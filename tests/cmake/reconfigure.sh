#!/bin/sh
# Configure a build directory with one set of options, reconfigure it with
# another, and compare everything generated against a fresh configure with
# the second set. A difference means a value from the first configure
# survived in the cache and leaked into the second.
#
#   tests/cmake/reconfigure.sh <source dir> <work dir>

set -e
src=$(cd "$1" && pwd)
work=$2
common="-DPJ_SKIP_EXPERIMENTAL_NOTICE=ON"

# Everything a stale value could change: the installed package description,
# the generated headers, every target's flags and link line, the target list.
snapshot() {
    dir=$1; out=$2
    rm -rf "$out"; mkdir -p "$out"
    cmake --build "$dir" --target help | sort > "$out/targets"
    for f in $(cd "$dir" && find . \( -name flags.make -o -name link.txt \
                 -o -name '*_auto.h' -o -name sip_autoconf.h \
                 -o -name PjConfig.cmake -o -name PjDeps.cmake \
                 -o -name libpjproject.pc \) \
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
while IFS='|' read -r first second; do
    case $first in ''|'#'*) continue;; esac
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
-DPJMEDIA_WITH_VIDEO=ON|-DPJMEDIA_WITH_VIDEO=OFF
-DPJMEDIA_WITH_VIDEO=OFF|-DPJMEDIA_WITH_VIDEO=ON
-DPJMEDIA_WITH_AUDIODEV=ON|-DPJMEDIA_WITH_AUDIODEV=OFF
-DPJMEDIA_WITH_AUDIODEV=OFF|-DPJMEDIA_WITH_AUDIODEV=ON
-DPJ_DEP_RESAMPLE=none|-DPJ_DEP_RESAMPLE=bundled
-DPJ_DEP_SPEEX=none -DPJ_DEP_WEBRTC=none|-DPJ_DEP_SPEEX=bundled -DPJ_DEP_WEBRTC=bundled
-DPJLIB_WITH_SSL=|-DPJLIB_WITH_SSL=openssl
-DPJ_BUILD_APPS=OFF|-DPJ_BUILD_APPS=ON
PAIRS
exit $status
