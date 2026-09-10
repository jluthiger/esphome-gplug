import { useEffect, useState } from "preact/hooks";
import { html } from "../h.js";
import { S } from "../strings.js";
import { api } from "../api.js";

const POLL_MS = 10000;

// History is just the same 1h/10s ring /api/live already needs elsewhere -- no separate backend
// endpoint. "10 Min"/"60 Min" are both client-side slices of the same 360-sample response.
export function HistTab({ status, presets }) {
  const [ring, setRing] = useState(null);
  const [values, setValues] = useState(null);   // one-shot register snapshot, not polled
  const [err, setErr] = useState(null);
  const [range, setRange] = useState("60");

  useEffect(() => {
    let stop = false;
    async function tick() {
      try { const r = await api.ring(); if (!stop) { setRing(r); setErr(null); } }
      catch (e) { if (!stop) setErr(String(e.message || e)); }
    }
    tick();
    const id = setInterval(tick, POLL_MS);
    return () => { stop = true; clearInterval(id); };
  }, []);

  useEffect(() => {
    api.live().then((l) => setValues(l.values || {})).catch(() => {});
  }, []);

  const samples = ring?.samples || [];
  const count = range === "10" ? 60 : 360;
  const win = samples.slice(-count);
  const vals = win.map(([pi, po]) => pi - po);

  const preset = presets?.find((p) => p.id === status?.meter?.preset);

  return html`
    <div class="range">
      <button class="${range === "10" ? "active" : ""}" onClick=${() => setRange("10")}>${S.range10}</button>
      <button class="${range === "60" ? "active" : ""}" onClick=${() => setRange("60")}>${S.range60}</button>
    </div>
    ${err && html`<div class="err">${err}</div>`}
    ${vals.length >= 2 && html`
      <div class="card">
        <${HistChart} vals=${vals} />
      </div>
      <div class="stats3">
        <div class="card"><div class="s">${S.statMax}</div><div class="t">${(Math.max(...vals) / 1000).toFixed(2)} kW</div></div>
        <div class="card"><div class="s">${S.statAvg}</div><div class="t">${(vals.reduce((a, b) => a + b, 0) / vals.length / 1000).toFixed(2)} kW</div></div>
        <div class="card"><div class="s">${S.statMin}</div><div class="t">${(Math.min(...vals) / 1000).toFixed(2)} kW</div></div>
      </div>`}
    ${preset && values && html`
      <div class="card">
        <div class="s">${S.registers}</div>
        <div class="kv mono" style="margin-top:8px">
          ${preset.obis.filter((o) => values[o.name] != null).map((o) => html`
            <b>${o.obis}</b><span>${values[o.name]}${o.unit ? " " + o.unit : ""}</span>`)}
        </div>
      </div>`}`;
}

function HistChart({ vals }) {
  const w = 320, h = 130;
  const maxAbs = Math.max(1, ...vals.map(Math.abs));
  const hasNeg = vals.some((v) => v < 0);
  const zeroY = hasNeg ? h * 0.6 : h - 2;
  const bw = w / vals.length;
  return html`
    <svg viewBox="0 0 ${w} ${h}" class="hist-chart">
      <line x1="0" y1=${zeroY} x2=${w} y2=${zeroY} class="hist-zero" />
      ${vals.map((v, i) => {
        const bh = Math.max(1, (Math.abs(v) / maxAbs) * (v >= 0 ? zeroY - 4 : h - zeroY - 4));
        const y = v >= 0 ? zeroY - bh : zeroY;
        return html`<rect x=${(i * bw + 0.5).toFixed(1)} y=${y.toFixed(1)} width=${Math.max(1, bw - 1).toFixed(1)}
          height=${bh.toFixed(1)} rx="1" class=${v >= 0 ? "hist-bar-up" : "hist-bar-down"} />`;
      })}
    </svg>`;
}
