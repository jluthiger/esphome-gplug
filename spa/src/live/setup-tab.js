import { useEffect, useState } from "preact/hooks";
import { html } from "../h.js";
import { S } from "../strings.js";
import { api } from "../api.js";
import { Wifi } from "../steps/wifi.js";

const DIM_KEY = "gplug.dim";

function applyDim(on) {
  document.documentElement.classList.toggle("dim", on);
}

export function SetupTab({ status, onOpenSetup }) {
  const [live, setLive] = useState(null);
  const [showWifi, setShowWifi] = useState(false);
  const [wifiVal, setWifiVal] = useState({
    ssid: status?.wifi?.ssid || "",
    psk: "",
    result: status?.wifi?.connected ? status.wifi : null,
  });
  const [dim, setDim] = useState(() => {
    try { return localStorage.getItem(DIM_KEY) === "1"; } catch { return false; }
  });

  useEffect(() => { applyDim(dim); }, []);   // re-apply on mount (e.g. after a reload)

  useEffect(() => {
    let stop = false;
    api.live().then((l) => { if (!stop) setLive(l); }).catch(() => {});
    return () => { stop = true; };
  }, []);

  function toggleDim() {
    const next = !dim;
    setDim(next);
    applyDim(next);
    try { localStorage.setItem(DIM_KEY, next ? "1" : "0"); } catch { /* private mode, ignore */ }
  }

  if (showWifi) {
    return html`<${Wifi} value=${wifiVal} onChange=${setWifiVal}
      onBack=${() => setShowWifi(false)} onNext=${() => setShowWifi(false)} />`;
  }

  const host = status?.hostname ? `http://${status.hostname}.local/` : null;
  const ip = wifiVal.result?.ip ? `http://${wifiVal.result.ip}/` : status?.wifi?.ip ? `http://${status.wifi.ip}/` : null;

  const encrypted = status?.meter?.encrypted;
  const keyBadge = !encrypted ? ["", S.keyUnset]
    : live?.key_invalid ? ["err", S.keyInvalidBadge]
    : live?.age != null ? ["ok", S.keySet]
    : ["warn", S.keySet];

  return html`
    ${(host || ip) && html`
      <div class="card">
        <div class="s">${S.setupConn}</div>
        ${host && html`<div><a class="mono" href=${host}>${host}</a></div>`}
        ${ip && html`<div><a class="mono" href=${ip}>${ip}</a></div>`}
        <p><button onClick=${() => setShowWifi(true)}>${S.changeWifi}</button></p>
      </div>`}
    ${encrypted && html`
      <div class="card">
        <div class="row" style="justify-content:space-between">
          <span>${S.setupKey}</span>
          <span class="badge ${keyBadge[0]}">${keyBadge[1]}</span>
        </div>
        <p class="hint">${S.keyNote}</p>
      </div>`}
    <div class="card dimrow">
      <div>
        <div class="t">${S.dimMode}</div>
        <div class="s">${S.dimModeHint}</div>
      </div>
      <button class="switch ${dim ? "on" : ""}" onClick=${toggleDim}><span></span></button>
    </div>
    <div class="nav"><button style="width:100%" onClick=${onOpenSetup}>${S.openSetup}</button></div>`;
}
