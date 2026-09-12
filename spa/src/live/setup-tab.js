import { useState } from "preact/hooks";
import { html } from "../h.js";
import { S, LANGS, getLang, setLang, presetLabel } from "../strings.js";
import { Wifi } from "../steps/wifi.js";
import { FirmwareCard } from "./firmware-card.js";

export function SetupTab({ status, live, presets, theme, onTheme }) {
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
    [S.profile, presetLabel(preset) || status?.meter?.preset],
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
    <${FirmwareCard} status=${status} />
    <div class="card">
      <div class="lbl">${S.language}</div>
      <div class="seg" role="radiogroup">
        ${LANGS.map(([code, label]) => html`
          <button role="radio" aria-checked=${getLang() === code} class=${getLang() === code ? "active" : ""}
            lang=${code} onClick=${() => setLang(code)}>${label}</button>`)}
      </div>
      <p class="hint" style="margin:0">${S.languageHint}</p>
    </div>
    <div class="card">
      <div class="lbl">${S.theme}</div>
      <div class="seg" role="radiogroup">
        ${[["light", S.themeLight], ["dark", S.themeDark]].map(([t, label]) => html`
          <button role="radio" aria-checked=${theme === t} class=${theme === t ? "active" : ""}
            onClick=${() => onTheme(t)}>${label}</button>`)}
      </div>
      <p class="hint" style="margin:0">${S.themeHint}</p>
    </div>`;
}
