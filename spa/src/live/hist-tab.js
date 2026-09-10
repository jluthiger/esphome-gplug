import { useState } from "preact/hooks";
import { html } from "../h.js";
import { S } from "../strings.js";
import { de } from "../fmt.js";

// History is the same 1h/10s ring the Live tab draws -- "10 Min"/"60 Min" are client-side slices
// of one /api/ring response, no separate backend endpoint. The register list is the current
// snapshot from /api/live's `values`, named/unit-tagged via the matching preset.
export function HistTab({ live, ring, status, presets }) {
  const [range, setRange] = useState("60");
  const samples = ring?.samples || [];
  const win = samples.slice(-(range === "10" ? 60 : 360));
  const vals = win.map(([pi, po]) => pi - po);
  const preset = presets?.find((p) => p.id === status?.meter?.preset);
  const values = live?.values || {};
  const regs = preset ? preset.obis.filter((o) => values[o.name] != null) : [];

  return html`
    <div class="seg">
      <button class=${range === "10" ? "active" : ""} onClick=${() => setRange("10")}>${S.range10}</button>
      <button class=${range === "60" ? "active" : ""} onClick=${() => setRange("60")}>${S.range60}</button>
    </div>
    <div class="card">
      <div class="between" style="margin-bottom:12px"><span class="lbl">${S.netPower}</span><span class="mono" style="font-size:.66rem;color:var(--muted2)">${S.samples(vals.length)}</span></div>
      ${vals.length >= 2 ? html`<${Bars} vals=${vals} />` : html`<p class="hint">${S.waitingData}</p>`}
      <div class="axis"><span>${range === "10" ? S.ago10 : S.ago60}</span><span>${S.now}</span></div>
    </div>
    ${vals.length >= 2 && html`
      <div class="grid3">
        <${Stat} k=${S.statMax} v=${Math.max(...vals)} />
        <${Stat} k=${S.statAvg} v=${vals.reduce((a, b) => a + b, 0) / vals.length} />
        <${Stat} k=${S.statMin} v=${Math.min(...vals)} />
      </div>`}
    ${regs.length > 0 && html`
      <div class="card">
        <div class="lbl" style="margin-bottom:4px">${S.registers}</div>
        ${regs.map((o) => html`
          <div class="reg"><span class="o">${o.obis.replace(/^\d-\d:/, "")}</span><span class="n">${o.name}</span>
            <span class="v">${de(values[o.name], o.unit === "kWh" || o.unit === "kVArh" ? 3 : o.unit === "W" ? 0 : 2)}</span><span class="u">${o.unit || ""}</span></div>`)}
      </div>`}`;
}

function Stat({ k, v }) {
  return html`<div class="card stat"><div class="lbl">${k}</div><div class="v" style="font-size:1.1rem">${de(v / 1000, 2)}</div><div class="u">kW</div></div>`;
}

function Bars({ vals }) {
  const w = 320, h = 130, buckets = 40;
  const per = Math.ceil(vals.length / buckets);
  const avg = [];
  for (let i = 0; i < vals.length; i += per) { const c = vals.slice(i, i + per); avg.push(c.reduce((a, b) => a + b, 0) / c.length); }
  const maxAbs = Math.max(1, ...avg.map(Math.abs));
  const zero = avg.some((v) => v < 0) ? h * 0.62 : h - 2;
  const bw = w / avg.length;
  return html`
    <svg viewBox="0 0 ${w} ${h}" class="hist">
      <line x1="0" y1=${zero} x2=${w} y2=${zero} class="zero" />
      ${avg.map((v, i) => {
        const bh = Math.max(2, (Math.abs(v) / maxAbs) * (v >= 0 ? zero - 4 : h - zero - 4));
        return html`<rect x=${(i * bw + 1).toFixed(1)} y=${(v >= 0 ? zero - bh : zero).toFixed(1)} width=${(bw - 2).toFixed(1)} height=${bh.toFixed(1)} rx="1.5" class=${v >= 0 ? "up" : "down"} />`;
      })}
    </svg>`;
}
