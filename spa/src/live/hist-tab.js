import { useEffect, useRef, useState } from "preact/hooks";
import { html } from "../h.js";
import { S } from "../strings.js";
import { api } from "../api.js";
import { num, bucketLabel, bucketWhen, dateShort, dateRange, qhDate, QH_EPOCH } from "../fmt.js";
import { Collapsible } from "./collapsible.js";
import { useBox } from "../box.js";
import { useScrub, Readout } from "../scrub.js";
// Every range here comes from the flash history (/api/history), fetched per range and kept until
// the store gains a record, so re-visiting one costs the device nothing; the scan reads flash,
// which is why it is not on the 10 s poll. There is deliberately no "last hour" range: that is live data, not stored history,
// and the Live tab draws exactly the same series one tap away.
const RANGES = [["day", "rangeDay"], ["week", "rangeWeek"], ["month", "rangeMonth"], ["year", "rangeYear"]];
const HF_CONFIG_CHANGE = 8, HF_NO_DATA = 16;
const MAX_BARS = 400;
const cache = new Map();

export function HistTab({ live, day, status, presets, wide }) {
  const [range, setRange] = useState("day");
  const [hist, setHist] = useState(null);
  const [loading, setLoading] = useState(false);
  const [err, setErr] = useState(null);

  useEffect(() => {
    // The live screen already holds the day range (it needs it to fill the last hour), so taking
    // it from there saves the device a second flash scan for data the app has in hand.
    if (range === "day" && day) { cache.set("day", day); setHist(day); setErr(null); return; }
    // A cached range is good until the store has a newer record than it does. The day history the
    // live screen re-reads every 5 min tells when that happened; without that check a tab left open
    // kept showing the week as it was at first view, hours behind the day range.
    const kept = cache.get(range);
    if (kept && (!day || kept.newest_qh === day.newest_qh)) { setHist(kept); setErr(null); return; }
    let stop = false;
    setErr(null);
    // A stale range stays on screen while it is re-read, rather than blanking the chart every 15 min.
    if (kept) setHist(kept); else { setLoading(true); setHist(null); }
    api.history(range)
      .then((h) => { if (!stop) { cache.set(range, h); setHist(h); } })
      .catch((e) => { if (!stop) setErr(String(e.message || e)); })
      .finally(() => { if (!stop) setLoading(false); });
    return () => { stop = true; };
  }, [range, day]);

  const bars = histBars(hist, range);
  const vals = bars.filter((b) => !b.missing).map((b) => b.v);
  const estimated = hist && hist.pts.some((p) => p[0] === null);

  const preset = presets?.find((p) => p.id === status?.meter?.preset);
  const values = live?.values || {};
  const regs = preset ? preset.obis.filter((o) => values[o.name] != null) : [];

  const chartPart = html`
    <div class="seg">
      ${RANGES.map(([id, key]) => html`
        <button class=${range === id ? "active" : ""} onClick=${() => setRange(id)}>${S[key]}</button>`)}
    </div>
    ${err && html`<div class="err">${err}</div>`}
    <div class="card">
      <div class="between" style="margin-bottom:2px">
        <span class="lbl">${S.energyPerBucket}</span>
        <span class="num" style="font-size:.66rem;color:var(--muted2)">
          ${S.points(vals.length)}
        </span>
      </div>
      ${loading && html`<p class="hint"><span class="spin"></span> ${S.loading}</p>`}
      ${!loading && bars.length < 2 && html`<p class="hint">${S.noHistory}</p>`}
      ${bars.length >= 2 && html`<${Bars} bars=${bars} range=${range} />`}
      ${bars.length >= 2 && html`
        <div class="axis">
          ${axisLabels(bars).map((l) => html`<span>${l}</span>`)}
        </div>`}
      ${vals.length >= 2 && html`<${EnergyStats} bars=${bars} />`}
      ${estimated && html`<p class="hint" style="margin:8px 0 0">${S.timeEstimated}</p>`}
    </div>`;
  // Closed by default like the export card: the list is one row per OBIS value, several phone
  // screens on a three-phase meter, and the Live tab already shows the figures most people want.
  const registers = regs.length > 0 && html`
    <${Collapsible} id="regs" title=${S.registers} summary=${S.points(regs.length)}>
      <div style="margin-top:4px">
      ${regs.map((o) => html`
        <div class="reg"><span class="o">${o.obis.replace(/^\d-\d:/, "")}</span><span class="n">${o.name}</span>
          <span class="v">${num(values[o.name], o.unit === "kWh" || o.unit === "kVArh" ? 3 : o.unit === "W" ? 0 : 2)}</span><span class="u">${o.unit || ""}</span></div>`)}
      </div>
    <//>`;

  // Wide screens: the chart and its figures take the width, the export and the register list sit
  // in a column beside them instead of below the fold.
  if (wide) return html`
    <div class="histgrid">
      <div class="h-main">${chartPart}</div>
      <aside class="h-side"><${ExportCard} status=${status} />${registers}</aside>
    </div>`;

  return html`${chartPart}<${ExportCard} status=${status} />${registers}`;
}

