#!/bin/bash
#
# Verify a built PJSIP.xcframework the way a consumer would use it.
#
# The framework lists are not written down here: they are parsed out of
# build/apple/Package.swift.in, so a framework that the manifest forgets to
# declare fails these tests rather than silently working for whoever happens
# to link it by hand.
#
#   tier 1  per slice, with no -D flags: the shipped headers assert the exact
#           configuration, the public API and the Clang module both compile,
#           the archive links beside a consumer that defines the same
#           third-party symbols, and nothing outside the pj API is exported
#   tier 2  a SwiftPM consumer on macOS that links the real manifest's linker
#           settings and runs
#   tier 3  a real iOS app bundle installed on a booted simulator, covering
#           the audio and video device backends and a live TLS listener
#
# Usage:
#   build/apple/tests/verify-xcframework.sh [options] [path-to-xcframework]
#
# Options:
#   --tier N        run only this tier; repeatable (default: 1 2 3)
#   --device NAME   simulator name or UDID for tier 3 (default: first
#                   available iPhone)
#   --keep          keep the scratch directory for inspection
#
# Tier 3 also registers against a real server when SIP_DOMAIN, SIP_USER and
# SIP_PASS are set; otherwise that check is skipped.
#
set -euo pipefail

SELF_DIR=$(cd "$(dirname "$0")" && pwd)
APPLE_DIR=$(cd "$SELF_DIR/.." && pwd)
PJDIR=$(cd "$APPLE_DIR/../.." && pwd)

XCFRAMEWORK=""
TIERS=""
DEVICE=""
KEEP=""
FAILURES=0

while [ $# -gt 0 ]; do
    case $1 in
    --tier)   TIERS="$TIERS $2"; shift 2 ;;
    --device) DEVICE=$2; shift 2 ;;
    --keep)   KEEP=1; shift ;;
    -h|--help) sed -n '2,32p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *)        XCFRAMEWORK=$1; shift ;;
    esac
done

XCFRAMEWORK=${XCFRAMEWORK:-$PJDIR/out/dist/PJSIP.xcframework}
TIERS=${TIERS:-"1 2 3"}

# Must match KEEP_SYMBOLS in build-xcframework.sh: the C API, the pj namespace,
# and std:: template instantiations. Anything else exported is a leak.
KEEP_SYMBOLS='^_pj|^_PJ|^__Z[A-Za-z]*2pj|^__Z[A-Za-z]*St[0-9]|^__ZSt'

die() { echo "error: $*" >&2; exit 1; }
pass() { printf '  \033[32mok\033[0m   %s\n' "$*"; }
fail() { printf '  \033[31mFAIL\033[0m %s\n' "$*"; FAILURES=$((FAILURES + 1)); }
head1() { printf '\n== %s\n' "$*"; }

[ -d "$XCFRAMEWORK" ] || die "no xcframework at $XCFRAMEWORK"

WORK=$(mktemp -d "${TMPDIR:-/tmp}/pjsip-verify.XXXXXX")
cleanup() { [ -n "$KEEP" ] && echo "scratch kept at $WORK" || rm -rf "$WORK"; }
trap cleanup EXIT

# ---------------------------------------------------------------- manifest --
# Frameworks the shipped Package.swift declares, split by platform condition.
manifest_frameworks() {
    local scope=$1 manifest=$APPLE_DIR/Package.swift.in
    case $scope in
    common) grep -o '\.linkedFramework("[A-Za-z]*")' "$manifest" ;;
    ios)    grep -o '\.linkedFramework("[A-Za-z]*", \.when(platforms: \[\.iOS\]))' "$manifest" ;;
    macos)  grep -o '\.linkedFramework("[A-Za-z]*", \.when(platforms: \[\.macOS\]))' "$manifest" ;;
    esac | sed 's/.*("\([A-Za-z]*\)".*/\1/'
}

