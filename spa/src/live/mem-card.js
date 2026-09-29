import { useEffect, useRef, useState } from "preact/hooks";
import { html } from "../h.js";
import { S } from "../strings.js";
import { api } from "../api.js";
import { clockAt, dur, num } from "../fmt.js";
import { useBox } from "../box.js";
import { useScrub, Readout } from "../scrub.js";
import { Collapsible } from "./collapsible.js";

// Heap figures and the device's 24 h heap trend (/api/heap, heap_monitor.h). The point is a slow
// leak: one free-heap number says nothing, a line that keeps falling over hours does. The device
// samples every 5 min, so polling faster would only redraw the same line.
const MEM_POLL_MS = 300000;

// kB are 1024 B and truncated, exactly as the device stores them in the ring and publishes them to
// Home Assistant, so the same byte count reads the same everywhere.
const toKb = (b) => (b == null ? null : Math.floor(b / 1024));
const kbText = (k) => (k == null ? null : `${num(k)} kB`);
const kb = (b) => kbText(toKb(b));

// Wall-clock second of ring sample i: its distance in uptime from the device's uptime at the fetch.
// Uptime wraps after 49.7 days; a sample from before the wrap falls back to counting samples back
// from the newest, which is off by at most one period.
function sampleAt({ samples, period = 300, uptime, at }, i) {
  const s = samples[i];
  const ago = uptime != null && s[0] <= uptime ? uptime - s[0] : (samples.length - 1 - i) * period;
  return at - ago;
}

// Closed, the card shows the free heap from the page's /api/status and fetches nothing; MemBody
// only exists (and polls) while the card is open.
export function MemCard({ status }) {
  const free = status?.mem?.free ?? status?.heap;
  return html`
    <${Collapsible} id="mem" title=${S.memTitle} summary=${free != null && S.memSummary(kb(free))}>
      <${MemBody} status=${status} />
    <//>`;
}

function MemBody({ status }) {
  const [st, setSt] = useState(status);
  const [trend, setTrend] = useState(null);

  useEffect(() => {
    let stop = false;
    // /api/status is otherwise loaded once per page, so the card refreshes its own copy alongside
    // the trend -- otherwise "free" would stay at its page-load value next to a moving line.
    const load = () => {
      api.heap().then((r) => { if (!stop) setTrend({ ...r, at: Date.now() / 1000 }); }).catch(() => {});
      api.status().then((r) => { if (!stop) setSt(r); }).catch(() => {});
    };
    load();
    const id = setInterval(load, MEM_POLL_MS);
    return () => { stop = true; clearInterval(id); };
  }, []);

  // The heap rows show the ring's newest sample, the one the chart ends in, and say when it was taken.
  // /api/status reads the heap at its own moment, so its figure could sit kB away from the chart's
  // last point and the card contradicted itself (issue #20). Status is the fallback until the first
  // sample exists, and the only source for the stacks.
  const m = st?.mem;
  const samples = trend?.samples || [];
  const last = samples.at(-1);
  const rows = [
    [S.memFree, last ? kbText(last[1]) : kb(m?.free ?? st?.heap)],
    [S.memMin, last ? kbText(last[2]) : kb(m?.min_free)],
    [S.memLargest, last ? kbText(last[3]) : kb(m?.largest)],
    [S.memSampled, last ? clockAt(sampleAt(trend, samples.length - 1)) : null],
    [S.memStack, m ? `${num(m.stack_loop)} / ${num(m.stack_httpd)} B` : null],
  ].filter(([, v]) => v);

  // From the sample count, not the uptimes: the device's uptime wraps after 49.7 days.
  const span = Math.max(0, samples.length - 1) * (trend?.period || 300);

  return html`
      <div class="kv">${rows.map(([k, v]) => html`<b>${k}</b><span>${v}</span>`)}</div>
      ${samples.length >= 2 ? html`
        <${Spark} trend=${trend} />
        <div class="axis wrap"><span>${S.memTrend(dur(span))}</span><span>— ${S.memFree} · - - ${S.memLargest} · ··· ${S.memMinShort}</span></div>
        <p class="hint" style="margin:8px 0 0">${S.memHint}</p>`
      : trend && html`<p class="hint" style="margin:12px 0 0">${S.memWaiting}</p>`}`;
}

// Free heap (solid), largest free block (dashed) and minimum since boot (dotted) on one kB scale
// from zero, so a fragmenting heap shows as the dashed line falling away while the solid one holds.
// The minimum is a watermark that only steps down: it shows when a dip too short for the 5-min
// samples happened, which the free line alone cannot.
function Spark({ trend }) {
  const { samples } = trend;
  const svg = useRef(null);
  const [w, h] = useBox(svg, 320, 104);
  const max = Math.max(1, ...samples.map((s) => s[1]));
  const X = (i) => ((i / (samples.length - 1)) * w).toFixed(1);
  const Y = (v) => (h - 2 - (v / max) * (h - 4)).toFixed(1);
  const path = (col) => "M" + samples.map((s, i) => `${X(i)} ${Y(s[col])}`).join(" L ");
  const last = samples[samples.length - 1];

  const { i, props } = useScrub(samples.length);
  const sel = i === null ? null : samples[i];
  const text = sel && `${clockAt(sampleAt(trend, i))} · ${S.memFree} ${num(sel[1])} kB · `
    + `${S.memLargest} ${num(sel[3])} kB · ${S.memMinShort} ${num(sel[2])} kB`;
  return html`
    <${Readout} text=${text} />
    <svg ref=${svg} viewBox="0 0 ${w} ${h}" preserveAspectRatio="none" class="chart" role="img"
      aria-label=${S.memTitle} ...${props}>
      <path d=${path(2)} class="line mem3" />
      <path d=${path(3)} class="line mem2" />
      <path d=${path(1)} class="line mem" />
      <circle cx=${w} cy=${Y(last[1])} r="3.5" class="head" />
      ${sel && html`<line x1=${X(i)} y1="0" x2=${X(i)} y2=${h} class="guide" />
        <circle cx=${X(i)} cy=${Y(sel[1])} r="3.5" class="dot" />`}
    </svg>`;
}
