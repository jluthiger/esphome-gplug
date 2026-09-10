// Dev server: serves dist/index.html and fakes the device API so the wizard
// can be exercised without hardware.  `npm run build && npm run dev`
import { createServer } from "node:http";
import { readFileSync, existsSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";

const here = dirname(fileURLToPath(import.meta.url));
const PORT = Number(process.env.PORT || 8080);
const presets = JSON.parse(readFileSync(join(here, "presets.json"), "utf8"));

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
  if (!state.meter) return { age: null };
  const t = (Date.now() - state.t0) / 1000;
  const keyInvalid = state.meter.key !== undefined && state.meter.key?.toLowerCase().startsWith("dead");
  if (keyInvalid) return { age: null, key_invalid: true };
  return {
    smid: "55771146", age: Math.floor(t % 10),
    p: 1.11 + 0.4 * Math.sin(t / 30), p1: 0.4, p2: 0.35, p3: 0.36,
    ei: 19087 + t / 3600, eo: 30836,
  };
}

const routes = {
  "GET /api/status": () => ({ ...state, uptime: (Date.now() - state.t0) / 1000, heap: 123456 }),
  "GET /api/presets": () => presets,
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
  "POST /api/reboot": () => ({ ok: true }),
};

createServer(async (req, res) => {
  const key = `${req.method} ${req.url.split("?")[0]}`;
  let body = "";
  for await (const c of req) body += c;
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
