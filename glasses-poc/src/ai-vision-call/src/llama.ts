/**
 * Llama 4 Maverick client (Llama API, OpenAI-compatible chat/completions
 * with image parts and tool calling). Early-fusion multimodal: one call
 * takes the camera frame + transcript + CRM context and returns a SIP
 * action through tool calls.
 */
import type { CallContext, SipAction, TranscriptSegment, VisionContext, CrmTicketDraft, CameraFrame } from "./types.js";

export interface LlamaOptions {
  apiKey?: string;
  baseUrl?: string;
  model?: string;
}

const DEFAULTS = {
  baseUrl: process.env.LLAMA_API_BASE ?? "https://api.llama.com/compat/v1",
  model: process.env.LLAMA_MODEL ?? "Llama-4-Maverick-17B-128E-Instruct-FP8",
};

type Part = { type: "text"; text: string } | { type: "image_url"; image_url: { url: string } };
type Msg = { role: "system" | "user" | "assistant" | "tool"; content: string | Part[]; tool_call_id?: string; name?: string };

const TOOLS = [
  tool("crm_lookup", "Find a contact, queue or support extension by name, machine model or account.",
    { query: { type: "string" } }, ["query"]),
  tool("sip_call", "Place a SIP call from the glasses.",
    { target: { type: "string", description: "SIP URI" }, display_name: { type: "string" },
      vision_context: { type: "object", description: "object/model/serial/fault seen by the camera" } }, ["target"]),
  tool("sip_transfer", "Transfer the active call.", { target: { type: "string" } }, ["target"]),
  tool("sip_hangup", "End the active call.", {}, []),
  tool("no_action", "Nothing to do; explain why.", { reason: { type: "string" } }, ["reason"]),
];

function tool(name: string, description: string, props: Record<string, unknown>, required: string[]) {
  return { type: "function", function: { name, description, parameters: { type: "object", properties: props, required } } };
}

export class LlamaClient {
  private key: string;
  private base: string;
  private model: string;
  constructor(o: LlamaOptions = {}) {
    this.key = o.apiKey ?? process.env.LLAMA_API_KEY ?? "";
    this.base = o.baseUrl ?? DEFAULTS.baseUrl;
    this.model = o.model ?? DEFAULTS.model;
  }

  async chat(messages: Msg[], opts: { tools?: unknown[]; json?: boolean } = {}): Promise<any> {
    if (!this.key) return this.offline(messages, opts);
    const res = await fetch(`${this.base}/chat/completions`, {
      method: "POST",
      headers: { "content-type": "application/json", authorization: `Bearer ${this.key}` },
      body: JSON.stringify({
        model: this.model, messages, temperature: 0.2,
        ...(opts.tools ? { tools: opts.tools, tool_choice: "auto" } : {}),
        ...(opts.json ? { response_format: { type: "json_object" } } : {}),
      }),
    });
    if (!res.ok) throw new Error(`llama ${res.status}: ${await res.text()}`);
    return res.json();
  }

  /**
   * Vision + intent -> SipAction. `crmLookup` is executed locally when the
   * model asks for it, then the model is re-prompted with the result.
   */
  async decideSipAction(
    utterance: string,
    frame: CameraFrame | null,
    ctx: CallContext,
    crmLookup: (q: string) => Promise<string>,
  ): Promise<SipAction> {
    const parts: Part[] = [{ type: "text", text: `Technician said: "${utterance}"` }];
    if (frame) parts.push({ type: "image_url", image_url: { url: `data:image/jpeg;base64,${frame.jpeg.toString("base64")}` } });

    const messages: Msg[] = [
      { role: "system", content: SYSTEM_PROMPT(ctx) },
      { role: "user", content: parts },
    ];

    for (let round = 0; round < 4; round++) {
      const r = await this.chat(messages, { tools: TOOLS });
      const msg = r.choices?.[0]?.message;
      if (!msg) return { type: "none", reason: "empty response" };
      const calls = msg.tool_calls ?? [];
      if (calls.length === 0) return { type: "none", reason: String(msg.content ?? "no tool call") };
      messages.push(msg);
      for (const tc of calls) {
        const args = safeJson(tc.function.arguments);
        switch (tc.function.name) {
          case "crm_lookup":
            messages.push({ role: "tool", tool_call_id: tc.id, name: "crm_lookup", content: await crmLookup(args.query) });
            break;
          case "sip_call":
            return { type: "call", target: args.target, displayName: args.display_name,
                     visionContext: toVisionContext(args.vision_context) };
          case "sip_transfer":
            return ctx.callId != null ? { type: "transfer", callId: ctx.callId, target: args.target }
                                      : { type: "none", reason: "no active call" };
          case "sip_hangup":
            return ctx.callId != null ? { type: "hangup", callId: ctx.callId } : { type: "none", reason: "no active call" };
          case "no_action":
            return { type: "none", reason: args.reason };
        }
      }
    }
    return { type: "none", reason: "tool loop exhausted" };
  }

