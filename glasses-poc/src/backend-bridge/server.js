/**
 * backend-bridge: tiny HTTP service between the glasses SIP client, a
 * legacy PBX / PJSIP gateway, the WhatsApp Business Calling API and the
 * CRM. No dependencies (node:http only) so it runs anywhere.
 *
 * Routes
 *   GET  /contacts?q=            CRM lookup -> [{name, uri, company}]
 *   GET  /callerid?uri=          reverse lookup for the overlay
 *   POST /sip/action             relay a SipAction to the device (adb) or to the PBX
 *   POST /call                   {from, to, visionContext}: route to whatsapp:/pstn:/sip:
 *   POST /crm/tickets            Muse Spark after-call ticket
 *   POST /webhooks/whatsapp      WhatsApp Calling API webhook (call events / SDP)
 *
 * Routing rule for /call:
 *   to = "whatsapp:+91..."  -> WhatsApp Business Calling API (Multi-Solution
 *                              Conversations): POST /{PHONE_NUMBER_ID}/calls
 *                              with action "connect" and our SDP offer; the
 *                              media leg terminates on the PJSIP gateway.
 *   to = "pstn:+1..."       -> PJSIP gateway trunk (sip:+1...@gw;user=phone)
 *   to = "sip:..."          -> PBX as-is
 * X-Meta-Vision-Context is forwarded as a custom header to sip:/pstn: and
 * stored on the CRM interaction for whatsapp:.
 */
import http from "node:http";
import { execFile } from "node:child_process";

const PORT = Number(process.env.PORT ?? 8787);
const WA_PHONE_ID = process.env.WA_PHONE_NUMBER_ID ?? "";
const WA_TOKEN = process.env.WA_ACCESS_TOKEN ?? "";
const WA_GRAPH = process.env.WA_GRAPH_BASE ?? "https://graph.facebook.com/v21.0";
const PBX_GATEWAY = process.env.PBX_GATEWAY ?? "pbx.local";

/* Demo CRM. Replace with the real CRM client. */
const CONTACTS = [
  { name: "Warehouse", uri: "sip:2001@pbx.local", company: "Acme Logistics" },
  { name: "Dispatch", uri: "sip:2002@pbx.local", company: "Acme Logistics" },
  { name: "CNC Support", uri: "sip:support-cnc@pbx.local", company: "Haas", models: ["ST-20", "VF-2"] },
  { name: "Priya (Warehouse)", uri: "sip:priya@pbx.local", company: "Acme Logistics", whatsapp: "whatsapp:+919999000001" },
];
const tickets = [];
const interactions = new Map();

const server = http.createServer(async (req, res) => {
  const url = new URL(req.url, `http://${req.headers.host}`);
  try {
    if (req.method === "GET" && url.pathname === "/contacts") {
      const q = (url.searchParams.get("q") ?? "").toLowerCase();
      return json(res, { results: CONTACTS.filter(c => JSON.stringify(c).toLowerCase().includes(q)) });
    }
    if (req.method === "GET" && url.pathname === "/callerid") {
      const uri = url.searchParams.get("uri") ?? "";
      const c = CONTACTS.find(c => uri.includes(c.uri.replace("sip:", "").split("@")[0]));
      return json(res, { name: c?.name ?? null, company: c?.company ?? null });
    }
    if (req.method === "POST" && url.pathname === "/sip/action") {
      const action = await body(req);
      return json(res, await relayToDevice(action));
    }
    if (req.method === "POST" && url.pathname === "/call") {
      return json(res, await routeCall(await body(req)));
    }
    if (req.method === "POST" && url.pathname === "/crm/tickets") {
      const t = { id: `T-${tickets.length + 1}`, createdAt: new Date().toISOString(), ...(await body(req)) };
      tickets.push(t);
      console.log("ticket", t.id, t.title);
      return json(res, t, 201);
    }
    if (req.method === "POST" && url.pathname === "/webhooks/whatsapp") {
      const ev = await body(req);
      console.log("wa webhook", JSON.stringify(ev).slice(0, 300));
      // TODO: on "call.connect" with SDP answer -> hand to PJSIP gateway (pjsua --sdp)
      return json(res, { ok: true });
    }
    if (req.method === "GET" && url.pathname === "/webhooks/whatsapp") {
      return text(res, url.searchParams.get("hub.challenge") ?? "");   // verification
    }
    json(res, { error: "not found" }, 404);
  } catch (e) {
    json(res, { error: String(e) }, 500);
  }
});

