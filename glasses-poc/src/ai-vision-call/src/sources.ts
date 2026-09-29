/**
 * Device-side sources. On glasses these map to the v207 Passthrough
 * Camera API and to PcmTapPort (over adb-forwarded TCP 7788).
 * On a laptop they read a JPEG file and a raw PCM file so the pipeline
 * can be exercised without hardware.
 */
import { execFile } from "node:child_process";
import { readFile } from "node:fs/promises";
import { connect } from "node:net";
import { promisify } from "node:util";
import type { CameraFrame, PcmChunk } from "./types.js";

const execFileP = promisify(execFile);

export interface FrameSourceOptions {
  /** Path to a JPEG for laptop testing. */
  file?: string;
  /** Use `adb exec-out screencap` on the connected device/emulator. */
  adbScreencap?: boolean;
  maxEdgePx?: number;
}

/** getCameraFrame(): one downscaled frame, or null on camera-free glasses (Luna). */
export async function getCameraFrame(opts: FrameSourceOptions = {}): Promise<CameraFrame | null> {
  const now = Date.now();
  if (opts.file) {
    const jpeg = await readFile(opts.file);
    return { jpeg, width: 0, height: 0, timestampMs: now, source: "file" };
  }
  if (opts.adbScreencap) {
    // Horizon OS / AVD stand-in for the Passthrough Camera API: screen
    // capture of whatever the emulator's virtual camera shows.
    const { stdout } = await execFileP("adb", ["exec-out", "screencap", "-p"],
      { encoding: "buffer", maxBuffer: 32 * 1024 * 1024 });
    return { jpeg: stdout as unknown as Buffer, width: 0, height: 0, timestampMs: now, source: "camera2" };
  }
  // TODO(v207): PassthroughCamera.captureStill({ maxEdge: opts.maxEdgePx ?? 512 })
  return null;
}

export interface PcmSourceOptions {
  /** Raw 16 kHz 16-bit mono PCM file (laptop testing). */
  file?: string;
  /** Host/port of PcmTapServer (adb forward tcp:7788 tcp:7788). */
  host?: string;
  port?: number;
  side?: "near" | "far";
}

/**
 * getAudioPCM(): async iterator of PCM chunks. From the device it reads
 * PcmTapServer frames: [side u8][len u16le][pcm...].
 */
export async function* getAudioPCM(opts: PcmSourceOptions = {}): AsyncGenerator<PcmChunk> {
  const side = opts.side ?? "near";
  if (opts.file) {
    const buf = await readFile(opts.file);
    const frame = 320 * 2; // 20 ms @ 16 kHz
    for (let off = 0; off + frame <= buf.length; off += frame) {
      yield { side, sampleRate: 16000, pcm16le: buf.subarray(off, off + frame), timestampMs: Date.now() };
    }
    return;
  }
  const sock = connect({ host: opts.host ?? "127.0.0.1", port: opts.port ?? 7788 });
  let pending = Buffer.alloc(0);
  for await (const data of sock) {
    pending = Buffer.concat([pending, data as Buffer]);
    while (pending.length >= 3) {
      const len = pending.readUInt16LE(1);
      if (pending.length < 3 + len) break;
      const chunkSide = pending[0] === 0 ? "near" : "far";
      const pcm = pending.subarray(3, 3 + len);
      pending = pending.subarray(3 + len);
      if (!opts.side || chunkSide === opts.side)
        yield { side: chunkSide, sampleRate: 16000, pcm16le: Buffer.from(pcm), timestampMs: Date.now() };
    }
  }
}
