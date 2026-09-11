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
  const p = 1.11 + 0.4 * Math.sin(t / 30);
  return {
    smid: "55771146", age: Math.floor(t % 10), no_data: false,
    p, ei: 19087 + t / 3600, eo: 30836,
    // register snapshot keyed by preset name, like the firmware's `values` (subset: what the
    // Verlauf register list and the Live phase rows read)
    values: { Pi: p, Po: 0, V1: 231.4, V2: 230.1, V3: 229.6, I1: p * 1000 / 3 / 231.4, I2: p * 1000 / 3 / 230.1, I3: p * 1000 / 3 / 229.6,
      Ei: 19087.204 + t / 3600, Eo: 30836.881 },
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

// Mirrors /api/history. Deliberately reproduces every degraded case the firmware can emit, so the
// rendering of each is exercisable without hardware: a gap where the device was off, a reboot, a
// config change (counter chain broken -> null deltas), records written before the clock synced
// (qh null), and -- via ?noepoch=1 -- a device that never got NTP at all.
const QH_EPOCH = 1577836800;
const HF_BOOT = 4, HF_CONFIG = 8, HF_NO_DATA = 16;
const RANGE_SPEC = { day: [1, 96], week: [4, 168], month: [24, 120], year: [96, 365] };

function history(range, noEpoch) {
  const [bucket, n] = RANGE_SPEC[range] || RANGE_SPEC.day;
  const nowQh = Math.floor((Date.now() / 1000 - QH_EPOCH) / 900);
  const pts = [];
  if (state.meter) {
    for (let i = n; i > 0; i--) {
      if (i === 12) continue;                        // device was off: a hole in the qh sequence
      const qh = nowQh - i * bucket;
      const load = 1400 + 900 * Math.sin(qh / 9) + 300 * Math.sin(qh / 2.3);
      const pv = Math.max(0, 2600 * Math.sin(((qh % 96) / 96) * Math.PI));
      const net = Math.round(load - pv);
      const hrs = (bucket * 900) / 3600;
      let flags = 0;
      if (i === 20) flags |= HF_BOOT;
      if (i === 30) flags |= HF_CONFIG;
      if (i === 40) flags |= HF_NO_DATA;
      const broken = (flags & HF_CONFIG) !== 0 || (flags & HF_NO_DATA) !== 0;
      const timeless = noEpoch || i <= 3;            // the newest few were written pre-sync
      pts.push([
        timeless ? null : qh,
        broken ? null : Math.round(Math.max(0, net) * hrs),
        broken ? null : Math.round(Math.max(0, -net) * hrs),
        net - 400, net + 600, net, flags,
      ]);
    }
  }
  return {
    period: 900, range: RANGE_SPEC[range] ? range : "day", bucket,
    qh_epoch: QH_EPOCH, epoch_valid: !noEpoch, epoch: noEpoch ? 0 : Math.floor(Date.now() / 1000),
    now_qh: noEpoch ? 0 : nowQh, oldest_qh: nowQh - n * bucket, newest_qh: nowQh,
    count: pts.length, ok: true, pts,
  };
}

// Mirrors /api/frames: the last FRAME_LEN captures, newest-first -- DLMS HDLC frames or DSMR P1
// telegrams depending on the configured profile, exactly as the firmware does it. `encoding` tells
// the SPA whether the readable view is hex or the telegram's own ASCII.
const FRAME_LEN = 5, FRAME_CAP = 1280;
function frames() {
  const protocol = state.meter?.descriptor?.protocol === "dlms" ? "dlms"
    : state.meter?.descriptor?.protocol === "dsmr" ? "dsmr" : "none";
  const encoding = protocol === "dsmr" ? "text" : "hex";
  if (protocol === "none") return { protocol, encoding, cap: FRAME_CAP, len: FRAME_LEN, count: 0, frames: [] };
  if (protocol === "dsmr") {
    const n = Math.min(FRAME_LEN, Math.floor((Date.now() - state.t0) / 10000) + 1);
    const list = [];
    for (let i = 0; i < n; i++) {
      const ok = i !== 2;
      list.push({ i, age: i * 10, ok, raw_len: telegram(i).length, raw_trunc: false, plain_len: 0, plain_trunc: false });
    }
    return { protocol, encoding, cap: FRAME_CAP, len: FRAME_LEN, count: n, frames: list };
  }
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
  return { protocol, encoding, cap: FRAME_CAP, len: FRAME_LEN, count: n, frames: list };
}

// A plausible P1 telegram, CRC line included. Frame 2 gets a mangled CRC so the failure state is
// exercisable, matching the DLMS mock's convention.
function telegram(i) {
  const t = (Date.now() - state.t0) / 1000 - i * 10;
  const p = (1.4 + 0.5 * Math.sin(t / 30)).toFixed(3);
  return [
    "/ISk5\\2MT382-1000",
    "",
    "1-3:0.2.8(50)",
    "0-0:1.0.0(" + new Date().toISOString().slice(2, 19).replace(/[-:T]/g, "") + "W)",
    "0-0:96.1.1(4B384547303034303436333935353037)",
    "1-0:1.8.1(019087.213*kWh)",
    "1-0:1.8.2(019087.000*kWh)",
    "1-0:2.8.1(030836.881*kWh)",
    "1-0:1.7.0(" + p + "*kW)",
    "1-0:2.7.0(00.000*kW)",
    "1-0:32.7.0(231.4*V)",
    "1-0:31.7.0(001*A)",
    "!" + (i === 2 ? "BEEF" : "A1B2"),
    "",
  ].join("\r\n");
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
  const all = frames();
  const f = all.frames.find((x) => x.i === i);
  if (!f) return null;
  if (all.encoding === "text") {
    // One capture, two views: the telegram itself, or a hex dump of the same bytes.
    const t = telegram(i);
    return kind === "plain" ? t : hexDump(i + 1, t.length);
  }
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
  const url = new URL(req.url, "http://localhost");
  const path = url.pathname;
  const key = `${req.method} ${path}`;
  let body = "";
  for await (const c of req) body += c;

  // Query-param endpoint: the flat table below is keyed on the path alone.
  if (req.method === "GET" && path === "/api/history") {
    console.log(key, url.search);
    res.writeHead(200, { "content-type": "application/json" });
    return res.end(JSON.stringify(history(url.searchParams.get("range") || "day",
                                          url.searchParams.get("noepoch") === "1")));
  }

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
