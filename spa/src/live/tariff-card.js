import { useEffect, useRef, useState } from "preact/hooks";
import { html } from "../h.js";
import { S } from "../strings.js";
import { api } from "../api.js";
import { num, clock, dateShort, qhDate, QH_EPOCH } from "../fmt.js";
import { Collapsible } from "./collapsible.js";
import { useBox } from "../box.js";
import { useScrub, Readout } from "../scrub.js";

// Dynamic tariffs (issue 51). The device fetches the price per quarter hour from the chosen utility
// (the APIs send no CORS headers, so the browser cannot) and serves it at /api/tariff. Two
// providers, different tariff components, so the UI always names which one it shows: BKW's feed-in
// remuneration (what exported energy earns), CKW's dynamic grid usage tariff (what imported energy
// costs for the grid, energy itself not included), and Swiss Tariff Hub's total price (energy +
// grid + levies) of whichever tariff the user's hub account is for.
const PROVIDERS = [["off", "tariffOff"], ["bkw", "tariffBkw"], ["ckw", "tariffCkw"], ["sth", "tariffSth"]];
const KIND = { bkw: "tariffKindFeedIn", ckw: "tariffKindGrid", sth: "tariffKindTotal" };
const ERR = { net: "tariffErrNet", http: "tariffErrHttp", format: "tariffErrFormat", empty: "tariffErrEmpty",
  memory: "tariffErrMemory", auth: "tariffErrAuth" };

// Device tab: choose the provider. BKW, CKW and off are saved at once, like language and appearance;
// the hub needs its tariff and key first, so its button opens a form. The closed card's summary
// comes from /api/status (`tariff`), so a closed card makes no request.
export function TariffSetup({ status, onSaved }) {
  const prov = status?.tariff;
  if (prov == null) return null;   // a firmware without dynamic tariffs
  const label = S[PROVIDERS.find(([id]) => id === prov)?.[1]] || prov;
  return html`
    <${Collapsible} id="tariff" title=${S.tariffTitle} summary=${label}>
      <${TariffSetupBody} prov=${prov} onSaved=${onSaved} />
    <//>`;
}

function TariffSetupBody({ prov, onSaved }) {
  const [t, setT] = useState(null);
  const [rev, setRev] = useState(0);   // bumped by a save, so the state is read again at once
  const [busy, setBusy] = useState(false);
  const [err, setErr] = useState(null);
  const [form, setForm] = useState(null);   // hub settings being edited: {tariff_id, key}
  // While open, follow the fetch the device runs after a change, so "fetched" shows up without a reload.
  useEffect(() => {
    let stop = false, timer;
    const poll = () => api.tariff().then((r) => { if (!stop) { setT(r); timer = setTimeout(poll, r.fetching || !r.fetched_ago && r.provider !== "off" ? 3000 : 60000); } })
      .catch(() => { if (!stop) timer = setTimeout(poll, 10000); });
    poll();
    return () => { stop = true; clearTimeout(timer); };
  }, [prov, rev]);

  async function save(body) {
    setBusy(true); setErr(null);
    try { await api.setTariff(body); onSaved(body.provider); setForm(null); setRev(rev + 1); }
    catch (e) { setErr(String(e.message || e)); } finally { setBusy(false); }
  }
  function pick(id) {
    if (busy) return;
    if (id === "sth") return setForm({ tariff_id: t?.tariff_id || "ckw_home_dynamic", key: "" });
    setForm(null);
    if (id !== prov) save({ provider: id });
  }
  const sel = form ? "sth" : prov;
  // Left empty, the key field keeps the stored key (the device never sends it back).
  const keySet = prov === "sth" && t?.key_set;

  return html`
    <p class="hint" style="margin-top:4px">${S.tariffHint}</p>
    <div class="seg" role="radiogroup">
      ${PROVIDERS.map(([id, key]) => html`
        <button role="radio" aria-checked=${sel === id} class=${sel === id ? "active" : ""} disabled=${busy}
          onClick=${() => pick(id)}>${S[key]}</button>`)}
    </div>
    ${sel !== "off" && html`<p class="hint" style="margin:0">${S[KIND[sel]]}</p>`}
    ${sel === "sth" && !form && html`<p style="margin:10px 0 0"><button disabled=${busy}
      onClick=${() => setForm({ tariff_id: t?.tariff_id || "", key: "" })}>${S.tariffSthEdit}</button></p>`}
    ${form && html`
      <div style="margin:12px 0 10px"><label for="sth-id">${S.tariffSthId}</label>
        <input id="sth-id" type="text" autocomplete="off" spellcheck="false" value=${form.tariff_id}
          onInput=${(e) => setForm({ ...form, tariff_id: e.target.value.trim() })} /></div>
      <div style="margin-bottom:12px"><label for="sth-key">${S.tariffSthKey}</label>
        <input id="sth-key" type="password" autocomplete="off" spellcheck="false" value=${form.key}
          placeholder=${keySet ? S.tariffSthKeySet : ""} onInput=${(e) => setForm({ ...form, key: e.target.value.trim() })} /></div>
      <button class="primary" style="width:100%" disabled=${busy || !form.tariff_id || (!form.key && !keySet)}
        onClick=${() => save({ provider: "sth", tariff_id: form.tariff_id, ...(form.key && { key: form.key }) })}>${S.tariffSthSave}</button>`}
    ${prov !== "off" && !form && t?.provider === prov && html`<p class="hint" style="margin:8px 0 0">${tariffState(t)}</p>`}
    ${err && html`<div class="err">${err}</div>`}`;
}

