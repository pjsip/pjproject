/**
 * The three end-to-end flows the demos exercise.
 */
import { getAudioPCM, getCameraFrame, type FrameSourceOptions, type PcmSourceOptions } from "./sources.js";
import { LlamaClient } from "./llama.js";
import { EchoStt, HttpStt, utterances, type SttProvider } from "./stt.js";
import { executeSipAction, pushTranscript, type ExecOptions } from "./sip-actions.js";
import type { CallContext, SipAction, TranscriptSegment } from "./types.js";

export function defaultStt(): SttProvider {
  return process.env.STT_URL ? new HttpStt(process.env.STT_URL, process.env.STT_API_KEY) : new EchoStt();
}

/** Scenario 2: "call support for this" with a camera frame. */
export async function visionCall(utterance: string, frame: FrameSourceOptions, ctx: CallContext, exec: ExecOptions,
                                 llama = new LlamaClient()): Promise<{ action: SipAction; result: string }> {
  const img = await getCameraFrame(frame);
  const crm = async (q: string) => JSON.stringify(await crmLookup(q, exec.bridgeUrl));
  const action = await llama.decideSipAction(utterance, img, ctx, crm);
  const result = await executeSipAction(action, exec);
  return { action, result };
}

/** Scenario 3: live transcription of both sides into the passthrough overlay. */
export async function liveTranscribe(pcm: PcmSourceOptions, callId: number, exec: ExecOptions,
                                     stt = defaultStt(), onSegment?: (s: TranscriptSegment) => void): Promise<TranscriptSegment[]> {
  const all: TranscriptSegment[] = [];
  for await (const seg of utterances(getAudioPCM(pcm), stt)) {
    all.push(seg);
    onSegment?.(seg);
    await pushTranscript(seg.side, seg.text, callId, exec.dryRun);
  }
  return all;
}

/**
 * Real-time translation loop (EN on glasses <-> HI on the SIP phone).
 * Near-end EN utterances are translated to HI and shown/spoken to the far
 * end; far-end HI utterances are translated to EN for the overlay. TTS
 * injection back into the media path is a second AudioMediaPort feeding
 * the call (see docs, "Media path"); here we only produce the text.
 */
export async function translationLoop(pcm: PcmSourceOptions, nearLang: string, farLang: string, exec: ExecOptions,
                                      llama = new LlamaClient(), stt = defaultStt()): Promise<void> {
  for await (const seg of utterances(getAudioPCM({ ...pcm, side: undefined }), stt)) {
    const target = seg.side === "near" ? farLang : nearLang;
    const out = await llama.translate(seg, target);
    await pushTranscript(seg.side, `${seg.text}  ->  ${out}`, -1, exec.dryRun);
  }
}

/** Muse Spark-style after-call: summary, ticket, notes via backend-bridge. */
export async function afterCall(transcript: TranscriptSegment[], ctx: CallContext, bridgeUrl = "http://127.0.0.1:8787",
                                llama = new LlamaClient()) {
  const draft = await llama.summarizeCall(transcript, ctx);
  const res = await fetch(`${bridgeUrl}/crm/tickets`, { method: "POST",
    headers: { "content-type": "application/json" }, body: JSON.stringify(draft) }).catch(() => null);
  return { draft, posted: !!res && res.ok };
}

async function crmLookup(q: string, bridgeUrl = "http://127.0.0.1:8787"): Promise<unknown> {
  try {
    const r = await fetch(`${bridgeUrl}/contacts?q=${encodeURIComponent(q)}`);
    return r.ok ? await r.json() : { error: r.status };
  } catch {
    return { results: [{ name: "CNC Support", uri: "sip:support-cnc@pbx.local" }], offline: true };
  }
}
