#!/usr/bin/env python3
"""Scenario tests for pcaputil gap/reorder/jitter handling.

Generates synthetic PCMA streams whose every frame encodes its own index,
runs pcaputil on each, and decodes the WAV back into the sequence of frame
indexes.

Usage: tests/pcaputil/pcaputil_test.py [PCAPUTIL] [scenario...]

PCAPUTIL defaults to pjsip-apps/bin/samples/<target>/pcaputil (the newest
one if there are several).
"""
import glob, os, random, struct, subprocess, sys, tempfile, wave

PTIME = 0.02
N = 1000


def alaw2lin(a):
    a ^= 0x55
    t = (a & 0x0f) << 4
    seg = (a & 0x70) >> 4
    t = t + 8 if seg == 0 else (t + 0x108) << max(seg - 1, 0)
    return t if a & 0x80 else -t


LIN2A = {alaw2lin(a): a for a in range(256)}


def payload(fid):
    return bytes([0x80 | (fid >> 7), fid & 0x7f]) * 80


def frame_id(samples):
    codes = [LIN2A.get(s) for s in samples]
    if None in codes or len(set(codes[0::2])) != 1 or \
       len(set(codes[1::2])) != 1 or not codes[0] & 0x80 or codes[1] & 0x80:
        return None
    return ((codes[0] & 0x7f) << 7) | codes[1]


def write_pcap(path, pkts):
    """pkts: list of (arrival_sec, ssrc, seq, ts, marker, fid)."""
    with open(path, 'wb') as f:
        f.write(struct.pack('<IHHiIII', 0xa1b2c3d4, 2, 4, 0, 0, 65535, 1))
        for t, ssrc, seq, ts, m, fid in sorted(pkts, key=lambda p: p[0]):
            rtp = struct.pack('>BBHII', 0x80, (m << 7) | 8, seq & 0xffff,
                              ts & 0xffffffff, ssrc) + payload(fid)
            udp = struct.pack('>HHHH', 4000, 4002, 8 + len(rtp), 0) + rtp
            ip = struct.pack('>BBHHHBBH4s4s', 0x45, 0, 20 + len(udp), 0, 0,
                             64, 17, 0, b'\x0a\0\0\x01', b'\x0a\0\0\x02')
            pkt = b'\0' * 12 + b'\x08\x00' + ip + udp
            us = int(round(t * 1e6)) + 1700000000 * 1000000
            f.write(struct.pack('<IIII', us // 1000000, us % 1000000,
                                len(pkt), len(pkt)) + pkt)


def stream(n=N, seq0=1000, ts0=5000, ssrc=0x1234, fid0=0, t0=1.0):
    return [[t0 + i * PTIME, ssrc, seq0 + i, ts0 + i * 160, 0, fid0 + i]
            for i in range(n)]


