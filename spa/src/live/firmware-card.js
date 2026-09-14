import { useRef, useState } from "preact/hooks";
import { html } from "../h.js";
import { Collapsible } from "./collapsible.js";
import { S } from "../strings.js";
import { api } from "../api.js";

// Firmware update from the phone: pick an ESPHome OTA image, check its header here, upload it to
// the device's /update handler, then wait for the reboot and confirm a different build came up.
// The only update path once onboarding is done that needs no ESPHome tooling.
//
// A plain restart lives here too: it is the same wait for the device to come back, and sharing the
// phase state means a restart can never be started in the middle of an upload.

const APP_SLOT = 0x160000;         // app0/app1 size in gplug.yaml's partition table (1408 kB)
const CHIP_ESP32C3 = 5;            // esp_image_header_t.chip_id
const APP_DESC_MAGIC = 0xabcd5432; // esp_app_desc_t.magic_word, at 0x20 in an app image only
const ELF_SHA_OFF = 0xb0;          // esp_app_desc_t.app_elf_sha256: 0x20 + 144, 32 bytes
const REBOOT_TIMEOUT_MS = 120000;  // an OTA reboot also verifies and switches the image
const RESTART_TIMEOUT_MS = 60000;
const CONFIRM_MS = 10000;          // how long "Really restart?" waits before it takes itself back

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// Returns a string *key* for a rejection rather than the sentence itself, so the message follows
// a language switch made while the file is still selected (see msgText below).
// An ESP-IDF app image starts with a 24-byte image header (magic 0xE9, chip id at 12) and one
// segment header, then esp_app_desc_t. A factory image also starts with 0xE9 -- it is the
// bootloader at offset 0 -- but carries no app descriptor there, which is how it is told apart.
async function inspect(file) {
  if (file.size > APP_SLOT) return { error: "fwTooBig" };
  const b = new Uint8Array(await file.slice(0, 0xd0).arrayBuffer());
  if (b.length < 0xd0 || b[0] !== 0xe9) return { error: "fwNotImage" };
  if (new DataView(b.buffer).getUint32(0x20, true) !== APP_DESC_MAGIC) return { error: "fwFactory" };
  if ((b[12] | (b[13] << 8)) !== CHIP_ESP32C3) return { error: "fwWrongChip" };
  const str = (o) => new TextDecoder().decode(b.subarray(o, o + 32)).replace(/\0[\s\S]*$/, "");
  // esp_app_desc_t.app_elf_sha256, the image's own identity. The device reports the same first
  // 16 hex digits for whatever it is running (`app` in /api/status), so after the reboot the two
  // can be compared directly: equal means this exact file is running.
  const sha = [...b.subarray(ELF_SHA_OFF, ELF_SHA_OFF + 8)].map((x) => x.toString(16).padStart(2, "0")).join("");
  return { version: str(0x30), name: str(0x50), app: sha };
}

// Waits for a reboot that began at t0 to finish: resolves with the first /api/status whose uptime
// is younger than the wait, or null on timeout. The 4 s head start is there because the device can
// still answer with its old uptime between accepting the request and actually going down.
async function waitForReboot(t0, timeoutMs) {
  await sleep(4000);
  while (Date.now() - t0 < timeoutMs) {
    try {
      const s = await api.status();
      if (s.uptime * 1000 < Date.now() - t0 + 5000) return s;
    } catch { /* still rebooting */ }
    await sleep(2000);
  }
  return null;
}

const fmtBuild = (t) => t ? new Date(t * 1000).toLocaleString("de-CH", { dateStyle: "medium", timeStyle: "short" }) : null;

