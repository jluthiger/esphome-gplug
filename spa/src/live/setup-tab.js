import { useState } from "preact/hooks";
import { html } from "../h.js";
import { S } from "../strings.js";
import { Wifi } from "../steps/wifi.js";

export function SetupTab({ status, live, presets, dim, onToggleDim }) {
  const [showWifi, setShowWifi] = useState(false);
  const [wifiVal, setWifiVal] = useState({
    ssid: status?.wifi?.ssid || "",
    psk: "",
    result: status?.wifi?.connected ? status.wifi : null,
  });

  if (showWifi) {
    return html`<${Wifi} value=${wifiVal} onChange=${setWifiVal}
      onBack=${() => setShowWifi(false)} onNext=${() => setShowWifi(false)} />`;
  }

  const wifi = wifiVal.result || (status?.wifi?.connected ? status.wifi : null);
  const preset = presets?.find((p) => p.id === status?.meter?.preset);
  const conn = [
    [S.host, status?.hostname ? `${status.hostname}.local` : null],
    [S.ip, wifi?.ip],
    [S.wlan, wifi ? `${wifi.ssid} · ${wifi.rssi} dBm` : S.offline],
    [S.profile, preset?.name || status?.meter?.preset],
    [S.firmware, status?.version],
  ].filter(([, v]) => v);

  const encrypted = status?.meter?.encrypted;
  // The key itself never leaves the device (no accessor exists); only its validity verdict does.
  const keyBadge = live?.key_invalid ? ["err", S.keyInvalidBadge] : live?.age != null ? ["ok", S.keySet] : ["warn", S.keySet];

  return html`
    <div class="card">
      <div class="lbl" style="margin-bottom:12px">${S.setupConn}</div>
      <div class="kv">${conn.map(([k, v]) => html`<b>${k}</b><span>${v}</span>`)}</div>
      <p style="margin:14px 0 0"><button onClick=${() => setShowWifi(true)}>${S.changeWifi}</button></p>
    </div>
    ${encrypted && html`
      <div class="card">
        <div class="lbl">${S.setupKey}</div>
        <div class="keymask"><span class="m">•••• •••• •••• •••• •••• •••• •••• ••••</span><span class="badge ${keyBadge[0]}">${keyBadge[1]}</span></div>
        <p class="hint" style="margin:8px 0 0">${S.keyNote}</p>
      </div>`}
    <div class="card switchrow">
      <div><div class="t">${S.dimMode}</div><div class="s">${S.dimModeHint}</div></div>
      <button class="switch ${dim ? "on" : ""}" onClick=${onToggleDim}><span></span></button>
    </div>`;
}
