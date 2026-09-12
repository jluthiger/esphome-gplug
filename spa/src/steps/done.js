import { useEffect, useState } from "preact/hooks";
import { html } from "../h.js";
import { S } from "../strings.js";
import { api } from "../api.js";
import { num } from "../fmt.js";
import { DiagCard, diagOf } from "../diag.js";

// End of the wizard: don't hand over to the Live view until the meter has actually been read. A
// wrong port, profile or key shows up here, while the user is still in setup, with a button back
// to the step that fixes it. The firmware grants 60 s after the meter step before it calls a
// verdict ("waiting" until then), because some meters push only every 10-30 s.
const POLL_MS = 2000;

export function Done({ onOpenLive }) {
  const [live, setLive] = useState(null);
  const [t0] = useState(Date.now());
  const [now, setNow] = useState(Date.now());

  useEffect(() => {
    let stop = false;
    async function tick() {
      try { const l = await api.live(); if (!stop) setLive(l); } catch { /* keep polling */ }
      if (!stop) setNow(Date.now());
    }
    tick();
    const id = setInterval(tick, POLL_MS);
    return () => { stop = true; clearInterval(id); };
  }, []);

  const d = diagOf(live);
  const ok = d === "ok";
  const waiting = !live || d === "waiting";

  return html`
    <h2>${S.done}</h2>
    ${(waiting || ok) && html`<div class="card">
      <div class="lbl">${S.checkTitle}</div>
      ${waiting && html`
        <p style="margin:10px 0 0"><span class="spin"></span> ${S.checkWaiting}
          <span class="num muted"> ${Math.round((now - t0) / 1000)} s</span></p>
        <p class="hint" style="margin:6px 0 0">${S.checkWaitingHint}</p>`}
      ${ok && html`
        <p style="margin:10px 0 0"><span class="badge ok">${S.checkOk}</span></p>
        <div class="kv" style="margin-top:12px">
          ${live.smid && html`<b>${S.meterId}</b><span>${live.smid}</span>`}
          ${live.p != null && html`<b>${S.activePower}</b><span>${num(live.p, 2)} kW</span>`}
          <b>${S.values}</b><span>${Object.keys(live.values || {}).length}</span>
        </div>`}
    </div>`}
    ${!waiting && !ok && html`<${DiagCard} diag=${d} />`}
    ${ok && html`<p>${S.doneText}</p>`}
    <div class="nav">
      ${ok
        ? html`<button class="primary" style="width:100%" onClick=${onOpenLive}>${S.openLive}</button>`
        : html`<button style="width:100%" onClick=${onOpenLive}>${S.continueAnyway}</button>`}
    </div>`;
}
