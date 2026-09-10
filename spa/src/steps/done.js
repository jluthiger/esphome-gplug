import { useEffect, useState } from "preact/hooks";
import { html } from "../h.js";
import { S } from "../strings.js";
import { api } from "../api.js";

export function Done({ wifi, status }) {
  const [live, setLive] = useState(null);
  const [err, setErr] = useState(null);

  useEffect(() => {
    let stop = false;
    (async () => {
      while (!stop) {
        try { setLive(await api.live()); setErr(null); } catch (e) { setErr(String(e.message || e)); }
        await new Promise((r) => setTimeout(r, 5000));
      }
    })();
    return () => { stop = true; };
  }, []);

  const host = status?.hostname ? `http://${status.hostname}.local/` : null;
  const ip = wifi?.result?.ip ? `http://${wifi.result.ip}/` : null;
  const age = live?.age;
  const badge = live?.key_invalid ? ["err", S.keyWrong]
    : age == null ? ["warn", S.waitingData]
    : age > 60 ? ["err", S.dataStale]
    : ["ok", `${S.dataOk} (-${age}s)`];

  return html`
    <h2>${S.done}</h2>
    <p>${S.doneText}</p>
    ${(host || ip) && html`
      <div class="card">
        <div class="s">${S.reachable}</div>
        ${host && html`<div><a class="mono" href=${host}>${host}</a></div>`}
        ${ip && html`<div><a class="mono" href=${ip}>${ip}</a></div>`}
      </div>`}
    <div class="card">
      <div class="row" style="justify-content:space-between">
        <span>${S.meterData} ${live?.smid ? html`<span class="mono">${live.smid}</span>` : ""}</span>
        <span class="badge ${badge[0]}" style="flex:0 0 auto">${badge[1]}</span>
      </div>
      ${live && age != null && !live.key_invalid && html`
        <div class="big">${(live.p ?? 0).toFixed(2)} <small>kW</small></div>
        <div class="kv">
          <b>${S.bezug}</b><span>${live.ei?.toFixed(0) ?? "–"} kWh</span>
          <b>${S.einspeisung}</b><span>${live.eo?.toFixed(0) ?? "–"} kWh</span>
        </div>`}
      ${err && html`<div class="err">${err}</div>`}
    </div>
    <div class="nav"><a href="/" style="flex:1"><button class="primary" style="width:100%">${S.openLive}</button></a></div>`;
}
