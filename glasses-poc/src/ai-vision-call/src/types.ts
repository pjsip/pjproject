/** Shared types for the AI glasses bridge. */

export interface CameraFrame {
  jpeg: Buffer;
  width: number;
  height: number;
  timestampMs: number;
  source: "passthrough" | "camera2" | "file" | "none";
}

export interface PcmChunk {
  side: "near" | "far";
  sampleRate: 16000;
  pcm16le: Buffer;
  timestampMs: number;
}

export interface TranscriptSegment {
  side: "near" | "far";
  text: string;
  lang: string;          // BCP-47, e.g. "en", "hi"
  startMs: number;
  endMs: number;
}

export interface CallContext {
  callId?: number;
  remoteUri?: string;
  crmContact?: { name: string; company?: string; account?: string };
  deviceModel?: string;
}

/** Compact JSON that travels as the X-Meta-Vision-Context SIP header (<= 1 KB). */
export interface VisionContext {
  ctx_id: string;
  object?: string;       // "CNC lathe"
  model?: string;        // "Haas ST-20"
  serial?: string;
  fault?: string;        // "spindle overtemp alarm 9932"
  confidence: number;
  captured_at: number;
}

export type SipAction =
  | { type: "call"; target: string; displayName?: string; headers?: Record<string, string>; visionContext?: VisionContext }
  | { type: "answer"; callId: number }
  | { type: "hangup"; callId: number }
  | { type: "mute"; callId: number; mute: boolean }
  | { type: "transfer"; callId: number; target: string }
  | { type: "none"; reason: string };

export interface CrmTicketDraft {
  title: string;
  summary: string;
  actionItems: string[];
  contact?: string;
  visionContext?: VisionContext;
}
