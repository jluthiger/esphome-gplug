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
// rather than pretending frames exist. "Ansicht eingefroren" only pauses this view's polling; the
// device keeps capturing regardless, and the copy says so.
export function StreamTab() {
  const [frames, setFrames] = useState(null);
  const [err, setErr] = useState(null);
  const [mode, setMode] = useState("plain");   // "raw" | "plain"
  const [tail, setTail] = useState(true);
  const [open, setOpen] = useState(null);
  const [cache, setCache] = useState({});      // `${i}:${kind}` -> hex text
  const [sel, setSel] = useState([]);
  const [copied, setCopied] = useState(null);

  useEffect(() => {
    if (!tail) return;
    let stop = false;
    async function tick() {
      try { const f = await api.frames(); if (!stop) { setFrames(f); setErr(null); } }
      catch (e) { if (!stop) setErr(String(e.message || e)); }
    }
    tick();
    const id = setInterval(tick, POLL_MS);
    return () => { stop = true; clearInterval(id); };
  }, [tail]);

  async function fetchText(i, kind) {
    const key = `${i}:${kind}`;
    if (cache[key] != null) return cache[key];
    const text = kind === "raw" ? await api.frameRaw(i) : await api.framePlain(i);
    setCache((c) => ({ ...c, [key]: text }));
    return text;
  }

  function toggleOpen(i) {
    const next = open === i ? null : i;
    setOpen(next);
    if (next != null) fetchText(next, mode).catch((e) => setErr(String(e.message || e)));
  }
  function pickMode(m) {
    setMode(m);
    if (open != null) fetchText(open, m).catch((e) => setErr(String(e.message || e)));
  }
  const toggleSel = (i) => setSel((s) => (s.includes(i) ? s.filter((x) => x !== i) : s.concat(i)));

  async function exportAs(kind) {
    const all = frames?.frames || [];
    const ids = sel.length ? sel : all.map((f) => f.i);
    const parts = [];
    for (const i of ids) {
      const f = all.find((x) => x.i === i);
      try {
        const text = await fetchText(i, kind);
        parts.push(`# frame ${i}  age=${f?.age ?? "?"}s  ${f?.ok ? S.crcOk : S.crcFail}\n${text}`);
      } catch { parts.push(`# frame ${i}  age=${f?.age ?? "?"}s  ${f?.ok ? S.crcOk : S.crcFail}\n(no ${kind} data)\n`); }
    }
    download(`gplug-${kind}-${ids.length}frames.txt`, parts.join("\n"));
  }

  if (err && !frames) return html`<div class="err">${err}</div>`;
  if (!frames) return html`<p><span class="spin"></span> ${S.loading}</p>`;
  if (frames.protocol !== "dlms") return html`<div class="card"><p class="hint">${S.streamEmptyDsmr}</p></div>`;

  const list = frames.frames;
  const kb = Math.round((frames.len * frames.cap * 2) / 1024);
  const allSel = list.length > 0 && sel.length === list.length;

  return html`
    <div class="seg">
      <button class=${mode === "raw" ? "active" : ""} onClick=${() => pickMode("raw")}>${S.streamRaw}</button>
      <button class=${mode === "plain" ? "active" : ""} onClick=${() => pickMode("plain")}>${S.streamPlain}</button>
    </div>
    <div class="card switchrow">
      <div><div class="t">${tail ? S.tailOn : S.tailOff}</div><div class="s">${tail ? S.tailOnNote(frames.len, kb) : S.tailOffNote}</div></div>
      <button class="switch ${tail ? "on" : ""}" onClick=${() => setTail(!tail)}><span></span></button>
    </div>
    ${!frames.count && html`<div class="card"><p class="hint">${S.streamEmptyWaiting}</p></div>`}
    ${frames.count > 0 && html`
      <div class="selrow">
        <button onClick=${() => setSel(allSel ? [] : list.map((f) => f.i))}>${allSel ? S.streamDeselectAll : S.streamSelectAll}</button>
        <span>${sel.length ? S.selCount(sel.length, list.length) : S.selNone}</span>
      </div>
      ${list.map((f) => {
        const key = `${f.i}:${mode}`;
        const len = mode === "raw" ? f.raw_len : f.plain_len;
        return html`
          <div class="card frame" style="border-color:${open === f.i ? "var(--border2)" : "var(--border)"}">
            <div class="head">
              <button class="chk ${sel.includes(f.i) ? "on" : ""}" aria-label="Frame auswählen" onClick=${() => toggleSel(f.i)}><i>${sel.includes(f.i) ? "✓" : ""}</i></button>
              <button class="open" onClick=${() => toggleOpen(f.i)}>
                <span class="sq ${f.ok ? "" : "bad"}"></span>
                <span class="ts">−${f.age} s</span>
                <span class="meta">${len} B · ${mode === "raw" ? S.frameMetaRaw : S.frameMetaPlain}${(mode === "raw" ? f.raw_trunc : f.plain_trunc) ? " · …" : ""}</span>
                <span class="crc ${f.ok ? "" : "bad"}">${f.ok ? S.crcOk : S.crcFail}</span>
              </button>
            </div>
            ${open === f.i && html`
              <div class="body">
                <div class="hex">${len ? (cache[key] ?? html`<span class="spin"></span>`) : "–"}</div>
                <div class="row">
                  <button class="primary" disabled=${!cache[key]} onClick=${() => download(`gplug-${mode}-frame${f.i}.txt`, cache[key])}>${S.frameTxt}</button>
                  <button disabled=${!cache[key]} onClick=${() => { navigator.clipboard?.writeText(cache[key]); setCopied(f.i); setTimeout(() => setCopied(null), 1500); }}>${copied === f.i ? S.copied : S.copy}</button>
                </div>
              </div>`}
          </div>`;
      })}
      <div class="card">
        <div class="lbl">${sel.length ? S.exportSel : S.exportAll}</div>
        <p class="hint" style="margin:6px 0 0">${sel.length ? S.exportNoteSel(sel.length) : S.exportNoteAll(list.length)}</p>
        <div class="row" style="margin-top:12px">
          <button class="primary" onClick=${() => exportAs("raw")}>${S.dlRaw}</button>
          <button onClick=${() => exportAs("plain")}>${S.dlPlain}</button>
        </div>
        <div class="mono" style="font-size:.7rem;color:var(--muted2);margin-top:9px">${S.dlHint}</div>
      </div>`}
    ${err && html`<div class="err">${err}</div>`}`;
}