function tariffState(t) {
  if (t.fetching) return S.tariffFetching;
  if (t.error) return S[ERR[t.error]] || t.error;
  if (t.fetched_ago != null) return S.tariffFetched(Math.round(t.fetched_ago / 60));
  return S.tariffFetching;
}

// Local midnight of the day containing `d`, and the quarter-hour index of a Date.
const midnight = (d) => new Date(d.getFullYear(), d.getMonth(), d.getDate());
const qhOf = (d) => Math.floor((d.getTime() / 1000 - QH_EPOCH) / 900);
const histCache = new Map();

// History tab: price line over the measured energy of one local day, negative prices marked, and
// what the day's energy earned (BKW) or cost in grid usage (CKW). `day` is the live screen's day
// history: when it gains a record, today's prices and energy are read again.
export function TariffChart({ status, day }) {
  const prov = status?.tariff;
  if (!prov || prov === "off") return null;
  const feed = prov === "bkw";
  const title = S[{ bkw: "tariffChartFeedIn", ckw: "tariffChartGrid", sth: "tariffChartTotal" }[prov]];
  return html`
    <${Collapsible} id="tariffchart" open title=${title} summary=${S[PROVIDERS.find(([id]) => id === prov)?.[1]]}>
      <${TariffDay} feed=${feed} total=${prov === "sth"} title=${title} newest=${day?.newest_qh} />
    <//>`;
}

function TariffDay({ feed, total, title, newest }) {
  const [t, setT] = useState(null);
  const [err, setErr] = useState(null);
  const [off, setOff] = useState(0);   // days from today
  const [hist, setHist] = useState(null);

  useEffect(() => {
    let stop = false;
    api.tariff().then((r) => { if (!stop) { setT(r); setErr(null); } }).catch((e) => { if (!stop) setErr(String(e.message || e)); });
    return () => { stop = true; };
  }, [newest]);

  const now = new Date();
  const d0 = midnight(new Date(now.getFullYear(), now.getMonth(), now.getDate() + off));
  const d1 = midnight(new Date(d0.getFullYear(), d0.getMonth(), d0.getDate() + 1));
  const q0 = qhOf(d0), q1 = qhOf(d1);
  const future = d0 > now;

  useEffect(() => {
    if (future) { setHist(null); return; }
    let stop = false;
    // Past days do not change; today's is read again when the store gains a record.
    const key = `${q0}:${off === 0 ? newest : ""}`;
    if (histCache.has(key)) { setHist(histCache.get(key)); return; }
    setHist(null);
    api.historyWindow(q0, q1).then((h) => { if (!stop) { histCache.set(key, h); setHist(h); } }).catch(() => { /* chart without energy */ });
    return () => { stop = true; };
  }, [q0, newest]);

  const prices = t?.prices || [];
  const first = t?.first_qh ?? 0;
  const lastQh = first + prices.length;
  const scale = t?.scale || 1e6;
  const energy = new Map();
  for (const [qh, dEi, dEo] of hist?.pts || []) {
    const v = feed ? dEo : dEi;
    if (qh != null && v != null) energy.set(qh, v);
  }
  const slots = [];
  for (let q = q0; q < q1; q++) {
    const p = q >= first && q < lastQh ? prices[q - first] : null;
    slots.push({ qh: q, p: p == null ? null : p / scale, wh: energy.has(q) ? energy.get(q) : null });
  }
  const priced = slots.filter((s) => s.p != null);
  const both = priced.filter((s) => s.wh != null);
  const chf = both.reduce((a, s) => a + (s.wh / 1000) * s.p, 0);
  const kwh = both.reduce((a, s) => a + s.wh / 1000, 0);
  const neg = priced.filter((s) => s.p < 0).length;
  // Quarter hours that are over: only those can have energy, so only they count as "missing" it.
  const nowQh = qhOf(now);
  const pastPriced = priced.filter((s) => s.qh < nowQh).length;
  // Days the device holds prices for, to bound the day picker.
  const minOff = t?.first_qh != null ? Math.round((midnight(qhDate(first)) - midnight(now)) / 864e5) : 0;
  const maxOff = t?.first_qh != null ? Math.round((midnight(qhDate(lastQh - 1)) - midnight(now)) / 864e5) : 0;

  let note = null;
  if (err) note = html`<div class="err">${err}</div>`;
  else if (!t) note = html`<p class="hint"><span class="spin"></span> ${S.loading}</p>`;
  else if (!priced.length) note = html`<p class="hint">${t.fetching ? S.tariffFetching : t.error ? S[ERR[t.error]] || t.error : S.tariffNoPrices}</p>`;

  return html`
    <div class="between tday">
      <button class="tnav" aria-label=${S.tariffPrev} disabled=${off <= Math.min(0, minOff)} onClick=${() => setOff(off - 1)}>‹</button>
      <span class="lbl">${off === 0 ? S.today : off === 1 ? S.tomorrow : dateShort(d0)}</span>
      <button class="tnav" aria-label=${S.tariffNext} disabled=${off >= Math.max(0, maxOff)} onClick=${() => setOff(off + 1)}>›</button>
    </div>
    ${note}
    ${priced.length > 0 && html`
      <${PriceChart} slots=${slots} feed=${feed} title=${title} />
      <div class="axis">${[0, 32, 64, 95].map((i) => html`<span>${clock(qhDate(slots[Math.min(i, slots.length - 1)].qh))}</span>`)}</div>
      <div class="hsum">
        <div class="stat"><div class="lbl">${feed ? S.statExport : S.statImport}</div>
          <div class="v ${feed ? "exp" : "imp"}">${both.length ? num(kwh, 2) : "–"} <span class="u">kWh</span></div></div>
        <div class="stat"><div class="lbl">${S.tariffAvg}</div>
          <div class="v">${kwh > 0 ? num((chf / kwh) * 100, 2) : num(priced.reduce((a, s) => a + s.p, 0) / priced.length * 100, 2)} <span class="u">Rp/kWh</span></div></div>
        <div class="stat"><div class="lbl">${feed ? S.tariffEarned : total ? S.tariffCostTotal : S.tariffCost}</div>
          <div class="v">${both.length ? num(chf, 2) : "–"} <span class="u">CHF</span></div></div>
      </div>
      ${neg > 0 && html`<p class="hint" style="margin:8px 0 0"><span class="negkey"></span> ${S.tariffNeg(neg)}</p>`}
      ${both.length < pastPriced && html`<p class="hint" style="margin:8px 0 0">${S.tariffPartial(both.length, pastPriced)}</p>`}`}`;
}

