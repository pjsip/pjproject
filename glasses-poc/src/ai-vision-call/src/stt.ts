/**
 * Speech-to-text. Pluggable: on glasses this is the on-device recognizer
 * (Voice SDK / Muse dictation); here a provider interface plus a stub
 * that batches PCM into utterances on energy-based VAD.
 */
import type { PcmChunk, TranscriptSegment } from "./types.js";

export interface SttProvider {
  transcribe(pcm16le: Buffer, sampleRate: number, langHint?: string): Promise<{ text: string; lang: string }>;
}

/** Placeholder provider so the pipeline runs end-to-end without a key. */
export class EchoStt implements SttProvider {
  async transcribe(pcm16le: Buffer): Promise<{ text: string; lang: string }> {
    const ms = Math.round(pcm16le.length / 32);
    return { text: `[${ms} ms of speech]`, lang: "en" };
  }
}

/** HTTP provider: POST raw PCM to any Whisper-compatible endpoint. */
export class HttpStt implements SttProvider {
  constructor(private url: string, private apiKey?: string) {}
  async transcribe(pcm16le: Buffer, sampleRate: number, langHint?: string) {
    const res = await fetch(this.url, {
      method: "POST",
      headers: {
        "content-type": "application/octet-stream",
        "x-sample-rate": String(sampleRate),
        ...(langHint ? { "x-lang-hint": langHint } : {}),
        ...(this.apiKey ? { authorization: `Bearer ${this.apiKey}` } : {}),
      },
      body: new Uint8Array(pcm16le),
    });
    if (!res.ok) throw new Error(`stt ${res.status}`);
    return (await res.json()) as { text: string; lang: string };
  }
}

/** Simple energy VAD: yields an utterance after ~500 ms of silence. */
export async function* utterances(
  chunks: AsyncIterable<PcmChunk>,
  stt: SttProvider,
  opts: { silenceMs?: number; threshold?: number; langHint?: string } = {},
): AsyncGenerator<TranscriptSegment> {
  const silenceMs = opts.silenceMs ?? 500;
  const threshold = opts.threshold ?? 400;
  let buf: Buffer[] = [];
  let silent = 0;
  let start = 0;
  let side: "near" | "far" = "near";

  for await (const c of chunks) {
    side = c.side;
    const rms = rmsOf(c.pcm16le);
    if (buf.length === 0) start = c.timestampMs;
    buf.push(c.pcm16le);
    silent = rms < threshold ? silent + 20 : 0;
    if (silent >= silenceMs && buf.length * 20 > silenceMs + 300) {
      const pcm = Buffer.concat(buf);
      buf = []; silent = 0;
      const r = await stt.transcribe(pcm, 16000, opts.langHint);
      if (r.text.trim()) yield { side, text: r.text, lang: r.lang, startMs: start, endMs: c.timestampMs };
    }
  }
}

function rmsOf(pcm: Buffer): number {
  let acc = 0;
  const n = pcm.length / 2;
  for (let i = 0; i < n; i++) { const s = pcm.readInt16LE(i * 2); acc += s * s; }
  return Math.sqrt(acc / Math.max(1, n));
}
