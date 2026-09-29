#!/usr/bin/env node
/**
 * CLI used by demo-scenarios/:
 *   vision-call  --say "call support for this" [--frame path.jpg | --adb-screencap] [--dry-run] [--bridge]
 *   transcribe   [--pcm file.raw | --host 127.0.0.1 --port 7788] [--call-id N] [--dry-run]
 *   translate    [--pcm file.raw] --near en --far hi [--dry-run]
 */
import { afterCall, liveTranscribe, translationLoop, visionCall } from "./pipeline.js";

const [, , cmd, ...rest] = process.argv;
const flags = parse(rest);
const exec = { transport: flags.bridge ? "bridge" as const : "adb" as const,
               bridgeUrl: (flags["bridge-url"] as string) ?? "http://127.0.0.1:8787", dryRun: !!flags["dry-run"] };

(async () => {
  switch (cmd) {
    case "vision-call": {
      const { action, result } = await visionCall(String(flags.say ?? "call support for this"),
        { file: flags.frame as string | undefined, adbScreencap: !!flags["adb-screencap"] },
        { deviceModel: "Meta VR Glasses (sim)" }, exec);
      console.log(JSON.stringify({ action, result }, null, 2));
      break;
    }
    case "transcribe": {
      const segs = await liveTranscribe({ file: flags.pcm as string | undefined, host: flags.host as string | undefined,
        port: flags.port ? Number(flags.port) : undefined }, Number(flags["call-id"] ?? -1), exec,
        undefined, s => console.log(`${s.side}: ${s.text}`));
      if (flags["after-call"]) console.log(JSON.stringify(await afterCall(segs, {}), null, 2));
      break;
    }
    case "translate":
      await translationLoop({ file: flags.pcm as string | undefined }, String(flags.near ?? "en"), String(flags.far ?? "hi"), exec);
      break;
    default:
      console.error("usage: cli.ts vision-call|transcribe|translate [flags]");
      process.exit(2);
  }
})().catch(e => { console.error(e); process.exit(1); });

function parse(args: string[]): Record<string, string | boolean> {
  const out: Record<string, string | boolean> = {};
  for (let i = 0; i < args.length; i++) {
    if (!args[i].startsWith("--")) continue;
    const k = args[i].slice(2);
    const v = args[i + 1];
    if (v && !v.startsWith("--")) { out[k] = v; i++; } else out[k] = true;
  }
  return out;
}
