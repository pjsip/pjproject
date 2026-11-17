#!/bin/sh
# Run the embedded_ua image under QEMU on an MPS2 board, with the board's
# LAN9118 attached to a host TAP interface.
#
#   zephyr/samples/embedded_ua/run-qemu.sh [build dir] [qemu args...]
#
# The build directory defaults to ./build. The TAP interface name comes from
# CONFIG_EMBUA_HOST_TAP_IFACE in the build (default zeth); create it once on
# the host with
#   sudo ip tuntap add dev zeth mode tap user $USER
#   sudo ip addr add 192.0.2.2/24 dev zeth
#   sudo ip link set zeth up
# Extra arguments go to QEMU, e.g. -nic user to use QEMU's own NAT instead.
#
# "west build -t run" cannot be used here: Zephyr adds "-device lan9118",
# which QEMU rejects because that NIC is part of the machine, not pluggable.

set -e
build=${1:-build}
[ $# -gt 0 ] && shift
cfg=$build/zephyr/.config
elf=$build/zephyr/zephyr.elf
[ -f "$cfg" ] && [ -f "$elf" ] || { echo "no Zephyr build in $build" >&2; exit 1; }

conf() { sed -n "s/^$1=\"\{0,1\}\([^\"]*\)\"\{0,1\}$/\1/p" "$cfg"; }

if [ "$(conf CONFIG_BOARD_MPS2_AN521_CPU0)" = y ]; then
    machine=mps2-an521; cpu=cortex-m33
elif [ "$(conf CONFIG_BOARD_MPS2_AN385)" = y ]; then
    machine=mps2-an385; cpu=cortex-m3
else
    echo "$build is not an MPS2 build; use west build -t run" >&2; exit 1
fi

qemu=$(sed -n 's/^QEMU_BIN_PATH:[A-Z]*=\(.*\)$/\1/p' "$build/CMakeCache.txt")
qemu=${QEMU:-${qemu:+$qemu/}qemu-system-arm}
tap=$(conf CONFIG_EMBUA_HOST_TAP_IFACE)
nic="-nic tap,ifname=${tap:-zeth},script=no,downscript=no"
case " $* " in *" -nic "*) nic= ;; esac

exec "$qemu" -cpu "$cpu" -machine "$machine" -m 16 -nographic -vga none \
     $nic -kernel "$elf" "$@"
