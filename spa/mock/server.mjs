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

// MOCK_OTA_PASSWORD=x puts /update behind Basic auth (user "admin"), like gplug_smi's ota_password.
const OTA_PASSWORD = process.env.MOCK_OTA_PASSWORD || "";
// MOCK_OTA_ROLLBACK=1 accepts the upload and reboots, but comes back running the image it had
// before -- what the bootloader does with an image that crash-loops. The firmware card should
// report that, and can only see it by comparing image identities (`app`), not build timestamps.
const OTA_ROLLBACK = process.env.MOCK_OTA_ROLLBACK === "1";

const state = {
  version: "0.1.0-mock", hostname: "gplug-a1b2c3", build: Math.floor(Date.now() / 1000) - 86400,
  // Like the device's `app`: the first 16 hex digits of the running image's ELF SHA-256. The
  // firmware card compares it against the file it uploaded, so the mock has to take the uploaded
  // file's own hash on a successful "update" -- otherwise the success path is untestable here.
  app: "0f1e2d3c4b5a6978",
  ota_auth: !!OTA_PASSWORD,
  hardware: null, meter: null,
  wifi: { connected: false, ssid: null, ip: null, rssi: null, error: null },
  t0: Date.now(), hwT0: Date.now(),
};

const NETS = [
  { ssid: "Home-WLAN", rssi: -48, secure: true },
  { ssid: "Nachbar", rssi: -77, secure: true },
  { ssid: "Gast", rssi: -60, secure: false },
];

// Setup diagnosis like GplugSmi::diag_, with an 8 s grace instead of the firmware's 60 s.
// MOCK_DIAG=silent|garbled|no_match makes the *first* meter config fail that way; the next one
// (the user fixing it in the wizard) works -- enough to walk the whole fix loop.
const MOCK_DIAG = process.env.MOCK_DIAG || "";
const DIAG_GRACE_S = 8;

// Header sniffing like GplugSmi's ProtocolSniffer: what the line "speaks" shows up in /api/live
// `detect` DETECT_DELAY_S after the hardware step. Defaults follow the variant (gPlugD/D-E -> DSMR,
// gPlugK/M -> encrypted DLMS); MOCK_DETECT=dsmr|dlms|none overrides it, e.g. `dlms` on a gPlugD
// makes the wizard propose the encrypted P1 profile, `none` leaves the step in manual mode.
const MOCK_DETECT = process.env.MOCK_DETECT || "";
const DETECT_DELAY_S = 4;

function detect() {
  const none = { protocol: null, encrypted: null, hits: 0, age: null };
  if (!state.hardware) return none;
  const t = (Date.now() - state.hwT0) / 1000;
  const proto = MOCK_DETECT || (["gplugk", "gplugm"].includes(state.hardware.variant) ? "dlms" : "dsmr");
  if (proto === "none" || t < DETECT_DELAY_S) return none;
  return { protocol: proto, encrypted: proto === "dlms", hits: Math.floor(t / 5) + 1, age: Math.floor(t % 5) };
}

