// Thin wrapper around the device webservice. All endpoints are relative so the SPA
// works from the captive-portal AP (192.168.4.1) as well as from the STA address.
import { S } from "./strings.js";

const BASE = "";

async function req(method, path, body, ms = 8000) {
  const ctrl = new AbortController();
  const t = setTimeout(() => ctrl.abort(), ms);
  try {
    let r;
    try {
      r = await fetch(BASE + path, {
        method,
        headers: body ? { "content-type": "application/json" } : undefined,
        body: body ? JSON.stringify(body) : undefined,
        signal: ctrl.signal,
      });
    } catch (e) {
      // Browser-level fetch failure: AbortError from our own timeout, or a network error
      // (offline, DNS, refused). Neither message is meaningful to a non-technical user.
      throw new Error(e.name === "AbortError" ? S.errTimeout : S.errNetwork);
    }
    const ct = r.headers.get("content-type") || "";
    const isJson = ct.includes("json");
    if (!r.ok) {
      let detail = "";
      if (isJson) { try { detail = (await r.json()).error || ""; } catch { /* not valid JSON, ignore */ } }
      throw new Error(detail ? `${S.errServer}: ${detail}` : `${S.errServer} (HTTP ${r.status})`);
    }
    return isJson ? r.json() : r.text();
  } finally {
    clearTimeout(t);
  }
}

export const api = {
  status: () => req("GET", "/api/status"),
  presets: () => req("GET", "/api/presets"),
  wifiScan: () => req("GET", "/api/wifi/scan", null, 15000),
  setWifi: (cfg) => req("POST", "/api/config/wifi", cfg),
  setHardware: (cfg) => req("POST", "/api/config/hardware", cfg),
  setMeter: (cfg) => req("POST", "/api/config/meter", cfg),
  live: () => req("GET", "/api/live", null, 4000),
  ring: () => req("GET", "/api/ring", null, 4000),
  reboot: () => req("POST", "/api/reboot"),
};
