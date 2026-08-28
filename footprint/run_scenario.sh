#!/bin/bash
#
# run_scenario.sh -- drive the footprint measurement call scenario.
#
# Two pjsua instances on localhost, TLS-only SIP transport, SRTP
# mandatory, N concurrent calls held in steady state, then pjsua 'dd'
# pool dump and clean shutdown.  One side can run under valgrind massif.
#
# Usage:
#   SP=<scratch-dir> ./run_scenario.sh TAG NCALLS MASSIF_SIDE CODEC HOLD [BIN]
#
#   TAG          name for the output dir ($SP/results/run-TAG)
#   NCALLS       0..5 concurrent calls to place
#   MASSIF_SIDE  B (callee), A (caller), or none
#   CODEC        g722 | g711  (the only codec left enabled for the run)
#   HOLD         steady-state hold time in seconds
#   BIN          pjsua binary (default: 32-bit build in $SP/wt-m32)
#
# Sync notes: pjsua buffers both its console output and its log file, so
# readiness is detected from /proc/net/tcp (TLS listener in LISTEN state)
# and console output is forced out by sending blank console lines
# ("nudges") which make the app reprint its prompt and flush stdout.
# Log files are complete only after a clean 'q' exit.
#
set -u

SP=${SP:?set SP to the scratch dir}
TAG=$1; N=$2; MASSIF=$3; CODEC=$4; HOLD=$5
BIN=${6:-$SP/wt-m32/pjsip-apps/bin/pjsua-i686-pc-linux-gnu}
OUT=$SP/results/run-$TAG
CERT=$SP/certs/cert.pem
KEY=$SP/certs/key.pem

rm -rf "$OUT"; mkdir -p "$OUT"

case $CODEC in
    g722) DIS="--dis-codec PCMU --dis-codec PCMA"; CLOCK=16000;;
    g711) DIS="--dis-codec G722"; CLOCK=8000;;
    *) echo "bad codec $CODEC"; exit 1;;
esac

# NB: --use-tls must precede --no-udp/--no-tcp (pjsua validates in order)
COMMON="--null-audio --no-vad --max-calls 5 --use-srtp 2 --srtp-secure 1 \
 --use-tls --tls-cert-file $CERT --tls-privkey-file $KEY --no-udp --no-tcp \
 --ip-addr 127.0.0.1 --bound-addr 127.0.0.1 \
 --clock-rate $CLOCK $DIS --log-level 4 --app-log-level 3"

VG="valgrind --tool=massif --time-unit=ms --max-snapshots=100 \
 --detailed-freq=4 --threshold=0.2"

mkfifo "$OUT/b.in" "$OUT/a.in"

B_CMD="$BIN $COMMON --local-port 5070 --rtp-port 4000 --auto-answer 200 --log-file $OUT/b.log"
A_CMD="$BIN $COMMON --local-port 5080 --rtp-port 4200 --log-file $OUT/a.log"

[ "$MASSIF" = "B" ] && B_CMD="$VG --massif-out-file=$OUT/massif.b $B_CMD"
[ "$MASSIF" = "A" ] && A_CMD="$VG --massif-out-file=$OUT/massif.a $A_CMD"

# wait until a local TCP port is in LISTEN state (0A) per /proc/net/tcp
wait_listen() { # port timeout_s
    local hex i=0
    hex=$(printf '%04X' "$1")
    while [ $i -lt "$2" ]; do
        grep -qi ":$hex 00000000:0000 0A" /proc/net/tcp && return 0
        sleep 1; i=$((i+1))
    done
    echo "TIMEOUT waiting for listener on port $1" >&2
    return 1
}

# nudge the console (forces stdout flush) until pattern count appears
nudge_wait() { # fifo confile pattern count timeout_s
    local i=0
    while [ $i -lt "$5" ]; do
        printf '\n' > "$1"
        [ "$(grep -c "$3" "$2" 2>/dev/null)" -ge "$4" ] && return 0
        sleep 2; i=$((i+2))
    done
    echo "TIMEOUT waiting for '$3' (x$4) in $2" >&2
    return 1
}

$B_CMD < "$OUT/b.in" > "$OUT/b.con" 2>&1 &
BPID=$!
exec 8> "$OUT/b.in"
wait_listen 5071 180 || true

APID=""
if [ "$N" -gt 0 ]; then
    $A_CMD < "$OUT/a.in" > "$OUT/a.con" 2>&1 &
    APID=$!
    exec 9> "$OUT/a.in"
    wait_listen 5081 180 || true
    sleep 2
    c=1
    while [ $c -le "$N" ]; do
        printf 'm\n' >&9; sleep 2
        printf 'sip:footprint@127.0.0.1:5071;transport=tls\n' >&9
        sleep 2
        nudge_wait "$OUT/a.in" "$OUT/a.con" "state changed to CONFIRMED" "$c" 180 || true
        c=$((c+1))
    done
fi

echo "holding $N call(s) for ${HOLD}s..."
sleep "$HOLD"

printf 'dd\n' >&8
[ -n "$APID" ] && printf 'dd\n' >&9
sleep 8
nudge_wait "$OUT/b.in" "$OUT/b.con" "Start dumping application states" 1 30 || true

[ -n "$APID" ] && { printf 'q\n' >&9; sleep 1; exec 9>&-; }
if [ -n "$APID" ]; then
    for i in $(seq 1 180); do kill -0 "$APID" 2>/dev/null || break; sleep 1; done
fi
printf 'q\n' >&8; sleep 1; exec 8>&-
for i in $(seq 1 180); do kill -0 "$BPID" 2>/dev/null || break; sleep 1; done

# last resort so massif still gets written on a wedged instance
kill -TERM "$BPID" 2>/dev/null; [ -n "$APID" ] && kill -TERM "$APID" 2>/dev/null
wait 2>/dev/null

confirmed_a=0
[ -f "$OUT/a.log" ] && confirmed_a=$(grep -c "state changed to CONFIRMED" "$OUT/a.log")
confirmed_b=$(grep -c "state changed to CONFIRMED" "$OUT/b.log" 2>/dev/null)
echo "RUN $TAG done: A confirmed=$confirmed_a B confirmed=$confirmed_b"
