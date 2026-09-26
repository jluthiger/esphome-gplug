// Thin wrapper around the device webservice. All endpoints are relative so the SPA
// works from the captive-portal AP (192.168.4.1) as well as from the STA address.
import { S } from "./strings.js";

const BASE = "";

// The device answers a rejected request with a short English token (gplug_smi.cpp). Most of them
// are malformed-request diagnostics the UI cannot actually produce, and those still reach the user
// verbatim after the localized "error from the device" prefix -- better a technical token than a
// wrong translation. The few a user can genuinely hit get a real sentence in their own language.
const DEVICE_ERRORS = {
  "no stored key": "errNoStoredKey",
  "key must be 32 hex chars": "keyInvalid",
  "auth_key must be 32 hex chars": "keyInvalid",
  "history disabled": "errHistoryOff",
  "auth": "fwAuth",
  "no update": "fwRelGone",
  // POST /api/config/mqtt field checks; template errors (tpl_*) carry field + pos and are worded by
  // the MQTT card itself.
  "host": "errMqttHost",
  "port": "errMqttPort",
  "period": "errMqttPeriod",
  "client_id": "errMqttLong",
  "user": "errMqttLong",
  "password": "errMqttLong",
};

async function req(method, path, body, ms = 8000, headers = undefined) {
  const ctrl = new AbortController();
  const t = setTimeout(() => ctrl.abort(), ms);
  try {
    let r;
    try {
      r = await fetch(BASE + path, {
        method,
        headers: body ? { "content-type": "application/json", ...headers } : headers,
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
      let detail = "", parsed = null;
      if (isJson) { try { parsed = await r.json(); detail = parsed.error || ""; } catch { /* not valid JSON, ignore */ } }
      const known = DEVICE_ERRORS[detail];
      const err = new Error(known ? S[known] : detail ? `${S.errServer}: ${detail}` : `${S.errServer} (HTTP ${r.status})`);
      err.body = parsed;   // e.g. {error, field, pos} for a rejected MQTT template
      throw err;
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
  checkKey: (key) => req("POST", "/api/key/check", { key }),
  mqttConfig: () => req("GET", "/api/config/mqtt"),
  setMqtt: (cfg) => req("POST", "/api/config/mqtt", cfg),
  live: () => req("GET", "/api/live", null, 4000),
  ring: () => req("GET", "/api/ring", null, 4000),
  // Bulk read of the flash history; range=year scans the whole partition on the device, so it gets
  // a longer timeout than the polled endpoints.
  history: (range) => req("GET", "/api/history?range=" + range, null, 10000),
  // Full-resolution CSV download (Content-Disposition: attachment): navigated to, not fetched, so
  // the browser streams it straight to a file. from/to are quarter-hour indices, both optional --
  // leaving them out is the only way to get records that never got a timestamp.
  historyCsvUrl: (from, to) => {
    const q = from != null ? `?from=${from}&to=${to}` : "";
    return BASE + "/api/history.csv" + q;
  },
  frames: () => req("GET", "/api/frames", null, 4000),
  log: () => req("GET", "/api/log", null, 4000),
  heap: () => req("GET", "/api/heap", null, 4000),
  frameRaw: (i) => req("GET", `/api/frames/${i}/raw`, null, 4000),
  framePlain: (i) => req("GET", `/api/frames/${i}/plain`, null, 4000),
  reboot: () => req("POST", "/api/reboot"),
  // Install from the release (GET/POST /api/update*). Reading the state costs the device nothing;
  // only check() makes it contact github.io. install() carries the OTA password up front like the
  // file upload; the device answers a wrong one with a bare 401 (no browser login dialog).
  updateInfo: () => req("GET", "/api/update", null, 4000),
  updateCheck: () => req("POST", "/api/update/check"),
  updateInstall: (password) => req("POST", "/api/update/install", null, 8000,
    password ? { Authorization: basicAuth(password) } : undefined),
  firmware: uploadFirmware,
  firmwareAuth: checkFirmwareAuth,
};

// /update takes HTTP Basic auth, user fixed to "admin" (see gplug_smi's ota_password). Two browser
// quirks shape how it is sent:
//  * A request that draws a 401 carrying WWW-Authenticate makes the browser pop its own login
//    dialog -- unless the credentials came in through open(user, password).
//  * But with open(user, password) the browser sends the request *without* credentials first and
//    only repeats it after the 401. For a 1 MB body that is fatal: ESPHome's auth middleware rejects
//    every body chunk separately, the upload crawls for minutes and the device's web server is tied
//    up until the connection times out (seen on hardware).
// So the password is checked with an empty POST via open(user, password) -- instant 401 when wrong,
// no dialog; when right, the OTA handler answers "Update Failed!" without touching flash -- and the
// upload itself then sends the header up front, never drawing a 401.
function checkFirmwareAuth(password) {
  return new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest();
    xhr.open("POST", BASE + "/update", true, "admin", password);
    xhr.timeout = 8000;
    xhr.onload = () => resolve(xhr.status !== 401);
    xhr.onerror = () => reject(new Error(S.errNetwork));
    xhr.ontimeout = () => reject(new Error(S.errTimeout));
    xhr.send("");
  });
}

function basicAuth(password) {
  const bytes = new TextEncoder().encode("admin:" + password);   // UTF-8-safe btoa
  return "Basic " + btoa(String.fromCharCode(...bytes));
}

// POST the image to ESPHome's ota.web_server handler (/update, multipart field "update"). XHR, not
// fetch: fetch has no upload progress. Resolves with the HTTP status and ESPHome's plain-text verdict
// ("Update Successful!" / "Update Failed!"); the device reboots itself right after a success.
function uploadFirmware(file, password, onProgress) {
  return new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest();
    xhr.open("POST", BASE + "/update");
    if (password) xhr.setRequestHeader("Authorization", basicAuth(password));
    xhr.timeout = 180000;
    xhr.upload.onprogress = (e) => { if (e.lengthComputable) onProgress(e.loaded / e.total); };
    xhr.onload = () => resolve({ status: xhr.status, text: xhr.responseText || "" });
    xhr.onerror = () => reject(new Error(S.fwConnLost));
    xhr.ontimeout = () => reject(new Error(S.errTimeout));
    const fd = new FormData();
    fd.append("update", file, file.name);
    xhr.send(fd);
  });
}