function live() {
  const d = detect();
  const rxAge = state.hardware && MOCK_DETECT !== "none" ? Math.floor(((Date.now() - state.hwT0) / 1000) % 5) : null;
  const base = { detect: d, rx_age: rxAge };
  if (!state.meter) return { ...base, age: null, no_data: false, diag: "unconfigured" };
  const t = (Date.now() - state.t0) / 1000;
  const grace = t < DIAG_GRACE_S;
  const keyInvalid = state.meter.key !== undefined && state.meter.key?.toLowerCase().startsWith("dead");
  if (keyInvalid) return { ...base, age: grace ? null : 3, no_data: false, key_invalid: !grace, diag: grace ? "waiting" : "key" };
  // The sniffed protocol contradicting the profile is conclusive at once, no grace (GplugSmi::diag_).
  if (d.protocol && d.hits >= 2 && d.protocol !== state.meter.descriptor?.protocol) return { ...base, age: null, no_data: true, diag: "protocol" };
  const forced = MOCK_DIAG && state.meterCommits <= 1;
  if (grace && (forced || t < 3)) return { ...base, age: null, no_data: false, diag: "waiting" };
  if (forced && MOCK_DIAG === "no_match") return { ...base, age: 3, no_data: false, diag: "no_match", values: {} };
  if (forced) return { ...base, age: null, no_data: true, diag: MOCK_DIAG };
  const p = 1.11 + 0.4 * Math.sin(t / 30);
  return {
    ...base, smid: "55771146", age: Math.floor(t % 10), no_data: false, diag: "ok",
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

// Mirrors /api/history.csv and the `history` block of /api/status: MOCK_HIST_DAYS (default 40)
// days of synthetic 15-min records, same load/PV curve as history() above, with the same
// degraded cases (a hole, a reboot, a config change, undated records at the end).
const HIST_DAYS = Number(process.env.MOCK_HIST_DAYS || 40);
const HF_PARTIAL = 2;
function histRecords() {
  const nowQh = Math.floor((Date.now() / 1000 - QH_EPOCH) / 900);
  const n = HIST_DAYS * 96;
  const out = [];
  let ei = 12345678, eo = 2345678;
  for (let i = n; i > 0; i--) {
    const qh = nowQh - i;
    const load = 1400 + 900 * Math.sin(qh / 9) + 300 * Math.sin(qh / 2.3);
    const pv = Math.max(0, 2600 * Math.sin(((qh % 96) / 96) * Math.PI));
    const net = Math.round(load - pv);
    ei += Math.round(Math.max(0, net) / 4); eo += Math.round(Math.max(0, -net) / 4);
    if (i > 500 && i <= 512) continue;                              // device was off for 3 h
    let flags = 0;
    if (i === 500) flags |= HF_BOOT | HF_PARTIAL;
    if (i === 300) flags |= HF_CONFIG;
    if (i === 200) flags |= HF_NO_DATA;
    out.push({ qh: i <= 2 ? null : qh, ei, eo, pAvg: net, pMin: net - 400, pMax: net + 600, flags });
  }
  return state.meter ? out : [];
}
function historyMeta() {
  const r = histRecords();
  const dated = r.filter((x) => x.qh !== null);
  return { ok: true, count: r.length, oldest_qh: dated[0]?.qh || 0, newest_qh: dated.at(-1)?.qh || 0 };
}
const pad2 = (v) => String(v).padStart(2, "0");
function csvTime(qh) {
  const d = new Date((QH_EPOCH + qh * 900) * 1000);
  return `${d.getFullYear()}-${pad2(d.getMonth() + 1)}-${pad2(d.getDate())} ${pad2(d.getHours())}:${pad2(d.getMinutes())}`;
}
function historyCsv(from, to) {
  const windowed = from || to;
  let s = "von;bis;bezug_zaehler_wh;einspeisung_zaehler_wh;bezug_wh;einspeisung_wh;p_avg_w;p_min_w;p_max_w;hinweise\r\n";
  let prev = null;
  for (const r of histRecords()) {
    const emit = !windowed || (r.qh !== null && r.qh >= from && (!to || r.qh < to));
    const contiguous = prev && ((r.qh !== null && prev.qh !== null && r.qh === prev.qh + 1) ||
                                (r.qh === null && prev.qh === null && !(r.flags & HF_BOOT)));
    if (emit) {
      const chain = contiguous && !(r.flags & HF_CONFIG);
      const notes = [];
      if (r.qh === null) notes.push("zeit_unbekannt");
      if (prev && !contiguous) notes.push("luecke_davor");
      if (r.flags & HF_BOOT) notes.push("neustart");
      if (r.flags & HF_CONFIG) notes.push("konfig_geaendert");
      if (r.flags & HF_PARTIAL) notes.push("teilintervall");
      if (r.flags & HF_NO_DATA) notes.push("keine_daten");
      s += [r.qh === null ? "" : csvTime(r.qh), r.qh === null ? "" : csvTime(r.qh + 1), r.ei, r.eo,
            chain ? r.ei - prev.ei : "", chain ? r.eo - prev.eo : "", r.pAvg, r.pMin, r.pMax, notes.join(",")].join(";") + "\r\n";
    }
    prev = r;
  }
  return s;
}

// Mirrors /api/log (event_log.h): a plausible field history, including the events a healthy bench
// device never produces -- a crash, a watchdog, a brownout, a storage failure -- since those are
// exactly the ones whose rendering has to be right when a support case finally needs them.
// MOCK_LOG=empty serves an empty log instead.
function eventLog() {
  const now = Math.floor(Date.now() / 1000);
  if (process.env.MOCK_LOG === "empty") return { now, uptime: 3600, cap: 32, events: [] };
  const ago = (h) => now - Math.round(h * 3600);
  //        [t,        up,   code, detail, value, repeat]
  const raw = [
    [ago(96),  2,    1, 1, 182, 0],   // boot, power applied
    [ago(95),  8,    3, 0, 48,  0],   // wifi up
    [ago(94),  120,  7, 2, 0,   0],   // meter configured
    [ago(94),  180,  6, 0, 0,   0],   // meter data arrived
    [ago(52),  0,    4, 0, 0,   7],   // wifi flapping, folded
    [ago(52),  0,    3, 0, 61,  0],
    [ago(30),  0,    5, 5, 0,   0],   // meter went silent
    [ago(29),  0,    6, 0, 0,   0],   // and came back
    [ago(20),  0,    1, 9, 176, 0],   // brownout
    [ago(12),  0,    1, 4, 174, 0],   // crash
    [ago(11),  0,    8, 1, 0,   2],   // history write failed
    [ago(6),   0,    1, 3, 178, 0],   // software restart
    [ago(6),   0,    2, 0, 0,   0],   // ... which was an update
    [ago(2),   0,    9, 0, 0,   0],   // reset button
    [0,        41,   1, 1, 180, 0],   // a boot before the clock synced: uptime only
  ];
  return { now, uptime: 4100, cap: 32,
    events: raw.map(([t, up, code, detail, value, repeat]) => ({ t, up, code, detail, value, repeat })) };
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
  "GET /api/status": () => ({ ...state, meter: statusMeter(), uptime: (Date.now() - state.t0) / 1000, heap: 123456,
    time: { valid: true, epoch: Math.floor(Date.now() / 1000) }, history: historyMeta() }),
  "GET /api/presets": () => presets,
  "GET /api/frames": () => frames(),
  "GET /api/log": () => eventLog(),
  "GET /api/wifi/scan": () => new Promise((r) => setTimeout(() => r(NETS), 1200)),
  "POST /api/config/wifi": (b) => {
    state.wifi = { connected: false, ssid: b.ssid, ip: null, rssi: null, error: null };
    setTimeout(() => {
      if (b.psk === "wrong") state.wifi.error = "auth";
      else Object.assign(state.wifi, { connected: true, ip: "192.168.1.42", rssi: -51 });
    }, 4000);
    return { ok: true };
  },
  "POST /api/config/hardware": (b) => { state.hardware = b; state.hwT0 = Date.now(); return { ok: true }; },
  "POST /api/config/meter": (b) => {
    if (b.keep_key) {   // GplugSmi::merge_stored_keys_
      if (!state.meter?.key) return { __status: 400, error: "no stored key" };
      b = { ...b, key: state.meter.key }; delete b.keep_key;
    }
    state.meter = b; state.t0 = Date.now(); state.meterCommits = (state.meterCommits || 0) + 1;
    return { ok: true };
  },
  "GET /api/live": () => live(),
  "GET /api/ring": () => ring(),
  "POST /api/reboot": () => ({ ok: true }),
};

createServer(async (req, res) => {
  const url = new URL(req.url, "http://localhost");
  const path = url.pathname;
  const key = `${req.method} ${path}`;

  // ESPHome's ota.web_server: multipart upload, plain-text verdict, then a reboot. The mock checks
  // only the image magic byte, "reboots" (API down for 6 s, fresh uptime) and bumps the build time.
  if (req.method === "POST" && path === "/update") {
    const chunks = [];
    for await (const c of req) chunks.push(c);
    if (OTA_PASSWORD && req.headers.authorization !== "Basic " + Buffer.from("admin:" + OTA_PASSWORD).toString("base64")) {
      res.writeHead(401, { "www-authenticate": 'Basic realm="Login Required"' }); return res.end();
    }
    const buf = Buffer.concat(chunks);
    const start = buf.indexOf("\r\n\r\n") + 4;   // skip the multipart part headers
    const ok = buf[start] === 0xe9;
    console.log(key, buf.length, "bytes", ok ? "ok" : "bad image");
    res.writeHead(200, { "content-type": "text/plain" });
    res.end(ok ? "Update Successful!" : "Update Failed!");
    if (ok) {
      state.rebootUntil = Date.now() + 6000;
      state.t0 = Date.now();
      state.build = Math.floor(Date.now() / 1000);
      // esp_app_desc_t.app_elf_sha256 sits at 0xB0 of the image; `start` is where the image begins
      // inside the multipart body.
      if (!OTA_ROLLBACK) state.app = buf.subarray(start + 0xb0, start + 0xb8).toString("hex");
    }
    return;
  }
  if (state.rebootUntil && Date.now() < state.rebootUntil && path.startsWith("/api/")) { req.destroy(); return; }

  let body = "";
  for await (const c of req) body += c;

  // Query-param endpoint: the flat table below is keyed on the path alone.
  if (req.method === "GET" && path === "/api/history") {
    console.log(key, url.search);
    res.writeHead(200, { "content-type": "application/json" });
    return res.end(JSON.stringify(history(url.searchParams.get("range") || "day",
                                          url.searchParams.get("noepoch") === "1")));
  }

  if (req.method === "GET" && path === "/api/history.csv") {
    console.log(key, url.search);
    res.writeHead(200, { "content-type": "text/csv; charset=utf-8",
                         "content-disposition": `attachment; filename="${state.hostname}-lastgang.csv"` });
    return res.end(historyCsv(Number(url.searchParams.get("from") || 0), Number(url.searchParams.get("to") || 0)));
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
    const code = out?.__status || 200;
    if (out) delete out.__status;
    res.writeHead(code, { "content-type": "application/json" });
    return res.end(JSON.stringify(out));
  }
  const asset = { "/manifest.webmanifest": "application/manifest+json", "/icon.png": "image/png" }[path];
  if (req.method === "GET" && asset && existsSync(join(here, "..", "dist", path))) {
    res.writeHead(200, { "content-type": asset });
    return res.end(readFileSync(join(here, "..", "dist", path)));
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