// History -> bars: energy per bucket (Wh, import positive / export negative) for every range. Day
// used to plot the mean power instead, which made it the one range whose bars did not add up to its
// totals; a quarter-hour's energy is that mean times 0.25 h anyway, so the shape is unchanged.
// A hole in the qh sequence means the device was off: render it as a gap rather than closing it.
function histBars(hist, range) {
  if (!hist || !hist.pts?.length) return [];
  const step = hist.bucket || 1;
  const out = [];
  let prevQh = null;
  for (const [qh, dEi, dEo, , , , flags] of hist.pts) {
    if (qh !== null && prevQh !== null && out.length < MAX_BARS) {
      for (let g = prevQh + step; g < qh && out.length < MAX_BARS; g += step)
        out.push({ v: 0, missing: true, label: bucketLabel(range, g), qh: g });
    }
    const label = qh === null ? null : bucketLabel(range, qh);
    const known = dEi !== null || dEo !== null;
    out.push({ v: (dEi || 0) - (dEo || 0), label, qh, missing: !known || (flags & HF_NO_DATA) !== 0 });
    if (qh !== null) prevQh = qh;
  }
  return out;
}

// Lastgang download: the same 15-min records the charts are built from, but every one of them at
// native resolution, as a CSV the user takes to their grid operator or ZEV/LEG settlement. A date
// range picks quarter-hour indices (local midnight to local midnight, end exclusive); the "all"
// link drops the window, which is the only way to get records that never got a timestamp.
//
// Most visits only look at the charts, so the card starts closed. The chosen dates live here, above
// the Collapsible, so they survive closing it (a closed Collapsible unmounts its body).
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
  const f = from ?? oldest ?? isoDate(new Date());
  const t = to ?? newest ?? isoDate(new Date());
  const ordered = f <= t;
  const download = () => { window.location.assign(api.historyCsvUrl(qhOfDate(f), qhOfDate(dayAfter(t)))); };
  const summary = !count ? S.noData
    : timed ? dateRange(qhDate(h.oldest_qh), qhDate(h.newest_qh)) : S.csvSumNoTime(count);

  return html`
    <${Collapsible} id="csv" title=${S.csvTitle} summary=${summary}>
      <p class="hint">${S.csvHint}</p>
      <p class="hint">${count
        ? (timed ? S.csvStored(count, dateShort(qhDate(h.oldest_qh)), dateShort(qhDate(h.newest_qh))) : S.csvStoredNoTime(count))
        : S.noHistory}</p>
      <div class="row dates" style="margin:12px 0">
        <label><div class="lbl" style="margin-bottom:4px">${S.csvFrom}</div>
          <input type="date" value=${f} max=${t} onInput=${(e) => setFrom(e.target.value)} /></label>
        <label><div class="lbl" style="margin-bottom:4px">${S.csvTo}</div>
          <input type="date" value=${t} min=${f} onInput=${(e) => setTo(e.target.value)} /></label>
      </div>
      <p class="hint" style="margin:0 0 12px">${S.csvFmtFullHint}</p>
      ${!ordered && html`<div class="err">${S.csvOrder}</div>`}
      <button class="primary" style="width:100%" disabled=${!count || !ordered || !timed} onClick=${download}>${S.csvDownload}</button>
      ${count > 0 && html`<p class="hint" style="margin-top:10px;text-align:center">
        <a href=${api.historyCsvUrl()}>${S.csvAll}</a></p>`}
    <//>`;
}