export function FirmwareCard({ status }) {
  const input = useRef(null);
  const [file, setFile] = useState(null);
  const [info, setInfo] = useState(null);
  const [password, setPassword] = useState("");
  // idle | picked | upload | reboot | done | same | failed, and for a plain restart:
  // confirm | restart | back | lost
  const [phase, setPhase] = useState("idle");
  const [progress, setProgress] = useState(0);
  // A string key, not the text: text resolved at set-time would freeze the language it was set
  // in. api.js hands up already-translated sentences, which msgText() passes through unchanged.
  const [msg, setMsg] = useState("");
  const msgText = (m) => (m && S[m] !== undefined ? S[m] : m);
  const [build, setBuild] = useState(status?.build);
  const confirmTimer = useRef(null);

  async function pick(e) {
    const f = e.target.files?.[0];
    e.target.value = "";   // picking the same file again must fire change again
    if (!f) return;
    setFile(f); setInfo(await inspect(f)); setPhase("picked"); setMsg("");
  }

  function reset() { setFile(null); setInfo(null); setPhase("idle"); setMsg(""); setProgress(0); }

  async function install() {
    let before = build, beforeApp = status?.app;
    try {
      const st = await api.status();
      before = st.build;
      beforeApp = st.app;
    } catch { /* keep the last known build */ }
    const pw = status?.ota_auth ? password : "";
    setPhase("upload"); setProgress(0); setMsg("");
    let r;
    try {
      if (pw && !(await api.firmwareAuth(pw))) { setPhase("picked"); setMsg("fwAuth"); return; }
      r = await api.firmware(file, pw, setProgress);
    } catch (e) {
      setPhase("failed"); setMsg(e.message); return;
    }
    if (r.status === 401) { setPhase("picked"); setMsg("fwAuth"); return; }
    if (r.status !== 200 || !/success/i.test(r.text)) { setPhase("failed"); setMsg("fwRejected"); return; }

    // Accepted: the device reboots within a second. Wait until it answers again with a fresh uptime.
    setPhase("reboot");
    const s = await waitForReboot(Date.now(), REBOOT_TIMEOUT_MS);
    if (!s) { setPhase("failed"); setMsg("fwNoReturn"); return; }
    setBuild(s.build);
    // Identify the image that came back up. The ELF hash is exact, so it is asked first and both
    // ways round: matching the uploaded file proves success, matching what ran before proves the
    // device fell back. Only a firmware too old to report `app` at all (or one rolled back to such
    // a version) falls through to comparing build timestamps, which cannot see an update that
    // changed only embedded assets.
    if (s.app && info?.app) setPhase(s.app === info.app ? "done" : "same");
    else if (s.app && beforeApp) setPhase(s.app === beforeApp ? "same" : "done");
    else setPhase(s.build && s.build === before ? "same" : "done");
  }

  // Two steps instead of confirm(): a browser dialog is out of place on a phone, and the second
  // step takes itself back so a stray tap later cannot restart the device.
  function askRestart() {
    setPhase("confirm"); setMsg("");
    clearTimeout(confirmTimer.current);
    confirmTimer.current = setTimeout(() => setPhase((p) => (p === "confirm" ? "idle" : p)), CONFIRM_MS);
  }

  async function restart() {
    clearTimeout(confirmTimer.current);
    setPhase("restart"); setMsg("");
    const t0 = Date.now();
    try {
      await api.reboot();
    } catch (e) {
      // The device may drop the connection while answering; only a device that is still up with
      // its old uptime afterwards really refused. waitForReboot() tells the two apart.
      setMsg(e.message);
    }
    const s = await waitForReboot(t0, RESTART_TIMEOUT_MS);
    if (s) { setBuild(s.build); setPhase("back"); setMsg(""); }
    else setPhase("lost");
  }

  const busy = phase === "upload" || phase === "reboot" || phase === "restart";
  const otherName = info?.name && status?.hostname && info.name !== status.hostname;

  // The upload state lives here, above the Collapsible, so closing the card mid-way loses nothing;
  // the card is still held open while busy so the "keep powered" warning stays in view.
  return html`
    <${Collapsible} id="fw" title=${S.fwTitle} summary=${fmtBuild(build)} locked=${busy}>
      <div class="kv">
        ${status?.version && html`<b>${S.fwEsphome}</b><span>${status.version}</span>`}
        ${build ? html`<b>${S.fwBuild}</b><span>${fmtBuild(build)}</span>` : null}
      </div>
      <input ref=${input} type="file" accept=".bin,application/octet-stream" hidden onChange=${pick} />

      ${phase === "idle" && html`
        <p style="margin:14px 0 0"><button onClick=${() => input.current.click()}>${S.fwPick}</button></p>
        <p class="hint" style="margin:8px 0 0">${S.fwHint}</p>
        <p style="margin:18px 0 0"><button onClick=${askRestart}>${S.rsButton}</button></p>`}

      ${phase === "confirm" && html`
        <p class="hint" style="margin:14px 0 0">${S.rsHint}</p>
        <div class="nav">
          <button onClick=${() => { clearTimeout(confirmTimer.current); setPhase("idle"); }}>${S.fwCancel}</button>
          <button class="primary" onClick=${restart}>${S.rsConfirm}</button>
        </div>`}

      ${phase === "picked" && html`
        <div class="fwfile">
          <div class="t">${file.name}</div>
          <div class="s">${(file.size / 1024).toFixed(0)} kB${info?.version ? ` · ESPHome ${info.version} · ${info.name}` : ""}</div>
        </div>
        ${info?.error && html`<div class="err">${msgText(info.error)}</div>`}
        ${!info?.error && otherName && html`<div class="err">${S.fwOtherName(info.name, status.hostname)}</div>`}
        ${!info?.error && status?.ota_auth && html`
          <label>${S.fwPassword}</label>
          <input type="password" autocomplete="current-password" value=${password}
            onInput=${(e) => setPassword(e.target.value)} />`}
        ${msg && html`<div class="err">${msgText(msg)}</div>`}
        <div class="nav">
          <button onClick=${reset}>${S.fwCancel}</button>
          <button class="primary" disabled=${!!info?.error || (status?.ota_auth && !password)} onClick=${install}>${S.fwInstall}</button>
        </div>`}

      ${busy && html`
        <div class="progress"><i style=${`width:${phase === "upload" ? Math.round(progress * 100) : 100}%`}></i></div>
        <div class="between">
          <span class="muted"><span class="spin"></span> ${phase === "upload" ? S.fwUploading : S.fwRebooting}</span>
          ${phase === "upload" && html`<span class="num">${Math.round(progress * 100)} %</span>`}
        </div>
        ${phase !== "restart" && html`<p class="hint" style="margin:8px 0 0">${S.fwKeepPower}</p>`}`}

      ${(phase === "done" || phase === "same") && html`
        <p style="margin:14px 0 0"><span class="badge ${phase === "done" ? "ok" : "warn"}">${phase === "done" ? S.fwDone : S.fwRestarted}</span></p>
        ${phase === "same" && html`<p class="hint">${S.fwSameBuild}</p>`}
        <p style="margin:14px 0 0"><button class="primary" onClick=${() => location.reload()}>${S.fwReload}</button></p>`}

      ${phase === "back" && html`
        <p style="margin:14px 0 0"><span class="badge ok">${S.rsBack}</span></p>
        <p class="hint">${S.rsLogged}</p>
        <p style="margin:14px 0 0"><button class="primary" onClick=${() => location.reload()}>${S.fwReload}</button></p>`}

      ${phase === "lost" && html`
        <div class="err">${msg ? `${S.rsFailed}: ${msgText(msg)}` : S.fwNoReturn}</div>
        <p style="margin:14px 0 0"><button onClick=${reset}>${S.fwCancel}</button></p>`}

      ${phase === "failed" && html`
        <div class="err">${S.fwFailed}: ${msgText(msg)}</div>
        <p style="margin:14px 0 0"><button onClick=${reset}>${S.fwCancel}</button></p>`}
    <//>`;
}