link_flags() {
    local platform=$1 f out=""
    for f in $(manifest_frameworks common) $(manifest_frameworks "$platform"); do
        out="$out -framework $f"
    done
    echo "$out -lc++"
}

HOST_ARCH=$(uname -m)

# Deployment target and architecture are read back from the artifact rather
# than assumed, so the tests follow whatever the build was configured with and
# work on an Intel host as well as Apple Silicon.
slice_minos() {
    local lib=$1 arch=$2 tmp o minos
    tmp=$(mktemp -d "${TMPDIR:-/tmp}/pjsip-minos.XXXXXX")
    if ! lipo -thin "$arch" -output "$tmp/thin.a" "$lib" 2>/dev/null; then
        cp "$lib" "$tmp/thin.a"
    fi
    (cd "$tmp" && ar x thin.a 2>/dev/null) || true
    o=$(ls "$tmp"/*.o 2>/dev/null | grep -v thin | head -1)
    # Matched on the field, not anywhere in the line: vtool prints the
    # object's path first, and $tmp has "minos" in its own name.
    [ -n "$o" ] && minos=$(vtool -show-build "$o" 2>/dev/null \
                           | awk '$1 == "minos" { print $2; exit }')
    rm -rf "$tmp"
    # No default: every caller compiles against this, so inventing a version
    # would test a target the artifact never claimed -- and silently pass.
    [ -n "$minos" ] || die "cannot read the minimum OS version of $lib ($arch)"
    echo "$minos"
}

slice_dir() {
    local want=$1 d
    for d in "$XCFRAMEWORK"/*/; do
        [ -f "$d/libpjproject.a" ] || continue
        case $(basename "$d") in
        ios-arm64) [ "$want" = device ] && { echo "${d%/}"; return; } ;;
        *simulator*) [ "$want" = simulator ] && { echo "${d%/}"; return; } ;;
        macos*) [ "$want" = macos ] && { echo "${d%/}"; return; } ;;
        esac
    done
}

# ------------------------------------------------------------------ tier 1 --
tier1_slice() {
    local label=$1 dir=$2 sdk=$3 target=$4 platform=$5
    local hdr=$dir/Headers lib=$dir/libpjproject.a
    local w=$WORK/t1-$label
    mkdir -p "$w"

    # No -D flags: whatever the build needed must be in the headers already.
    if xcrun -sdk "$sdk" clang -fsyntax-only -target "$target" -I"$hdr" \
            "$SELF_DIR/tier1/config_assertions.c" 2>"$w/assert.log"; then
        pass "$label  configuration matches the shipped headers"
    else
        fail "$label  configuration assertions"
        sed 's/^/       /' "$w/assert.log" | grep -m4 error || true
    fi

    if xcrun -sdk "$sdk" clang -fmodules -fmodules-cache-path="$w/mcache" \
            -target "$target" -I"$hdr" -c "$SELF_DIR/tier1/use_api.m" \
            -o "$w/use_api.o" 2>"$w/module.log"; then
        pass "$label  module import and public headers compile"
    else
        fail "$label  module import"
        sed 's/^/       /' "$w/module.log" | grep -m4 error || true
    fi

    if xcrun -sdk "$sdk" clang -fsyntax-only -target "$target" -I"$hdr" \
            -Wno-macro-redefined \
            -DPJSIP_MAX_MODULE=9999 -DPJSIP_MAX_URL_SIZE=9999 \
            -DPJMEDIA_MAX_SDP_FMT=9999 -DPJ_MAX_OBJ_NAME=9999 \
            -DPJMEDIA_HAS_RTCP_XR=9999 \
            "$SELF_DIR/tier1/abi_override.c" 2>"$w/abi.log"; then
        pass "$label  consumer -D cannot move the ABI"
    else
        fail "$label  consumer -D overrides the frozen layout macros"
        sed 's/^/       /' "$w/abi.log" | grep -m3 error || true
    fi

    xcrun -sdk "$sdk" clang -target "$target" -c \
        "$SELF_DIR/tier1/symbol_clash.c" -o "$w/clash.o" 2>/dev/null

    if [ -f "$w/use_api.o" ] && xcrun -sdk "$sdk" clang -target "$target" \
            -dynamiclib -o "$w/out.dylib" "$w/use_api.o" "$w/clash.o" \
            -Wl,-all_load "$lib" $(link_flags "$platform") 2>"$w/link.log"; then
        pass "$label  links beside colliding third-party symbols"
    else
        fail "$label  link"
        sed 's/^/       /' "$w/link.log" | grep -m4 -i "error\|duplicate" || true
    fi

    local leaked
    leaked=$(nm -m "$lib" 2>/dev/null \
        | grep -v undefined \
        | grep -v "private external" \
        | grep -v "non-external" \
        | grep -E '\) (external|weak external) ' \
        | awk '{print $NF}' \
        | grep -vcE "$KEEP_SYMBOLS" || true)
    if [ "$leaked" = "0" ]; then
        pass "$label  exports nothing outside the pj API"
    else
        fail "$label  $leaked non-API symbols are still exported"
    fi
}

