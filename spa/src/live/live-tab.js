import { useEffect, useState } from "preact/hooks";
import { html } from "../h.js";
import { S } from "../strings.js";
import { api } from "../api.js";

// Polls /api/live + /api/ring at 0.1 Hz -- fast enough to feel live, slow enough not to matter on
// battery-backed phones or the device's own web server. Only runs while this tab is mounted.
const POLL_MS = 10000;

export function LiveTab() {
  const [live, setLive] = useState(null);
  const [ring, setRing] = useState(null);
  const [err, setErr] = useState(null);

  useEffect(() => {
    let stop = false;
    async function tick() {
      try {
        const [l, r] = await Promise.all([api.live(), api.ring()]);
        if (stop) return;
        setLive(l); setRing(r); setErr(null);
      } catch (e) { if (!stop) setErr(String(e.message || e)); }
    }
    tick();
    const id = setInterval(tick, POLL_MS);
    return () => { stop = true; clearInterval(id); };
  }, []);

  const age = live?.age;
  // no_data is the firmware's verdict (same one that turns the LED red), so the badge and the LED
  // never disagree -- age alone stays null forever when no frame has ever arrived.
  const badge = live?.key_invalid ? ["err", S.keyWrong]
    : live?.no_data ? ["err", S.noData]
    : age == null ? ["warn", S.waitingData]
    : ["ok", `${S.dataOk} (-${age}s)`];
  const hint = live?.key_invalid ? S.keyWrongHint : live?.no_data ? S.noDataHint : null;
  const hasData = live && age != null && !live.key_invalid;

  // Per-phase power isn't in /api/live (the meter descriptor decides which named registers exist,
  // and single-phase presets have none) -- take it from the ring's latest sample instead, which
  // the firmware always fills (0 when the meter has no per-phase registers). Hide the row rather
  // than show three zeroes on a single-phase install.
  const last = ring?.samples?.length ? ring.samples[ring.samples.length - 1] : null;
  const phases = last && (last[2] || last[3] || last[4]) ? last.slice(2, 5) : null;

  return html`
    <div class="card">
      <div class="row" style="justify-content:space-between">
        <span>${S.meterData} ${live?.smid ? html`<span class="mono">${live.smid}</span>` : ""}</span>
        <span class="badge ${badge[0]}" style="flex:0 0 auto">${badge[1]}</span>
      </div>
      ${hint && html`<div class="err">${hint}</div>`}
      ${hasData && html`
        <div class="big">${(live.p ?? 0).toFixed(2)} <small>kW</small></div>
        <div class="kv">
          <b>${S.bezug}</b><span>${live.ei?.toFixed(0) ?? "–"} kWh</span>
          <b>${S.einspeisung}</b><span>${live.eo?.toFixed(0) ?? "–"} kWh</span>
        </div>
        ${phases && html`
          <div class="kv" style="margin-top:6px">
            <b>${S.phase1}</b><span>${(phases[0] / 1000).toFixed(2)} kW</span>
            <b>${S.phase2}</b><span>${(phases[1] / 1000).toFixed(2)} kW</span>
            <b>${S.phase3}</b><span>${(phases[2] / 1000).toFixed(2)} kW</span>
          </div>`}
        <${AreaChart} ring=${ring} />`}
      ${err && html`<div class="err">${err}</div>`}
    </div>`;
}

function AreaChart({ ring }) {
  const samples = ring?.samples;
  if (!samples || samples.length < 2) return null;
  const vals = samples.map(([pi, po]) => pi - po);   // net W per sample, same sign convention as live.p
  const min = Math.min(0, ...vals), max = Math.max(0, ...vals);
  const span = max - min || 1;
  const w = 300, h = 72;
  const pts = vals.map((v, i) => {
    const x = (i / (vals.length - 1)) * w;
    const y = h - ((v - min) / span) * h;
    return `${x.toFixed(1)},${y.toFixed(1)}`;
  });
  const line = pts.join(" ");
  const zeroY = (h - ((0 - min) / span) * h).toFixed(1);
  const area = `0,${zeroY} ${line} ${w},${zeroY}`;
  return html`
    <div class="s" style="margin-top:10px">${S.last60min}</div>
    <svg viewBox="0 0 ${w} ${h}" class="spark" preserveAspectRatio="none">
      <polygon points=${area} class="area-fill" />
      ${min < 0 && max > 0 && html`<line x1="0" y1=${zeroY} x2=${w} y2=${zeroY} class="spark-zero" />`}
      <polyline points=${line} class="spark-line" />
    </svg>`;
}
