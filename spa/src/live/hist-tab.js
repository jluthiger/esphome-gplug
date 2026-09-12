import { useEffect, useState } from "preact/hooks";
import { html } from "../h.js";
import { S } from "../strings.js";
import { api } from "../api.js";
import { de, bucketLabel, qhDate, QH_EPOCH } from "../fmt.js";

// "60 Min" is the in-RAM ring the Live tab already polls -- free, and the only range with 10 s
// resolution. Everything longer comes from the flash history (/api/history), fetched once per
// range and kept, so re-visiting a range costs the device nothing. That scan reads flash, which is
// why it is not on the 10 s poll.
const RANGES = [["60", "range60"], ["day", "rangeDay"], ["week", "rangeWeek"], ["month", "rangeMonth"], ["year", "rangeYear"]];
const HF_CONFIG_CHANGE = 8, HF_NO_DATA = 16;
const MAX_BARS = 400;
const cache = new Map();

export function HistTab({ live, ring, status, presets }) {
  const [range, setRange] = useState("60");
  const [hist, setHist] = useState(null);
  const [loading, setLoading] = useState(false);
  const [err, setErr] = useState(null);

  useEffect(() => {
    if (range === "60") { setErr(null); return; }
    if (cache.has(range)) { setHist(cache.get(range)); setErr(null); return; }
    let stop = false;
    setLoading(true); setHist(null); setErr(null);
    api.history(range)
      .then((h) => { if (!stop) { cache.set(range, h); setHist(h); } })
      .catch((e) => { if (!stop) setErr(String(e.message || e)); })
      .finally(() => { if (!stop) setLoading(false); });
    return () => { stop = true; };
  }, [range]);

  const isRing = range === "60";
  const energy = range === "week" || range === "month" || range === "year";
  const bars = isRing ? ringBars(ring) : histBars(hist, range);
  const vals = bars.filter((b) => !b.missing).map((b) => b.v);
  const estimated = !isRing && hist && hist.pts.some((p) => p[0] === null);

  const preset = presets?.find((p) => p.id === status?.meter?.preset);
  const values = live?.values || {};
  const regs = preset ? preset.obis.filter((o) => values[o.name] != null) : [];

  return html`
    <div class="seg">
      ${RANGES.map(([id, key]) => html`
        <button class=${range === id ? "active" : ""} onClick=${() => setRange(id)}>${S[key]}</button>`)}
    </div>
    ${err && html`<div class="err">${err}</div>`}
    <div class="card">
      <div class="between" style="margin-bottom:12px">
        <span class="lbl">${energy ? S.energyPerBucket : S.netPower}</span>
        <span class="num" style="font-size:.66rem;color:var(--muted2)">
          ${isRing ? S.samples(vals.length) : S.points(vals.length)}
        </span>
      </div>
      ${loading && html`<p class="hint"><span class="spin"></span> ${S.loading}</p>`}
      ${!loading && bars.length < 2 && html`<p class="hint">${isRing ? S.waitingData : S.noHistory}</p>`}
      ${bars.length >= 2 && html`<${Bars} bars=${bars} />`}
      ${bars.length >= 2 && html`
        <div class="axis">
          ${axisLabels(bars).map((l) => html`<span>${l}</span>`)}
        </div>`}
    </div>
    ${estimated && html`<p class="hint">${S.timeEstimated}</p>`}
    ${vals.length >= 2 && (energy ? html`<${EnergyStats} bars=${bars} />` : html`<${PowerStats} vals=${vals} />`)}
    <${ExportCard} status=${status} />
    ${regs.length > 0 && html`
      <div class="card">
        <div class="lbl" style="margin-bottom:4px">${S.registers}</div>
        ${regs.map((o) => html`
          <div class="reg"><span class="o">${o.obis.replace(/^\d-\d:/, "")}</span><span class="n">${o.name}</span>
            <span class="v">${de(values[o.name], o.unit === "kWh" || o.unit === "kVArh" ? 3 : o.unit === "W" ? 0 : 2)}</span><span class="u">${o.unit || ""}</span></div>`)}
      </div>`}`;
}