run_tier1() {
    head1 "tier 1  headers, symbols and linking"
    local d
    local m
    d=$(slice_dir device)
    if [ -n "$d" ]; then
        m=$(slice_minos "$d/libpjproject.a" arm64)
        tier1_slice "ios-device   " "$d" iphoneos "arm64-apple-ios$m" ios
    fi
    d=$(slice_dir simulator)
    if [ -n "$d" ]; then
        m=$(slice_minos "$d/libpjproject.a" "$HOST_ARCH")
        tier1_slice "ios-simulator" "$d" iphonesimulator "$HOST_ARCH-apple-ios$m-simulator" ios
    fi
    d=$(slice_dir macos)
    if [ -n "$d" ]; then
        m=$(slice_minos "$d/libpjproject.a" "$HOST_ARCH")
        tier1_slice "macos        " "$d" macosx "$HOST_ARCH-apple-macos$m" macos
    fi
}

# ------------------------------------------------------------------ tier 2 --
run_tier2() {
    head1 "tier 2  SwiftPM consumer on macOS"
    local w=$WORK/t2 settings
    [ -n "$(slice_dir macos)" ] || { fail "no macos slice to test"; return; }

    mkdir -p "$w/Shim/include"
    cp -R "$SELF_DIR/tier2/Sources" "$w/"
    cp "$APPLE_DIR/spm/Sources/PJSIPLinkerSettings/shim.c" "$w/Shim/"
    cp "$APPLE_DIR/spm/Sources/PJSIPLinkerSettings/include/PJSIPLinkerSettings.h" \
       "$w/Shim/include/"
    cp -R "$XCFRAMEWORK" "$w/PJSIP.xcframework"

    # Copy the real manifest's linkerSettings block verbatim, so the test
    # exercises the settings that ship rather than a second copy of them.
    sed -n '/linkerSettings: \[/,/^            \]/p' \
        "$APPLE_DIR/Package.swift.in" | sed '1d;$d' > "$w/settings.txt"
    [ -s "$w/settings.txt" ] || { fail "could not read linkerSettings from the manifest"; return; }

    python3 - "$SELF_DIR/tier2/Package.swift.template" "$w/settings.txt" \
             "$w/Package.swift" <<'PYEOF'
import pathlib, sys
tpl, settings, out = sys.argv[1:4]
text = pathlib.Path(tpl).read_text()
text = text.replace("@XCFRAMEWORK@", "PJSIP.xcframework")
text = text.replace("@LINKER_SETTINGS@",
                    pathlib.Path(settings).read_text().rstrip("\n"))
pathlib.Path(out).write_text(text)
PYEOF

    if (cd "$w" && swift build >"$w/build.log" 2>&1); then
        pass "package builds against the shipped linker settings"
    else
        fail "package build"
        grep -m4 error "$w/build.log" | sed 's/^/       /' || true
        return
    fi

    if (cd "$w" && ./.build/debug/App >"$w/run.log" 2>&1) && grep -q VERIFY-OK "$w/run.log"; then
        pass "$(grep -m1 VERIFY-OK "$w/run.log")"
    else
        fail "consumer run"
        tail -4 "$w/run.log" | sed 's/^/       /'
    fi
}

