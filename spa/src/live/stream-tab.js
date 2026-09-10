import { useEffect, useState } from "preact/hooks";
import { html } from "../h.js";
import { S } from "../strings.js";
import { api } from "../api.js";

const POLL_MS = 10000;

function download(name, text) {
  const a = document.createElement("a");
  a.href = URL.createObjectURL(new Blob([text], { type: "text/plain" }));
  a.download = name;
  a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 2000);
}

// Raw DLMS HDLC frame capture, DLMS-only (see firmware/components/gplug_smi/frame_log.h) -- a
// DSMR-configured device never populates /api/frames, so there's a dedicated empty state for that
// rather than pretending frames exist.
export function StreamTab() {
  const [frames, setFrames] = useState(null);
  const [err, setErr] = useState(null);
  const [mode, setMode] = useState("plain");   // "raw" | "plain"
  const [open, setOpen] = useState(null);
  const [cache, setCache] = useState({});      // `${i}:${kind}` -> hex text
  const [sel, setSel] = useState([]);
  const [copied, setCopied] = useState(null);

  useEffect(() => {
    let stop = false;
    async function tick() {
      try { const f = await api.frames(); if (!stop) { setFrames(f); setErr(null); } }
      catch (e) { if (!stop) setErr(String(e.message || e)); }
    }
    tick();
    const id = setInterval(tick, POLL_MS);
    return () => { stop = true; clearInterval(id); };
  }, []);

  async function fetchText(i, kind) {
    const key = `${i}:${kind}`;
    if (cache[key] != null) return cache[key];
    const text = kind === "raw" ? await api.frameRaw(i) : await api.framePlain(i);
    setCache((c) => ({ ...c, [key]: text }));
    return text;
  }

  function toggleOpen(i) {
    setOpen((o) => (o === i ? null : i));
    if (open !== i) fetchText(i, mode).catch((e) => setErr(String(e.message || e)));
  }

  function toggleSel(i) {
    setSel((s) => (s.includes(i) ? s.filter((x) => x !== i) : s.concat(i)));
  }

  async function exportSelected() {
    const ids = sel.length ? sel : (frames?.frames || []).map((f) => f.i);
    const parts = [];
    for (const i of ids) {
      try {
        const text = await fetchText(i, mode);
        const f = frames.frames.find((x) => x.i === i);
        parts.push(`# frame ${i}  age=${f?.age ?? "?"}s  ${f?.ok ? S.streamCrcOk : S.streamCrcFail}\n${text}`);
      } catch { /* skip frames that failed to fetch, export what we have */ }
    }
    download(`gplug-${mode}-export.txt`, parts.join("\n"));
  }

  if (err && !frames) return html`<div class="err">${err}</div>`;
  if (!frames) return html`<p><span class="spin"></span> ${S.loading}</p>`;

  if (frames.protocol !== "dlms") return html`<p class="hint">${S.streamEmptyDsmr}</p>`;
  if (!frames.count) return html`<p class="hint">${S.streamEmptyWaiting}</p>`;

  const kb = ((frames.len * frames.cap * 2) / 1024).toFixed(0);

  return html`
    <div class="frame-toolbar">
      <button class="${mode === "raw" ? "active" : ""}" onClick=${() => setMode("raw")}>${S.streamRaw}</button>
      <button class="${mode === "plain" ? "active" : ""}" onClick=${() => setMode("plain")}>${S.streamPlain}</button>
    </div>
    <div class="row" style="margin-bottom:6px">
      <button onClick=${() => setSel(sel.length === frames.frames.length ? [] : frames.frames.map((f) => f.i))}>
        ${sel.length === frames.frames.length ? S.streamDeselectAll : S.streamSelectAll}
      </button>
      <button onClick=${exportSelected}>${sel.length ? S.streamExport : S.streamExportAll}</button>
    </div>
    <div class="card">
      ${frames.frames.map((f) => html`
        <div class="frame-row">
          <div class="frame-head" onClick=${() => toggleOpen(f.i)}>
            <input type="checkbox" checked=${sel.includes(f.i)}
              onClick=${(e) => { e.stopPropagation(); toggleSel(f.i); }} />
            <span class="mono t">-${f.age}s</span>
            <span class="badge ${f.ok ? "ok" : "err"}">${f.ok ? S.streamCrcOk : S.streamCrcFail}</span>
          </div>
          ${open === f.i && html`
            <div class="frame-hex">${cache[`${f.i}:${mode}`] ?? html`<span class="spin"></span>`}</div>
            <div class="row">
              <button onClick=${() => download(`gplug-frame${f.i}-${mode}.txt`, cache[`${f.i}:${mode}`] || "")}>
                ${S.streamDownload}
              </button>
              <button onClick=${async () => {
                const text = cache[`${f.i}:${mode}`] ?? await fetchText(f.i, mode);
                navigator.clipboard?.writeText(text);
                setCopied(f.i); setTimeout(() => setCopied(null), 1500);
              }}>${copied === f.i ? S.streamCopied : S.streamCopy}</button>
            </div>`}
        </div>`)}
    </div>
    <p class="hint">${S.streamBuffer(frames.len, kb)}</p>
    ${err && html`<div class="err">${err}</div>`}`;
}
