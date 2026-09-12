import { useRef } from "preact/hooks";
import { html } from "../h.js";
import { S } from "../strings.js";
import { num } from "../fmt.js";
import { DiagCard, diagInfo, diagOf } from "../diag.js";
import { lastHour } from "../hour.js";
import { useBox } from "../box.js";

export function LiveTab({ live, ring, day }) {
  const age = live?.age;
  const d = diagOf(live);
  // A setup problem (silent line, wrong profile or key) gets a card with the way back into the
  // wizard; no automatic redirect -- "no data" is as often a cable or a customer port the utility
  // hasn't enabled yet, and a correct setup must not be sent round the wizard again.
  if (diagInfo(d)) return html`<${DiagCard} diag=${d} />`;
  const hasData = live && age != null && !live.key_invalid;
  if (!hasData) return html`<div class="card"><div class="lbl">${S.activePower}</div><p class="hint">${S.waitingData}</p></div>`;

  const net = live.p ?? 0;   // kW, positive = draw from grid
  const samples = ring?.samples || [];
  // The hour comes from the ring where it can and from the stored 15-min records where it cannot,
  // which is what keeps this chart from being empty for an hour after every restart.
  const hour = lastHour(ring, day);
  const vals = hour.vals;
  const known = vals.filter((v) => v !== null);
  const max = known.length ? Math.max(0, ...known) : 0;

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
      <div class="big"><span class="v ${net >= 0 ? "imp" : "exp"}">${num(Math.abs(net), 2)}</span><span class="u">kW</span></div>
      <div class="dir ${net >= 0 ? "imp" : "exp"}">${net >= 0 ? S.drawFromGrid : S.feedToGrid}</div>
      ${known.length >= 2 && html`
        <${Chart} vals=${vals} />
        <div class="axis"><span>${S.ago60}</span><span>${num(max / 1000, 1)} kW ${S.max}</span><span>${S.now}</span></div>
        ${hour.fromStore > 0 && html`<p class="hint" style="margin:8px 0 0">${S.histFromStore}</p>`}`}
    </div>
    <div class="grid2">
      <div class="card stat"><div class="lbl imp">${S.importLbl}</div><div class="v imp">${num(live.ei)}</div><div class="u imp">kWh</div></div>
      <div class="card stat"><div class="lbl exp">${S.exportLbl}</div><div class="v exp">${num(live.eo)}</div><div class="u exp">kWh</div></div>
    </div>
    ${ph && html`
      <div class="card">
        <div class="lbl">${S.phases}</div>
        ${ph.map((w, i) => html`
          <div class="phase">
            <span class="n">L${i + 1}</span>
            <div class="bar"><i class=${w >= 0 ? "imp" : "exp"} style="width:${Math.round(Math.abs(w) / phMax * 100)}%"></i></div>
            <span class="kw ${w >= 0 ? "imp" : "exp"}">${num(w / 1000, 2)}</span>
            <span class="ui">${volt(i + 1) != null ? num(volt(i + 1), 1) + " V" : ""}${volt(i + 1) != null && amp(i + 1) != null ? " · " : ""}${amp(i + 1) != null ? num(amp(i + 1), 1) + " A" : ""}</span>
          </div>`)}
      </div>`}`;
}

// `vals` may contain nulls: an interval the device has no record of (it was off, or the meter
// delivered nothing). Those are drawn as a break in the line rather than a line through zero,
// which would read as "0 kW" -- a measurement the device never made.
function Chart({ vals }) {
  // Falls back to the CSS size until the first measurement lands, one frame later.
  const svg = useRef(null);
  const [w, h] = useBox(svg, 320, 104);
  const known = vals.filter((v) => v !== null);
  const min = Math.min(0, ...known), max = Math.max(0, ...known), span = max - min || 1;
  const X = (i) => (i / (vals.length - 1)) * w, Y = (v) => h - ((v - min) / span) * h;
  const zeroY = Y(0).toFixed(1);

  // Split into runs that are both contiguous (no nulls) and single-signed, inserting the exact
  // zero crossing between two samples of opposite sign. Each run can then be filled and stroked in
  // its own direction colour: above the line is drawn from the grid, below it is fed back.
  const runs = [];
  let cur = null;
  const start = (sign, pts) => { cur = { sign, pts }; runs.push(cur); };
  vals.forEach((v, i) => {
    if (v === null) { cur = null; return; }
    const sign = v >= 0 ? 1 : -1;
    const pt = `${X(i).toFixed(1)} ${Y(v).toFixed(1)}`;
    if (!cur) return start(sign, [pt]);
    if (sign !== cur.sign) {
      const prev = vals[i - 1];
      const f = Math.abs(prev) / (Math.abs(prev) + Math.abs(v) || 1);   // where the line meets zero
      const xc = (X(i - 1) + (X(i) - X(i - 1)) * f).toFixed(1);
      cur.pts.push(`${xc} ${zeroY}`);
      start(sign, [`${xc} ${zeroY}`, pt]);
      return;
    }
    cur.pts.push(pt);
  });

  const lastIdx = vals.length - 1;
  const cls = (r) => (r.sign >= 0 ? "imp" : "exp");
  return html`
    <svg ref=${svg} viewBox="0 0 ${w} ${h}" preserveAspectRatio="none" class="chart">
      ${runs.map((r) => {
        const line = "M" + r.pts.join(" L ");
        const x0 = r.pts[0].split(" ")[0], x1 = r.pts[r.pts.length - 1].split(" ")[0];
        return html`<path d="${line} L ${x1} ${zeroY} L ${x0} ${zeroY} Z" class="area ${cls(r)}" />`;
      })}
      <line x1="0" y1=${zeroY} x2=${w} y2=${zeroY} class="zero" />
      ${runs.map((r) => html`<path d=${"M" + r.pts.join(" L ")} class="line ${cls(r)}" />`)}
      ${vals[lastIdx] !== null && html`<circle cx=${w} cy=${Y(vals[lastIdx]).toFixed(1)} r="3.5" class="head" />`}
    </svg>`;
}
