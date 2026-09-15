import { useEffect, useRef, useState } from "preact/hooks";
import { html } from "../h.js";
import { S } from "../strings.js";
import { api } from "../api.js";
import { copyText, download } from "../dl.js";

const POLL_MS = 10000;

// The frames the device actually received (firmware/components/gplug_smi/frame_log.h): DLMS HDLC
// frames or DSMR P1 telegrams, whichever the configured profile speaks. The firmware says which via
// `encoding`, and the two view modes mean different things per protocol -- ciphertext vs decrypted
// APDU for DLMS, hex vs the telegram's own ASCII for DSMR.
// "Ansicht eingefroren" only pauses this view's polling; the device keeps capturing, and the copy
// says so.
export function StreamTab({ wide }) {
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

  async function fetchText(i, kind, force) {
    const key = `${i}:${kind}`;
    if (!force && cache[key] != null) return cache[key];
    const text = kind === "raw" ? await api.frameRaw(i) : await api.framePlain(i);
    setCache((c) => ({ ...c, [key]: text }));
    return text;
  }

  function toggleOpen(i) {
    // On a wide screen the row selects what the pane beside the list shows; there is nothing to
    // collapse, so a second click keeps it open.
    const next = open === i && !wide ? null : i;
    setOpen(next);
    if (next != null) fetchText(next, mode).catch((e) => setErr(String(e.message || e)));
  }

  // `i` is a position in the device's ring, newest first, not an identity: when a frame arrives
  // every capture moves one place down, and text fetched for position i now belongs to i+1. The
  // last poll's newest frame should now be older by the time that has passed; a newest frame
  // younger than that (2 s slack for whole-second ages), or a different count, means new arrivals.
  // Comparing ages alone would miss a meter that sends every second, whose newest frame is 0 s old
  // at every poll. Then the fetched texts are dropped and the open position is read again, so the
  // body always matches the header drawn above it.
  const head = useRef(null);
  useEffect(() => {
    const f0 = frames?.frames?.[0];
    const prev = head.current;
    head.current = f0 ? { age: f0.age, count: frames.count, t: Date.now() } : null;
    if (!f0 || !prev) return;
    const expected = prev.age + (Date.now() - prev.t) / 1000;
    if (f0.age + 2 >= expected && frames.count === prev.count) return;
    setCache({});
    if (open != null) fetchText(open, mode, true).catch((e) => setErr(String(e.message || e)));
  }, [frames]);

  // The wide layout's detail pane is never left empty while there are frames: it opens on the
  // newest one (position 0), and the effect above keeps its text current.
  const hasFrames = !!frames?.count;
  useEffect(() => {
    if (wide && hasFrames && open == null) toggleOpen(0);
  }, [wide, hasFrames]);
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
  if (frames.protocol === "none") return html`
    <div class="card">
      <p class="hint">${S.streamEmptyNone}</p>
      <div class="row" style="margin-top:12px">
        <button class="primary" onClick=${() => { location.hash = "#setup/meter"; }}>${S.fixMeter}</button>
      </div>
    </div>`;

  // DSMR telegrams are ASCII to begin with, so "plain" is the telegram itself and "raw" is a hex
  // dump of the same bytes -- one capture, two views. DLMS keeps ciphertext vs decrypted APDU.
  const text = frames.encoding === "text";
  const list = frames.frames;
  const kb = Math.round((frames.len * frames.cap) / 1024);
  const allSel = list.length > 0 && sel.length === list.length;

  const modeSeg = html`
    <div class="seg">
      <button class=${mode === "raw" ? "active" : ""} onClick=${() => pickMode("raw")}>${text ? S.streamHex : S.streamRaw}</button>
      <button class=${mode === "plain" ? "active" : ""} onClick=${() => pickMode("plain")}>${text ? S.streamText : S.streamPlain}</button>
    </div>`;
  const tailRow = html`
    <div class="card switchrow">
      <div><div class="t">${tail ? S.tailOn : S.tailOff}</div><div class="s">${tail ? S.tailOnNote(frames.len, kb) : S.tailOffNote}</div></div>
      <button class="switch ${tail ? "on" : ""}" onClick=${() => setTail(!tail)}><span></span></button>
    </div>`;
  const waiting = !frames.count && html`<div class="card"><p class="hint">${S.streamEmptyWaiting}</p></div>`;
  const selRow = html`
    <div class="selrow">
      <button onClick=${() => setSel(allSel ? [] : list.map((f) => f.i))}>${allSel ? S.streamDeselectAll : S.streamSelectAll}</button>
      <span>${sel.length ? S.selCount(sel.length, list.length) : S.selNone}</span>
    </div>`;
  const lenOf = (f) => ((text || mode === "raw") ? f.raw_len : f.plain_len);
  const frameMeta = (f) => html`
    <span class="sq ${f.ok ? "" : "bad"}"></span>
    <span class="ts">−${f.age} s</span>
    <span class="meta">${lenOf(f)} B · ${text ? S.frameMetaTelegram : mode === "raw" ? S.frameMetaRaw : S.frameMetaPlain}${(mode === "raw" || text ? f.raw_trunc : f.plain_trunc) ? " · …" : ""}</span>
    <span class="crc ${f.ok ? "" : "bad"}">${f.ok ? S.crcOk : S.crcFail}</span>`;
  const frameHead = (f) => html`
    <div class="head">
      <button class="chk ${sel.includes(f.i) ? "on" : ""}" aria-label=${S.selectFrame} onClick=${() => toggleSel(f.i)}><i>${sel.includes(f.i) ? "✓" : ""}</i></button>
      <button class="open" onClick=${() => toggleOpen(f.i)}>${frameMeta(f)}</button>
    </div>`;
  const frameBody = (f) => {
    const key = `${f.i}:${mode}`;
    return html`
      <div class="body">
        <div class="hex ${text && mode === "plain" ? "text" : ""}">${lenOf(f) ? (cache[key] ?? html`<span class="spin"></span>`) : "–"}</div>
        <div class="row">
          <button class="primary" disabled=${!cache[key]} onClick=${() => download(`gplug-${mode}-frame${f.i}.txt`, cache[key])}>${S.frameTxt}</button>
          <button disabled=${!cache[key]} onClick=${async () => {
            // Only report success when the clipboard actually took it: over plain HTTP
            // this can fail, and a button that lies is worse than one that says no.
            const ok = await copyText(cache[key]);
            setCopied(ok ? f.i : "x" + f.i);
            setTimeout(() => setCopied(null), 2500);
          }}>${copied === f.i ? S.copied : copied === "x" + f.i ? S.copyFailedShort : S.copy}</button>
        </div>
      </div>`;
  };
  const exportCard = html`
    <div class="card">
      <div class="lbl">${sel.length ? S.exportSel : S.exportAll}</div>
      <p class="hint" style="margin:6px 0 0">${sel.length ? S.exportNoteSel(sel.length) : S.exportNoteAll(list.length)}</p>
      <div class="row" style="margin-top:12px">
        <button class="primary" onClick=${() => exportAs(text ? "plain" : "raw")}>${text ? S.dlText : S.dlRaw}</button>
        <button onClick=${() => exportAs(text ? "raw" : "plain")}>${text ? S.streamHex + " .txt" : S.dlPlain}</button>
      </div>
      <div class="num" style="font-size:.7rem;color:var(--muted2);margin-top:9px">${text ? S.dsmrNote : S.dlHint}</div>
    </div>`;

  // Wide screens: master/detail. The list keeps its selection and export controls on the left,
  // the open frame gets a pane of its own on the right, tall enough to read a telegram whole.
  if (wide) {
    const shown = list.find((f) => f.i === open);
    return html`
      ${waiting || html`
        <div class="streamgrid">
          <div class="s-list">
            ${tailRow}${selRow}
            ${list.map((f) => html`<div class="card frame ${open === f.i ? "on" : ""}">${frameHead(f)}</div>`)}
            ${exportCard}
          </div>
          <div class="card s-detail">
            ${modeSeg}
            ${shown && html`<div class="frame on"><div class="head"><div class="open">${frameMeta(shown)}</div></div>${frameBody(shown)}</div>`}
          </div>
        </div>`}
      ${waiting && tailRow}
      ${err && html`<div class="err">${err}</div>`}`;
  }

  return html`
    ${modeSeg}${tailRow}${waiting}
    ${frames.count > 0 && html`
      ${selRow}
      ${list.map((f) => html`
        <div class="card frame" style="border-color:${open === f.i ? "var(--border2)" : "var(--border)"}">
          ${frameHead(f)}
          ${open === f.i && frameBody(f)}
        </div>`)}
      ${exportCard}`}
    ${err && html`<div class="err">${err}</div>`}`;
}
