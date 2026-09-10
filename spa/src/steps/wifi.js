import { useEffect, useState } from "preact/hooks";
import { html } from "../h.js";
import { S } from "../strings.js";
import { api } from "../api.js";

export function Wifi({ value, onChange, onNext, onBack }) {
  const [nets, setNets] = useState(null);
  const [scanning, setScanning] = useState(false);
  const [manual, setManual] = useState(false);
  const [state, setState] = useState(value.result ? "ok" : "idle"); // idle | connecting | ok | fail
  const [err, setErr] = useState("");

  async function scan() {
    setScanning(true); setErr("");
    try {
      const r = await api.wifiScan();
      const seen = new Set();
      setNets(r.filter((n) => n.ssid && !seen.has(n.ssid) && seen.add(n.ssid)).sort((a, b) => b.rssi - a.rssi));
    } catch (e) { setErr(String(e.message || e)); }
    setScanning(false);
  }
  useEffect(() => { if (!value.result) scan(); }, []);

  async function connect() {
    setState("connecting"); setErr("");
    try {
      await api.setWifi({ ssid: value.ssid, psk: value.psk });
      // Poll status until STA is connected or timeout. Device keeps the AP up meanwhile.
      const t0 = Date.now();
      while (Date.now() - t0 < 30000) {
        await new Promise((r) => setTimeout(r, 2000));
        let st;
        try { st = await api.status(); } catch { continue; }
        if (st.wifi?.connected) { onChange({ ...value, result: st.wifi }); setState("ok"); return; }
        if (st.wifi?.error) break;
      }
      setState("fail"); setErr(S.connectFailed);
    } catch (e) { setState("fail"); setErr(String(e.message || e)); }
  }

  const result = value.result;
  const canConnect = value.ssid && state !== "connecting";

  return html`
    <h2>${S.wifi}</h2>
    <p>${S.wifiText}</p>
    ${!manual && state !== "ok" && html`
      <div class="row">
        <button onClick=${scan} disabled=${scanning}>${scanning ? html`<span class="spin"></span>` : S.scan}</button>
        <button onClick=${() => setManual(true)}>${S.manual}</button>
      </div>
      ${nets && nets.map((n) => html`
        <div class="card click wifi ${value.ssid === n.ssid ? "sel" : ""}"
             onClick=${() => onChange({ ...value, ssid: n.ssid })}>
          <span class="t">${n.ssid}</span>
          <span class="s">${n.secure ? "🔒 " : ""}${n.rssi} dBm</span>
        </div>`)}`}
    ${manual && state !== "ok" && html`<label>${S.ssid}</label>
      <input type="text" value=${value.ssid} onInput=${(e) => onChange({ ...value, ssid: e.target.value })} />`}
    ${value.ssid && state !== "ok" && html`
      <label>${S.password} – ${value.ssid}</label>
      <input type="password" value=${value.psk} autocomplete="off"
        onInput=${(e) => onChange({ ...value, psk: e.target.value })} />`}
    ${state === "connecting" && html`<p><span class="spin"></span> ${S.connecting}</p>`}
    ${state === "ok" && result && html`<div class="card">
      <span class="badge ok">${S.connected}</span>
      <div class="kv" style="margin-top:8px">
        <b>SSID</b><span>${result.ssid}</span>
        <b>IP</b><span class="mono">${result.ip}</span>
        <b>RSSI</b><span>${result.rssi} dBm</span>
      </div>
      <p><button onClick=${() => { onChange({ ...value, result: null }); setState("idle"); scan(); }}>${S.wifi} ändern</button></p>
    </div>`}
    ${err && html`<div class="err">${err}</div>`}
    <div class="nav">
      <button onClick=${onBack}>${S.back}</button>
      ${state === "ok"
        ? html`<button class="primary" onClick=${onNext}>${S.next}</button>`
        : html`<button class="primary" disabled=${!canConnect} onClick=${connect}>Verbinden</button>`}
    </div>
    ${state !== "ok" && html`<p style="text-align:center">
      <button onClick=${onNext} style="background:none;color:var(--muted);font-weight:400">${S.skip}</button></p>`}`;
}