async function routeCall({ from, to, visionContext, displayName }) {
  const ctxId = visionContext?.ctx_id ?? `ctx_${Date.now().toString(36)}`;
  if (visionContext) interactions.set(ctxId, { from, to, visionContext });

  if (to.startsWith("whatsapp:")) {
    const wa = to.slice("whatsapp:".length);
    if (!WA_PHONE_ID || !WA_TOKEN) return { routed: "whatsapp", dryRun: true, to: wa, ctxId };
    // WhatsApp Business Calling API: business-initiated call (requires user
    // permission via a call-permission request in a Multi-Solution
    // Conversation). The SDP offer comes from the PJSIP gateway leg.
    const sdp = await gatewaySdpOffer(from, ctxId);
    const r = await fetch(`${WA_GRAPH}/${WA_PHONE_ID}/calls`, {
      method: "POST",
      headers: { authorization: `Bearer ${WA_TOKEN}`, "content-type": "application/json" },
      body: JSON.stringify({ messaging_product: "whatsapp", to: wa, action: "connect",
                             session: { sdp_type: "offer", sdp } }),
    });
    return { routed: "whatsapp", status: r.status, body: await r.json().catch(() => ({})), ctxId };
  }
  const sipTarget = to.startsWith("pstn:") ? `sip:${to.slice(5)}@${PBX_GATEWAY};user=phone` : to;
  return relayToDevice({ type: "call", target: sipTarget, displayName,
                         headers: { "X-Meta-Vision-Ctx-Id": ctxId }, visionContext });
}

/** Ask the PJSIP gateway (pjsua/pjsua2 process) for an SDP offer. Stub. */
async function gatewaySdpOffer(from, ctxId) {
  return `v=0\r\no=- 0 0 IN IP4 0.0.0.0\r\ns=glasses-${ctxId}\r\nt=0 0\r\nm=audio 4000 RTP/SAVPF 111\r\na=rtpmap:111 opus/48000/2\r\n`;
}

/** Relay a SipAction to the connected glasses via adb (same contract as ai-vision-call). */
function relayToDevice(action) {
  if (action.type !== "call") return Promise.resolve({ relayed: false, reason: "only call supported here" });
  const args = ["shell", "am", "start-foreground-service", "-n", "org.pjsip.glasses/.sip.SipService",
    "-a", "org.pjsip.glasses.action.MAKE_CALL", "--es", "dst_uri", action.target];
  if (action.displayName) args.push("--es", "display_name", action.displayName);
  if (action.visionContext) args.push("--es", "vision_context", JSON.stringify(action.visionContext));
  return new Promise(resolve => execFile("adb", args, (err, stdout) =>
    resolve({ relayed: !err, out: err ? String(err) : stdout.trim() })));
}

async function body(req) {
  const chunks = [];
  for await (const c of req) chunks.push(c);
  return chunks.length ? JSON.parse(Buffer.concat(chunks).toString("utf8")) : {};
}
function json(res, obj, code = 200) { res.writeHead(code, { "content-type": "application/json" }); res.end(JSON.stringify(obj)); }
function text(res, s) { res.writeHead(200, { "content-type": "text/plain" }); res.end(s); }

server.listen(PORT, () => console.log(`backend-bridge listening on :${PORT}`));
