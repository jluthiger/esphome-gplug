// Dev server: serves dist/index.html and fakes the device API so the wizard
// can be exercised without hardware.  `npm run build && npm run dev`
import { createServer } from "node:http";
import { readFileSync, existsSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";

const here = dirname(fileURLToPath(import.meta.url));
const PORT = Number(process.env.PORT || 8080);
// Canonical presets live in the firmware component (what the device serves); see tools/scripts2presets.py.
const presets = JSON.parse(readFileSync(join(here, "..", "..", "firmware", "components", "gplug_smi", "presets.json"), "utf8"));

const state = {
  version: "0.1.0-mock", hostname: "gplug-a1b2c3",
  hardware: null, meter: null,
  wifi: { connected: false, ssid: null, ip: null, rssi: null, error: null },
  t0: Date.now(),
};

const NETS = [
  { ssid: "Home-WLAN", rssi: -48, secure: true },
  { ssid: "Nachbar", rssi: -77, secure: true },
  { ssid: "Gast", rssi: -60, secure: false },
];

function live() {
  if (!state.meter) return { age: null, no_data: false };
  const t = (Date.now() - state.t0) / 1000;
  const keyInvalid = state.meter.key !== undefined && state.meter.key?.toLowerCase().startsWith("dead");
  if (keyInvalid) return { age: null, no_data: false, key_invalid: true };
  return {
    smid: "55771146", age: Math.floor(t % 10), no_data: false,
    p: 1.11 + 0.4 * Math.sin(t / 30),
    ei: 19087 + t / 3600, eo: 30836,
  };
}

// Mirrors /api/ring: 360 samples at 10 s = 1 h, [pi, po, p1, p2, p3] in W per sample, same shape
// the real gplug_smi ring buffer sends.
function ring() {
  if (!state.meter) return { period: 10, samples: [] };
  const n = Math.min(360, Math.floor((Date.now() - state.t0) / 10000) + 1);
  const samples = [];
  for (let i = 0; i < n; i++) {
    const t = (Date.now() - state.t0) / 1000 - (n - 1 - i) * 10;
    const pi = Math.round(1400 + 500 * Math.sin(t / 30));
    samples.push([pi, 0, Math.round(pi / 3), Math.round(pi / 3), Math.round(pi / 3)]);
  }
  return { period: 10, samples };
}

// Mirrors /api/frames: last FRAME_LEN raw DLMS HDLC frame captures, newest-first, DLMS only --
// same gating a DSMR-configured device gets from the real firmware (frame_log.h is never
// populated for DSMR, see firmware/components/gplug_smi/gplug_smi.cpp's loop()).
const FRAME_LEN = 5, FRAME_CAP = 768;
function frames() {
  const protocol = state.meter?.descriptor?.protocol === "dlms" ? "dlms"
    : state.meter?.descriptor?.protocol === "dsmr" ? "dsmr" : "none";
  if (protocol !== "dlms") return { protocol, cap: FRAME_CAP, len: FRAME_LEN, count: 0, frames: [] };
  const n = Math.min(FRAME_LEN, Math.floor((Date.now() - state.t0) / 10000) + 1);
  const list = [];
  for (let i = 0; i < n; i++) {
    const ok = i !== 2;   // one synthetic CRC-fail entry so the empty/fail state is exercisable
    list.push({
      i, age: i * 10, ok,
      raw_len: 210 + (i % 3) * 40, raw_trunc: false,
      plain_len: ok ? 160 + (i % 3) * 20 : 0, plain_trunc: false,
    });
  }
  return { protocol, cap: FRAME_CAP, len: FRAME_LEN, count: n, frames: list };
}

function hexDump(seed, n) {
  const lines = [];
  for (let i = 0; i < n; i += 16) {
    const row = [];
    for (let j = i; j < Math.min(i + 16, n); j++) row.push(((seed * 7 + j * 31) & 0xff).toString(16).padStart(2, "0").toUpperCase());
    lines.push(row.join(" "));
  }
  return lines.join("\n") + "\n";
}

function frameDetail(i, kind) {
  const f = frames().frames.find((x) => x.i === i);
  if (!f) return null;
  const n = kind === "raw" ? f.raw_len : f.plain_len;
  if (!n) return null;
  return hexDump(i + 1, n);
}

function statusMeter() {
  if (!state.meter) return null;
  return { preset: state.meter.preset, protocol: state.meter.descriptor?.protocol,
    encrypted: state.meter.key !== undefined };
}

const routes = {
  "GET /api/status": () => ({ ...state, meter: statusMeter(), uptime: (Date.now() - state.t0) / 1000, heap: 123456 }),
  "GET /api/presets": () => presets,
  "GET /api/frames": () => frames(),
  "GET /api/wifi/scan": () => new Promise((r) => setTimeout(() => r(NETS), 1200)),
  "POST /api/config/wifi": (b) => {
    state.wifi = { connected: false, ssid: b.ssid, ip: null, rssi: null, error: null };
    setTimeout(() => {
      if (b.psk === "wrong") state.wifi.error = "auth";
      else Object.assign(state.wifi, { connected: true, ip: "192.168.1.42", rssi: -51 });
    }, 4000);
    return { ok: true };
  },
  "POST /api/config/hardware": (b) => { state.hardware = b; return { ok: true }; },
  "POST /api/config/meter": (b) => { state.meter = b; state.t0 = Date.now(); return { ok: true }; },
  "GET /api/live": () => live(),
  "GET /api/ring": () => ring(),
  "POST /api/reboot": () => ({ ok: true }),
};

createServer(async (req, res) => {
  const path = req.url.split("?")[0];
  const key = `${req.method} ${path}`;
  let body = "";
  for await (const c of req) body += c;

  // /api/frames/<i>/raw|plain -- text/plain, not in the flat JSON route table above.
  const m = req.method === "GET" && path.match(/^\/api\/frames\/(\d+)\/(raw|plain)$/);
  if (m) {
    const text = frameDetail(Number(m[1]), m[2]);
    console.log(key);
    if (text == null) { res.writeHead(404, { "content-type": "application/json" }); return res.end('{"error":"not found"}'); }
    res.writeHead(200, { "content-type": "text/plain; charset=utf-8" });
    return res.end(text);
  }

  const fn = routes[key];
  if (fn) {
    const out = await fn(body ? JSON.parse(body) : undefined);
    console.log(key, body || "");
    res.writeHead(200, { "content-type": "application/json" });
    return res.end(JSON.stringify(out));
  }
  if (req.method === "GET") {
    const f = join(here, "..", "dist", "index.html");
    if (existsSync(f)) {
      res.writeHead(200, { "content-type": "text/html; charset=utf-8" });
      return res.end(readFileSync(f));
    }
    res.writeHead(503); return res.end("run `npm run build` first");
  }
  res.writeHead(404); res.end();
}).listen(PORT, () => console.log(`mock gPlug on http://localhost:${PORT}/`));