function axisLabels(bars) {
  const labelled = bars.filter((b) => b.label);
  if (!labelled.length) return [S.ago60, S.now];
  const pick = [0, Math.floor(labelled.length / 2), labelled.length - 1];
  return [...new Set(pick)].map((i) => labelled[i].label);
}

function EnergyStats({ bars }) {
  const known = bars.filter((b) => !b.missing);
  const imp = known.reduce((a, b) => a + Math.max(0, b.v), 0) / 1000;
  const exp = known.reduce((a, b) => a + Math.max(0, -b.v), 0) / 1000;
  return html`
    <div class="hsum">
      <${Stat} k=${S.statImport} v=${num(imp, 1)} dir="imp" />
      <${Stat} k=${S.statExport} v=${num(exp, 1)} dir="exp" />
      <${Stat} k=${S.statSum} v=${num(imp - exp, 1)} />
    </div>`;
}

// The totals sit in the chart card as one line under the axis rather than as three tiles of their
// own: the tiles cost a card's padding and margins for three numbers, about 90 px on a phone.
function Stat({ k, v, dir }) {
  return html`<div class="stat"><div class="lbl">${k}</div>
    <div class="v ${dir || ""}">${v} <span class="u ${dir || ""}">kWh</span></div></div>`;
}

function Bars({ bars, range }) {
  // Bar width and gap are in pixels, so a wider column means more bars' worth of room rather than
  // wider bars (see box.js).
  const svg = useRef(null);
  const [w, h] = useBox(svg, 320, 130);
  const maxAbs = Math.max(1, ...bars.filter((b) => !b.missing).map((b) => Math.abs(b.v)));
  const hasNeg = bars.some((b) => !b.missing && b.v < 0);
  const zero = hasNeg ? h * 0.62 : h - 2;
  const bw = w / bars.length;

  // A record written before the clock synced has no time (qh null), so its reading is the value
  // alone rather than a made-up date. A quarter hour typically holds a few hundred Wh, which one
  // decimal of kWh would round to nothing, so Day reads to 10 Wh.
  const dec = range === "day" ? 2 : 1;
  const { i, props } = useScrub(bars.length, true);
  const sel = i === null ? null : bars[i];
  let text = null;
  if (sel) {
    const v = sel.missing ? S.noData
      : `${num(Math.abs(sel.v) / 1000, dec)} kWh ${sel.v >= 0 ? S.statImport : S.statExport}`;
    text = sel.qh == null ? v : `${bucketWhen(range, sel.qh)} · ${v}`;
  }
  const gx = i === null ? 0 : ((i + 0.5) * bw).toFixed(1);
  return html`
    <${Readout} text=${text} />
    <svg ref=${svg} viewBox="0 0 ${w} ${h}" preserveAspectRatio="none" class="hist${i === null ? "" : " on"}" role="img"
      aria-label=${S.energyPerBucket} ...${props}>
      <line x1="0" y1=${zero} x2=${w} y2=${zero} class="zero" />
      ${bars.map((b, idx) => {
        const x = (idx * bw + (bw > 3 ? 1 : 0.2)).toFixed(1);
        const bwd = Math.max(0.8, bw - (bw > 3 ? 2 : 0.4)).toFixed(1);
        if (b.missing) return html`<rect x=${x} y=${(zero - 3).toFixed(1)} width=${bwd} height="3" rx="1" class="gap" />`;
        const on = i === idx ? " sel" : "";
        const bh = Math.max(2, (Math.abs(b.v) / maxAbs) * (b.v >= 0 ? zero - 4 : h - zero - 4));
        return html`<rect x=${x} y=${(b.v >= 0 ? zero - bh : zero).toFixed(1)} width=${bwd}
          height=${bh.toFixed(1)} rx="1.5" class=${(b.v >= 0 ? "up" : "down") + on} />`;
      })}
      ${i !== null && html`<line x1=${gx} y1="0" x2=${gx} y2=${h} class="guide" />`}
    </svg>`;
}