// Ring -> bars: same 40-bucket mean over net power the tab has always drawn, in W.
function ringBars(ring) {
  const samples = ring?.samples || [];
  const vals = samples.map(([pi, po]) => pi - po);
  if (vals.length < 2) return [];
  const per = Math.ceil(vals.length / 40);
  const out = [];
  for (let i = 0; i < vals.length; i += per) {
    const c = vals.slice(i, i + per);
    out.push({ v: c.reduce((a, b) => a + b, 0) / c.length, label: null });
  }
  return out;
}

// History -> bars. Day keeps power (W, signed); longer ranges switch to energy per bucket (Wh,
// import positive / export negative) because a mean over a whole day says very little.
// A hole in the qh sequence means the device was off: render it as a gap rather than closing it.
function histBars(hist, range) {
  if (!hist || !hist.pts?.length) return [];
  const energy = range !== "day";
  const step = hist.bucket || 1;
  const out = [];
  let prevQh = null;
  for (const [qh, dEi, dEo, pMin, pMax, pAvg, flags] of hist.pts) {
    if (qh !== null && prevQh !== null && out.length < MAX_BARS) {
      for (let g = prevQh + step; g < qh && out.length < MAX_BARS; g += step)
        out.push({ v: 0, missing: true, label: bucketLabel(range, g) });
    }
    const label = qh === null ? null : bucketLabel(range, qh);
    if (energy) {
      const known = dEi !== null || dEo !== null;
      out.push({ v: (dEi || 0) - (dEo || 0), label, missing: !known || (flags & HF_NO_DATA) !== 0 });
    } else {
      out.push({ v: pAvg, label, missing: (flags & HF_NO_DATA) !== 0 });
    }
    if (qh !== null) prevQh = qh;
  }
  return out;
}

// Lastgang download: the same 15-min records the charts are built from, but every one of them at
// native resolution, as a CSV the user takes to their grid operator or ZEV/LEG settlement. A date
// range picks quarter-hour indices (local midnight to local midnight, end exclusive); the "all"
// link drops the window, which is the only way to get records that never got a timestamp.
const isoDate = (d) => `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, "0")}-${String(d.getDate()).padStart(2, "0")}`;
const qhOfDate = (iso) => {
  const [y, m, d] = iso.split("-").map(Number);
  return Math.floor((new Date(y, m - 1, d).getTime() / 1000 - QH_EPOCH) / 900);
};
const dayAfter = (iso) => { const [y, m, d] = iso.split("-").map(Number); return isoDate(new Date(y, m - 1, d + 1)); };

function ExportCard({ status }) {
  const h = status?.history;
  const count = h?.count || 0;
  const timed = h?.oldest_qh > 0 && h?.newest_qh > 0;
  const oldest = timed ? isoDate(qhDate(h.oldest_qh)) : null;
  const newest = timed ? isoDate(qhDate(h.newest_qh)) : null;
  const [from, setFrom] = useState(null);
  const [to, setTo] = useState(null);
  const [fmt, setFmt] = useState("full");
  const FORMATS = [["full", S.csvFmtFull], ["ckw", S.csvFmtCkw], ["ckw-einspeisung", S.csvFmtCkwOut]];
  const f = from ?? oldest ?? isoDate(new Date());
  const t = to ?? newest ?? isoDate(new Date());
  const ordered = f <= t;
  const download = () => { window.location.assign(api.historyCsvUrl(qhOfDate(f), qhOfDate(dayAfter(t)), fmt)); };

  return html`
    <div class="card">
      <div class="t">${S.csvTitle}</div>
      <p class="hint">${S.csvHint}</p>
      <p class="hint">${count ? (timed ? S.csvStored(count, oldest, newest) : S.csvStoredNoTime(count)) : S.noHistory}</p>
      <div class="row" style="margin:12px 0">
        <label><div class="lbl" style="margin-bottom:4px">${S.csvFrom}</div>
          <input type="date" value=${f} max=${t} onInput=${(e) => setFrom(e.target.value)} /></label>
        <label><div class="lbl" style="margin-bottom:4px">${S.csvTo}</div>
          <input type="date" value=${t} min=${f} onInput=${(e) => setTo(e.target.value)} /></label>
      </div>
      <div class="lbl" style="margin-bottom:4px">${S.csvFormat}</div>
      <div class="seg" style="margin:0 0 6px">
        ${FORMATS.map(([id, label]) => html`
          <button class=${fmt === id ? "active" : ""} onClick=${() => setFmt(id)}>${label}</button>`)}
      </div>
      <p class="hint" style="margin:0 0 12px">${fmt === "full" ? S.csvFmtFullHint : S.csvFmtCkwHint}</p>
      ${!ordered && html`<div class="err">${S.csvOrder}</div>`}
      <button class="primary" style="width:100%" disabled=${!count || !ordered || !timed} onClick=${download}>${S.csvDownload}</button>
      ${count > 0 && html`<p class="hint" style="margin-top:10px;text-align:center">
        <a href=${api.historyCsvUrl(null, null, fmt)}>${S.csvAll}</a></p>`}
    </div>`;
}

