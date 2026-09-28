import { useState } from "preact/hooks";
import { html } from "../h.js";
import { S, LANGS, getLang, setLang, presetLabel } from "../strings.js";
import { Wifi } from "../steps/wifi.js";
import { FirmwareCard } from "./firmware-card.js";
import { LogCard } from "./log-card.js";
import { MemCard } from "./mem-card.js";
import { MqttCard } from "./mqtt-card.js";
import { Collapsible } from "./collapsible.js";

export function SetupTab({ status, live, presets, theme, onTheme, wide }) {
  const [showWifi, setShowWifi] = useState(false);
  // The MQTT card's loaded config and unsaved draft: kept here because a closed card unmounts.
  const [mqtt, setMqtt] = useState(null);
  const [wifiVal, setWifiVal] = useState({
    ssid: status?.wifi?.ssid || "",
    psk: "",
    result: status?.wifi?.connected ? status.wifi : null,
  });

  if (showWifi) {
    const form = html`<${Wifi} value=${wifiVal} onChange=${setWifiVal}
      onBack=${() => setShowWifi(false)} onNext=${() => setShowWifi(false)} />`;
    // The Wi-Fi form is a short phone-shaped flow; on a wide screen it stays that narrow.
    return wide ? html`<div class="narrow">${form}</div>` : form;
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

  const device = html`
    <${Collapsible} id="conn" title=${S.setupConn} open summary=${conn[0]?.[1]}>
      <div class="kv">${conn.map(([k, v]) => html`<b>${k}</b><span>${v}</span>`)}</div>
      <p style="margin:14px 0 0"><button onClick=${() => setShowWifi(true)}>${S.changeWifi}</button></p>
    <//>
    ${encrypted && html`
      <${Collapsible} id="key" title=${S.setupKey} open locked=${live?.key_invalid}
        summary=${html`<span class="badge ${keyBadge[0]}">${keyBadge[1]}</span>`}>
        <div class="keymask"><span class="m">•••• •••• •••• •••• •••• •••• •••• ••••</span><span class="badge ${keyBadge[0]}">${keyBadge[1]}</span></div>
        <p class="hint" style="margin:8px 0 0">${S.keyNote}</p>
      <//>`}
    <${MqttCard} status=${status} live=${live} state=${mqtt} setState=${setMqtt} />
    <${FirmwareCard} status=${status} />
    <${MemCard} status=${status} />`;
  const prefs = html`
    <${Collapsible} id="lang" title=${S.language} summary=${LANGS.find(([code]) => code === getLang())?.[1]}>
      <div class="seg" role="radiogroup">
        ${LANGS.map(([code, label]) => html`
          <button role="radio" aria-checked=${getLang() === code} class=${getLang() === code ? "active" : ""}
            lang=${code} onClick=${() => setLang(code)}>${label}</button>`)}
      </div>
      <p class="hint" style="margin:0">${S.languageHint}</p>
    <//>
    <${Collapsible} id="theme" title=${S.theme} summary=${theme === "light" ? S.themeLight : S.themeDark}>
      <div class="seg" role="radiogroup">
        ${[["light", S.themeLight], ["dark", S.themeDark]].map(([t, label]) => html`
          <button role="radio" aria-checked=${theme === t} class=${theme === t ? "active" : ""}
            onClick=${() => onTheme(t)}>${label}</button>`)}
      </div>
      <p class="hint" style="margin:0">${S.themeHint}</p>
    <//>`;

  // Every card collapses (collapsible.js). Connection and key start open: the addresses are what a
  // user most often comes here for, and the key is the one a wrong setup breaks. Language and
  // theme start closed with the current choice as summary, being set once per browser.
  //
  // One column on every width, in the same order as on a phone. The former two-column wide layout
  // put preferences beside device matters and the log below, so the order changed with the width;
  // the log still becomes a table on a wide screen, where it has the room.
  const body = html`${device}<${LogCard} wide=${wide} />${prefs}`;
  return wide ? html`<div class="devcol">${body}</div>` : body;
}