# ------------------------------------------------------------------ tier 3 --
pick_device() {
    if [ -n "$DEVICE" ]; then echo "$DEVICE"; return; fi
    xcrun simctl list devices available -j 2>/dev/null | python3 -c '
import json, sys
data = json.load(sys.stdin)["devices"]
best = None
for runtime, devices in data.items():
    if "iOS" not in runtime:
        continue
    for d in devices:
        if d.get("isAvailable") and "iPhone" in d["name"]:
            best = best or d["udid"]
print(best or "")'
}

run_tier3() {
    head1 "tier 3  iOS app on a booted simulator"
    local w=$WORK/t3 dir udid app out
    dir=$(slice_dir simulator)
    [ -n "$dir" ] || { fail "no simulator slice to test"; return; }

    udid=$(pick_device)
    [ -n "$udid" ] || { fail "no available iPhone simulator"; return; }

    app=$w/VerifyPJSIP.app
    mkdir -p "$app"

    local minos
    minos=$(slice_minos "$dir/libpjproject.a" "$HOST_ARCH")

    # The bundle has to claim the artifact's own minimum, not a fixed one: a
    # simulator older than the plist's MinimumOSVersion refuses the install,
    # which would read as a library failure rather than a harness mismatch.
    cp "$SELF_DIR/tier3/Info.plist" "$app/Info.plist"
    plutil -replace MinimumOSVersion -string "$minos" "$app/Info.plist"
    if ! xcrun -sdk iphonesimulator clang -fobjc-arc \
            -target "$HOST_ARCH-apple-ios$minos-simulator" \
            -I"$dir/Headers" "$SELF_DIR/tier3/main.m" \
            -o "$app/VerifyPJSIP" \
            -Wl,-all_load "$dir/libpjproject.a" \
            $(link_flags ios) -framework UIKit >"$w.build.log" 2>&1; then
        fail "app build"
        grep -m4 -i error "$w.build.log" | sed 's/^/       /' || true
        return
    fi
    pass "app bundle builds"

    xcrun simctl bootstatus "$udid" -b >/dev/null 2>&1 || xcrun simctl boot "$udid" >/dev/null 2>&1 || true
    xcrun simctl bootstatus "$udid" -b >/dev/null 2>&1 || true

    if ! xcrun simctl install "$udid" "$app" >"$w.install.log" 2>&1; then
        fail "install on simulator"
        sed 's/^/       /' "$w.install.log"
        return
    fi
    pass "installs on $(xcrun simctl list devices | grep -m1 "$udid" | sed 's/^ *//;s/ (.*//')"

    out=$(xcrun simctl launch --console-pty "$udid" org.pjsip.VerifyPJSIP 2>&1 || true)
    xcrun simctl uninstall "$udid" org.pjsip.VerifyPJSIP >/dev/null 2>&1 || true

    if echo "$out" | grep -q VERIFY-OK; then
        pass "$(echo "$out" | grep -m1 VERIFY-OK)"
    else
        fail "app run"
        echo "$out" | grep -i "VERIFY-FAIL\|error" | head -4 | sed 's/^/       /'
    fi
}

# -------------------------------------------------------------------- main --
echo "verifying $XCFRAMEWORK"
for t in $TIERS; do
    case $t in
    1) run_tier1 ;;
    2) run_tier2 ;;
    3) run_tier3 ;;
    *) die "unknown tier '$t'" ;;
    esac
done

echo ""
if [ "$FAILURES" -eq 0 ]; then
    echo "all checks passed"
else
    echo "$FAILURES check(s) failed"
    exit 1
fi
