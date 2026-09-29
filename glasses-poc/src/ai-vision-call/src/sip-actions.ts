/**
 * Execute a SipAction against the glasses SipService. Two transports:
 *  - adb: broadcast/startService intents (emulator or USB-connected glasses)
 *  - bridge: POST to backend-bridge, which relays to the device or places
 *    the call from the PBX side
 */
import { execFile } from "node:child_process";
import { promisify } from "node:util";
import type { SipAction } from "./types.js";

const execFileP = promisify(execFile);
const SVC = "org.pjsip.glasses/.sip.SipService";
const A = "org.pjsip.glasses.action";

export interface ExecOptions { transport?: "adb" | "bridge"; bridgeUrl?: string; dryRun?: boolean }

export async function executeSipAction(action: SipAction, opts: ExecOptions = {}): Promise<string> {
  const args = toAdbArgs(action);
  if (opts.transport === "bridge") {
    const res = await fetch(`${opts.bridgeUrl ?? "http://127.0.0.1:8787"}/sip/action`, {
      method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify(action) });
    return `bridge ${res.status}`;
  }
  if (!args) return `no-op: ${(action as any).reason ?? ""}`;
  const cmd = ["shell", "am", "start-foreground-service", ...args];
  if (opts.dryRun) return `adb ${cmd.join(" ")}`;
  const { stdout } = await execFileP("adb", cmd);
  return stdout.trim();
}

function toAdbArgs(a: SipAction): string[] | null {
  const es = (k: string, v?: string) => (v == null ? [] : ["--es", k, v]);
  const ei = (k: string, v: number) => ["--ei", k, String(v)];
  switch (a.type) {
    case "call": {
      const headers = { ...(a.headers ?? {}) };
      return ["-n", SVC, "-a", `${A}.MAKE_CALL`, ...es("dst_uri", a.target), ...es("display_name", a.displayName),
              ...es("vision_context", a.visionContext ? JSON.stringify(a.visionContext) : undefined),
              ...Object.entries(headers).flatMap(([k, v]) => es(`hdr_${k}`, v))];
    }
    case "answer": return ["-n", SVC, "-a", `${A}.ANSWER_CALL`, ...ei("call_id", a.callId)];
    case "hangup": return ["-n", SVC, "-a", `${A}.HANGUP_CALL`, ...ei("call_id", a.callId)];
    case "mute": return ["-n", SVC, "-a", `${A}.SET_MUTE`, ...ei("call_id", a.callId), "--ez", "mute", String(a.mute)];
    case "transfer": return ["-n", SVC, "-a", `${A}.TRANSFER_CALL`, ...ei("call_id", a.callId), ...es("dst_uri", a.target)];
    case "none": return null;
  }
}

/** Push a transcript line to the passthrough overlay (EVENT_TRANSCRIPT). */
export async function pushTranscript(side: "near" | "far", text: string, callId = -1, dryRun = false): Promise<void> {
  const cmd = ["shell", "am", "broadcast", "-p", "org.pjsip.glasses", "-a", "org.pjsip.glasses.event.TRANSCRIPT",
               "--ei", "call_id", String(callId), "--es", "side", side, "--es", "text", text];
  if (dryRun) { console.log("adb", cmd.join(" ")); return; }
  await execFileP("adb", cmd);
}