def run(tool, pkts, tmp, name):
    """Returns (ids in output order, silent/concealed frames between them,
    {id: output time})."""
    pcap, wav = os.path.join(tmp, name + '.pcap'), \
                os.path.join(tmp, name + '.wav')
    write_pcap(pcap, pkts)
    subprocess.run([tool, pcap, wav], check=True, capture_output=True)
    w = wave.open(wav)
    data = w.readframes(w.getnframes())
    s = struct.unpack('<%dh' % (len(data) // 2), data)
    # Scan at sample granularity: PLC may shift the output alignment
    found, i = [], 0
    while i + 160 <= len(s):
        fid = frame_id(s[i:i + 160])
        if fid is None:
            i += 1
        else:
            found.append((i, fid))
            i += 160
    if not found:
        return [], 0, {}
    span = round((found[-1][0] - found[0][0]) / 160) + 1
    return [f for _, f in found], span - len(found), \
           {f: i / 8000 for i, f in found}


def in_order(got):
    return all(a < b for a, b in zip(got, got[1:]))


def check(got, holes, ids, exp_holes, tol):
    """The decoder's PLC alters the frames around each concealment, and the
    last frame is cut by its delay, hence tol."""
    missing = set(ids) - set(got)
    if not in_order(got):
        return 'out of order'
    if set(got) - set(ids):
        return 'unexpected frames %s' % sorted(set(got) - set(ids))[:5]
    if len(missing) > tol:
        return '%d frames missing %s' % (len(missing), sorted(missing)[:8])
    if abs(holes - exp_holes) > tol + len(missing):
        return 'expected %d silent frames, got %d' % (exp_holes, holes)
    return None


ALL = range(N)


# Each scenario returns (packets, expected ids, expected silent frames, tol)
def sc_baseline():
    return stream(), ALL, 0, 1


def sc_reorder():
    p = stream()
    for i in range(10, N - 1, 7):           # swap arrival of pairs
        p[i][0], p[i + 1][0] = p[i + 1][0], p[i][0]
    return p, ALL, 0, 1


def sc_duplicate():
    p = stream()
    p += [[t + 0.005] + r for t, *r in p[::3]]
    return p, ALL, 0, 1


def sc_loss():
    lost = {100, 101, 102, 103, 104, 500}
    p = [x for x in stream() if x[5] not in lost]
    return p, [i for i in ALL if i not in lost], len(lost), 5


def sc_jitter():
    rnd = random.Random(1)
    p = stream()
    # Not the first packets: the jitter buffer rejects frames older than
    # the first one it got.
    for x in p[10:]:
        x[0] += rnd.uniform(0, 0.08)
    return p, ALL, 0, 1


def sc_burst():
    p = stream()
    for i, x in enumerate(p):              # 10 packets delivered at once
        x[0] = 1.0 + (i // 10 + 1) * 10 * PTIME
    return p, ALL, 0, 1


def sc_drift_slow():
    p = stream()
    for i, x in enumerate(p):              # sender clock 0.5% slow
        x[0] = 1.0 + i * PTIME * 1.005
    return p, ALL, 5, 4


def sc_drift_fast():
    p = stream()
    for i, x in enumerate(p):              # sender clock 0.5% fast
        x[0] = 1.0 + i * PTIME * 0.995
    return p, ALL, 0, 6


def sc_seq_wrap():
    return stream(seq0=65000), ALL, 0, 1


def sc_ts_wrap():
    return stream(ts0=0xffffffff - 160 * 300), ALL, 0, 1


def pause(secs, ts_follows, restart=False):
    p = stream()
    for x in p[N // 2:]:
        x[0] += secs
        if ts_follows:
            x[3] += int(secs * 8000)
        if restart:
            x[1] = 0x5678
            x[2] += 30000
            x[3] += 123456789
    return p


def sc_pause_3s():
    """Pause where RTP ts follows wall clock (DTX/VAD)."""
    return pause(3, True), ALL, 150, 3


def sc_pause_3s_ts_frozen():
    """Pause where RTP ts does not advance (hold/marker case)."""
    p = pause(3, False)
    p[N // 2][4] = 1
    return p, ALL, 150, 3


def sc_pause_40s():
    """Long pause, same stream (dr-30.5-gap case): must be filled."""
    return pause(40, True), ALL, 2000, 3


def sc_ts_jump():
    """RTP ts jumps forward without a real pause: no silence."""
    p = stream()
    for x in p[N // 2:]:
        x[3] += 1000000000
    return p, ALL, 0, 1


def sc_restart():
    """SSRC/seq/ts restart mid-stream, no pause. The first packet of the
    new stream is dropped as bad by the RTP session."""
    return pause(0, False, True), [i for i in ALL if i != N // 2], 0, 1


def talkspurts(*lens, pause_s=2):
    """Talkspurts of the given packet counts, separated by pauses."""
    p, n = [], 0
    for k, cnt in enumerate(lens):
        for x in stream(n=cnt, fid0=n, seq0=1000 + n, ts0=5000 + n * 160,
                        t0=1.0 + n * PTIME + k * pause_s):
            x[3] += k * pause_s * 8000
            p.append(x)
        n += cnt
    return p, n


def sc_short_last_talkspurt():
    """A final talkspurt shorter than the jitter buffer delay."""
    p, n = talkspurts(200, 5)
    return p, range(n), 100, 3, (202,)


def sc_short_mid_talkspurt():
    """A short talkspurt between pauses is played on time, not held
    until the next talkspurt."""
    p, n = talkspurts(200, 5, 200)
    return p, range(n), 200, 5, (202, 206)


def sc_restart_40s():
    """Stream restart after >30s: the gap is skipped."""
    return pause(40, False, True), [i for i in ALL if i != N // 2], 0, 3


SCENARIOS = {k[3:]: v for k, v in globals().items() if k.startswith('sc_')}


def find_tool(args):
    if args and args[0] not in SCENARIOS:
        tool = args.pop(0)
    else:
        bins = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                            '..', '..', 'pjsip-apps', 'bin', 'samples', '*')
        found = glob.glob(os.path.join(bins, 'pcaputil')) + \
                glob.glob(os.path.join(bins, 'pcaputil.exe'))
        tool = max(found, key=os.path.getmtime) if found else None
    if not tool or not os.path.isfile(tool):
        sys.exit(__doc__)
    return os.path.abspath(tool)


def main():
    args = sys.argv[1:]
    tool = find_tool(args)
    names = args or list(SCENARIOS)
    fails = 0
    with tempfile.TemporaryDirectory() as tmp:
        for name in names:
            pkts, ids, exp_holes, tol, *timed = SCENARIOS[name]()
            got, holes, when = run(tool, pkts, tmp, name)
            err = check(got, holes, ids, exp_holes, tol)
            sent = {x[5]: x[0] for x in pkts}
            for fid in (timed[0] if timed else ()):
                late = when.get(fid, float('inf')) - when[got[0]] - \
                       (sent[fid] - sent[got[0]])
                if not err and abs(late) > 0.1:
                    err = 'frame %d played %.2fs off' % (fid, late)
            fails += bool(err)
            print('%-20s %s  (played=%d silent=%d)' %
                  (name, 'FAIL: ' + err if err else 'ok', len(got), holes))
    sys.exit(1 if fails else 0)


if __name__ == '__main__':
    main()