// Bars: the energy of each quarter hour (export for feed-in, import for grid usage), from the
// bottom. Line: the price, on its own scale over the full height, with a zero line once a price
// goes negative. A quarter hour with a negative price gets a shaded column, so it stands out even
// where no energy flowed.
function PriceChart({ slots, feed, title }) {
  const svg = useRef(null);
  const [w, h] = useBox(svg, 320, 130);
  const n = slots.length, bw = w / n;
  const ps = slots.filter((s) => s.p != null).map((s) => s.p);
  const pMin = Math.min(0, ...ps), pMax = Math.max(...ps, pMin + 0.01);
  const py = (p) => 4 + (h - 8) * (1 - (p - pMin) / (pMax - pMin));
  const eMax = Math.max(1, ...slots.map((s) => s.wh || 0));
  const { i, props } = useScrub(n, true);
  const sel = i === null ? null : slots[i];
  let text = null;
  if (sel) {
    const parts = [clock(qhDate(sel.qh)), sel.p == null ? S.noData : `${num(sel.p * 100, 2)} Rp/kWh`];
    if (sel.wh != null) parts.push(`${num(sel.wh / 1000, 3)} kWh`);
    if (sel.wh != null && sel.p != null) parts.push(`${num((sel.wh / 1000) * sel.p, 3)} CHF`);
    text = parts.join(" · ");
  }
  let path = "", pen = false;
  slots.forEach((s, k) => {
    if (s.p == null) { pen = false; return; }
    const x = (k * bw).toFixed(1), y = py(s.p).toFixed(1);
    path += `${pen ? "L" : "M"}${x},${y}H${((k + 1) * bw).toFixed(1)}`;   // a step: one price per quarter hour
    pen = true;
  });
  const gx = i === null ? 0 : ((i + 0.5) * bw).toFixed(1);
  return html`
    <${Readout} text=${text} />
    <svg ref=${svg} viewBox="0 0 ${w} ${h}" preserveAspectRatio="none" class="hist tariff${i === null ? "" : " on"}" role="img"
      aria-label=${title} ...${props}>
      ${slots.map((s, k) => s.p != null && s.p < 0 && html`<rect x=${(k * bw).toFixed(1)} y="0" width=${bw.toFixed(1)} height=${h} class="neg" />`)}
      ${slots.map((s, k) => s.wh > 0 && html`<rect x=${(k * bw + (bw > 3 ? 1 : 0.2)).toFixed(1)} y=${(h - (s.wh / eMax) * (h * 0.6)).toFixed(1)}
        width=${Math.max(0.8, bw - (bw > 3 ? 2 : 0.4)).toFixed(1)} height=${((s.wh / eMax) * (h * 0.6)).toFixed(1)} rx="1"
        class=${(feed ? "down" : "up") + (i === k ? " sel" : "")} />`)}
      ${pMin < 0 && html`<line x1="0" y1=${py(0)} x2=${w} y2=${py(0)} class="zero" />`}
      <path d=${path} class="price" />
      ${i !== null && html`<line x1=${gx} y1="0" x2=${gx} y2=${h} class="guide" />`}
    </svg>`;
}