function axisLabels(bars) {
  const labelled = bars.filter((b) => b.label);
  if (!labelled.length) return [S.ago60, S.now];
  const pick = [0, Math.floor(labelled.length / 2), labelled.length - 1];
  return [...new Set(pick)].map((i) => labelled[i].label);
}

function PowerStats({ vals }) {
  return html`
    <div class="grid3">
      <${Stat} k=${S.statMax} v=${de(Math.max(...vals) / 1000, 2)} u="kW" />
      <${Stat} k=${S.statAvg} v=${de(vals.reduce((a, b) => a + b, 0) / vals.length / 1000, 2)} u="kW" />
      <${Stat} k=${S.statMin} v=${de(Math.min(...vals) / 1000, 2)} u="kW" />
    </div>`;
}

function EnergyStats({ bars }) {
  const known = bars.filter((b) => !b.missing);
  const imp = known.reduce((a, b) => a + Math.max(0, b.v), 0) / 1000;
  const exp = known.reduce((a, b) => a + Math.max(0, -b.v), 0) / 1000;
  return html`
    <div class="grid3">
      <${Stat} k=${S.statImport} v=${de(imp, 1)} u="kWh" />
      <${Stat} k=${S.statExport} v=${de(exp, 1)} u="kWh" orange=${true} />
      <${Stat} k=${S.statSum} v=${de(imp - exp, 1)} u="kWh" />
    </div>`;
}

function Stat({ k, v, u, orange }) {
  return html`<div class="card stat"><div class="lbl">${k}</div>
    <div class="v ${orange ? "orange" : ""}" style="font-size:1.1rem">${v}</div><div class="u">${u}</div></div>`;
}

function Bars({ bars }) {
  const w = 320, h = 130;
  const maxAbs = Math.max(1, ...bars.filter((b) => !b.missing).map((b) => Math.abs(b.v)));
  const hasNeg = bars.some((b) => !b.missing && b.v < 0);
  const zero = hasNeg ? h * 0.62 : h - 2;
  const bw = w / bars.length;
  return html`
    <svg viewBox="0 0 ${w} ${h}" class="hist">
      <line x1="0" y1=${zero} x2=${w} y2=${zero} class="zero" />
      ${bars.map((b, i) => {
        const x = (i * bw + (bw > 3 ? 1 : 0.2)).toFixed(1);
        const bwd = Math.max(0.8, bw - (bw > 3 ? 2 : 0.4)).toFixed(1);
        if (b.missing) return html`<rect x=${x} y=${(zero - 3).toFixed(1)} width=${bwd} height="3" rx="1" class="gap" />`;
        const bh = Math.max(2, (Math.abs(b.v) / maxAbs) * (b.v >= 0 ? zero - 4 : h - zero - 4));
        return html`<rect x=${x} y=${(b.v >= 0 ? zero - bh : zero).toFixed(1)} width=${bwd}
          height=${bh.toFixed(1)} rx="1.5" class=${b.v >= 0 ? "up" : "down"} />`;
      })}
    </svg>`;
}
