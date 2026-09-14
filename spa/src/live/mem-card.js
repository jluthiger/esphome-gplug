import { useEffect, useRef, useState } from "preact/hooks";
import { html } from "../h.js";
import { S } from "../strings.js";
import { api } from "../api.js";
import { dur, num } from "../fmt.js";
import { useBox } from "../box.js";

// Heap figures and the device's 24 h heap trend (/api/heap, heap_monitor.h). The point is a slow
// leak: one free-heap number says nothing, a line that keeps falling over hours does. The device
// samples every 5 min, so polling faster would only redraw the same line.
const MEM_POLL_MS = 300000;

const kb = (b) => (b == null ? null : `${num(b / 1024)} kB`);

export function MemCard({ status }) {
  const [st, setSt] = useState(status);
  const [trend, setTrend] = useState(null);

  useEffect(() => {
    let stop = false;
    // /api/status is otherwise loaded once per page, so the card refreshes its own copy alongside
    // the trend -- otherwise "free" would stay at its page-load value next to a moving line.
    const load = () => {
      api.heap().then((r) => { if (!stop) setTrend(r); }).catch(() => {});
      api.status().then((r) => { if (!stop) setSt(r); }).catch(() => {});
    };
    load();
    const id = setInterval(load, MEM_POLL_MS);
    return () => { stop = true; clearInterval(id); };
  }, []);

  const m = st?.mem;
  const rows = [
    [S.memFree, kb(m?.free ?? st?.heap)],
    [S.memMin, kb(m?.min_free)],
    [S.memLargest, kb(m?.largest)],
    [S.memStack, m ? `${num(m.stack_loop)} / ${num(m.stack_httpd)} B` : null],
  ].filter(([, v]) => v);

  const samples = trend?.samples || [];
  // From the sample count, not the uptimes: the device's uptime wraps after 49.7 days.
  const span = Math.max(0, samples.length - 1) * (trend?.period || 300);

  return html`
    <div class="card">
      <div class="lbl" style="margin-bottom:12px">${S.memTitle}</div>
      <div class="kv">${rows.map(([k, v]) => html`<b>${k}</b><span>${v}</span>`)}</div>
      ${samples.length >= 2 ? html`
        <${Spark} samples=${samples} />
        <div class="axis wrap"><span>${S.memTrend(dur(span))}</span><span>— ${S.memFree} · - - ${S.memLargest}</span></div>
        <p class="hint" style="margin:8px 0 0">${S.memHint}</p>`
      : trend && html`<p class="hint" style="margin:12px 0 0">${S.memWaiting}</p>`}
    </div>`;
}

// Free heap (solid) and largest free block (dashed) on one kB scale from zero, so a fragmenting heap
// shows as the dashed line falling away while the solid one holds.
function Spark({ samples }) {
  const svg = useRef(null);
  const [w, h] = useBox(svg, 320, 104);
  const max = Math.max(1, ...samples.map((s) => s[1]));
  const X = (i) => ((i / (samples.length - 1)) * w).toFixed(1);
  const Y = (v) => (h - 2 - (v / max) * (h - 4)).toFixed(1);
  const path = (col) => "M" + samples.map((s, i) => `${X(i)} ${Y(s[col])}`).join(" L ");
  const last = samples[samples.length - 1];
  return html`
    <svg ref=${svg} viewBox="0 0 ${w} ${h}" preserveAspectRatio="none" class="chart">
      <path d=${path(3)} class="line mem2" />
      <path d=${path(1)} class="line mem" />
      <circle cx=${w} cy=${Y(last[1])} r="3.5" class="head" />
    </svg>`;
}
