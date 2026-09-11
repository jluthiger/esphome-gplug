import { html } from "../h.js";
import { S } from "../strings.js";
import { de } from "../fmt.js";

export function LiveTab({ live, ring }) {
  const age = live?.age;
  const hint = live?.key_invalid ? S.keyWrongHint : live?.no_data ? S.noDataHint : null;
  const hasData = live && age != null && !live.key_invalid;
  if (!hasData) return html`<div class="card"><div class="lbl">${S.activePower}</div><p class="hint">${hint || S.waitingData}</p></div>`;

  const net = live.p ?? 0;   // kW, positive = draw from grid
  const samples = ring?.samples || [];
  const vals = samples.map(([pi, po]) => pi - po);
  const max = vals.length ? Math.max(0, ...vals) : 0;

  // Per-phase power isn't in /api/live (the meter descriptor decides which named registers
  // exist) -- take it from the ring's latest sample, which the firmware always fills (0 when the
  // meter has no per-phase registers). Hide the card rather than show three zeroes.
  const last = samples.length ? samples[samples.length - 1] : null;
  const ph = last && (last[2] || last[3] || last[4]) ? last.slice(2, 5) : null;
  const phMax = ph ? Math.max(400, ...ph.map(Math.abs)) : 1;
  const v = live.values || {};
  const volt = (i) => v[`V${i}`] ?? v[`U${i}`];
  const amp = (i) => v[`I${i}`];

  return html`
    <div class="card">
      <div class="between"><span class="lbl">${S.activePower}</span><span class="num" style="font-size:.7rem;color:var(--sub)">−${age} s</span></div>
      <div class="big"><span class="v">${de(Math.abs(net), 2)}</span><span class="u">kW</span></div>
      <div class="dir">${net >= 0 ? S.drawFromGrid : S.feedToGrid}</div>
      ${vals.length >= 2 && html`
        <${Chart} vals=${vals} />
        <div class="axis"><span>${S.ago60}</span><span>${de(max / 1000, 1)} kW ${S.max}</span><span>${S.now}</span></div>`}
    </div>
    <div class="grid2">
      <div class="card stat"><div class="lbl">${S.importLbl}</div><div class="v">${de(live.ei)}</div><div class="u">kWh</div></div>
      <div class="card stat"><div class="lbl orange">${S.exportLbl}</div><div class="v orange">${de(live.eo)}</div><div class="u orange">kWh</div></div>
    </div>
    ${ph && html`
      <div class="card">
        <div class="lbl">${S.phases}</div>
        ${ph.map((w, i) => html`
          <div class="phase">
            <span class="n">L${i + 1}</span>
            <div class="bar"><i style="width:${Math.round(Math.abs(w) / phMax * 100)}%"></i></div>
            <span class="kw">${de(w / 1000, 2)}</span>
            <span class="ui">${volt(i + 1) != null ? de(volt(i + 1), 1) + " V" : ""}${volt(i + 1) != null && amp(i + 1) != null ? " · " : ""}${amp(i + 1) != null ? de(amp(i + 1), 1) + " A" : ""}</span>
          </div>`)}
      </div>`}`;
}

function Chart({ vals }) {
  const w = 320, h = 104;
  const min = Math.min(0, ...vals), max = Math.max(0, ...vals), span = max - min || 1;
  const X = (i) => (i / (vals.length - 1)) * w, Y = (v) => h - ((v - min) / span) * h;
  const pts = vals.map((v, i) => `${X(i).toFixed(1)} ${Y(v).toFixed(1)}`);
  const line = "M" + pts.join(" L ");
  const zeroY = Y(0).toFixed(1);
  return html`
    <svg viewBox="0 0 ${w} ${h}" preserveAspectRatio="none" class="chart">
      <path d="${line} L ${w} ${zeroY} L 0 ${zeroY} Z" class="area" />
      <line x1="0" y1=${zeroY} x2=${w} y2=${zeroY} class="zero" />
      <path d=${line} class="line" />
      <circle cx=${w} cy=${Y(vals[vals.length - 1]).toFixed(1)} r="3.5" class="head" />
    </svg>`;
}