  /** Translate one transcript segment; used by the real-time translation loop. */
  async translate(seg: TranscriptSegment, targetLang: string): Promise<string> {
    const r = await this.chat([
      { role: "system", content: `Translate the user's ${seg.lang} speech into ${targetLang}. Output only the translation, spoken register, no notes.` },
      { role: "user", content: seg.text },
    ]);
    return r.choices?.[0]?.message?.content?.trim() ?? "";
  }

  /** Muse Spark-style after-call agent: summary + ticket draft. */
  async summarizeCall(transcript: TranscriptSegment[], ctx: CallContext, vision?: VisionContext): Promise<CrmTicketDraft> {
    const text = transcript.map(s => `${s.side === "near" ? "TECH" : "REMOTE"}: ${s.text}`).join("\n");
    const r = await this.chat([
      { role: "system", content: "You are the after-call agent for a field technician. Return JSON {title, summary, actionItems[], contact}." },
      { role: "user", content: `Context: ${JSON.stringify({ ...ctx, vision })}\n\nTranscript:\n${text}` },
    ], { json: true });
    const draft = safeJson(r.choices?.[0]?.message?.content ?? "{}");
    return { title: draft.title ?? "Call", summary: draft.summary ?? "", actionItems: draft.actionItems ?? [],
             contact: draft.contact, visionContext: vision };
  }

  /* Deterministic stand-in when LLAMA_API_KEY is unset, so demos run offline. */
  private offline(messages: Msg[], opts: { tools?: unknown[]; json?: boolean }) {
    const last = messages[messages.length - 1];
    const text = typeof last.content === "string" ? last.content
      : last.content.filter((p): p is Extract<Part, { type: "text" }> => p.type === "text").map(p => p.text).join(" ");
    const hasImage = typeof last.content !== "string" && last.content.some(p => p.type === "image_url");
    if (opts.tools) {
      const m = /call (\w+)/i.exec(text);
      const name = (m?.[1] ?? "warehouse").toLowerCase();
      const target = hasImage ? "sip:support-cnc@pbx.local" : `sip:${name}@pbx.local`;
      const vision_context = hasImage ? { object: "CNC lathe", model: "Haas ST-20", fault: "alarm 9932 spindle overtemp", confidence: 0.71 } : undefined;
      return { choices: [{ message: { role: "assistant", content: null, tool_calls: [
        { id: "offline-1", type: "function", function: { name: "sip_call", arguments: JSON.stringify({ target, display_name: hasImage ? "CNC Support" : undefined, vision_context }) } } ] } }] };
    }
    if (opts.json) return { choices: [{ message: { content: JSON.stringify({ title: "Offline summary", summary: text.slice(0, 200), actionItems: ["(offline) create ticket"] }) } }] };
    return { choices: [{ message: { content: `[offline translation] ${text}` } }] };
  }
}

const SYSTEM_PROMPT = (ctx: CallContext) => `You are the calling agent inside a field technician's smart glasses.
You see what the technician sees (image, if present) and hear what they said.
Decide the single SIP action. Use crm_lookup to resolve names, machine models
or accounts to SIP URIs before calling. When an image is present, identify the
equipment (object, model, serial if legible, visible fault/alarm) and pass it as
vision_context so the callee sees it before answering. Keep vision_context under
600 characters. Current context: ${JSON.stringify(ctx)}`;

function toVisionContext(v: any): VisionContext | undefined {
  if (!v) return undefined;
  return { ctx_id: `ctx_${Date.now().toString(36)}`, object: v.object, model: v.model, serial: v.serial,
           fault: v.fault, confidence: Number(v.confidence ?? 0.5), captured_at: Date.now() };
}

function safeJson(s: string): any { try { return JSON.parse(s); } catch { return {}; } }
