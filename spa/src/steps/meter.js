import { useEffect, useState } from "preact/hooks";
import { html } from "../h.js";
import { S, presetLabel } from "../strings.js";
import { api } from "../api.js";

const HEX32 = /^[0-9a-fA-F]{32}$/;
const POLL_MS = 2000;
const LONG_WAIT_S = 30;

// storedKey: the device already holds a GUEK (status.meter.encrypted). It is never sent back, so
// instead of retyping it the user can keep it (keep_key, see GplugSmi::merge_stored_keys_) --
// the common case when only the profile was wrong.
//
// Detection: from the moment the hardware step is committed, the firmware reads the protocol off
// the header bytes of whatever the meter sends (/api/live "detect", protocol_sniff.h). This step
// polls it and follows it: when exactly one of the variant's profiles fits, it is selected and
// shown alone, so the user only confirms (and types the key when the line is encrypted). Several
// fitting profiles (gPlugM has two DLMS ones) narrow the list to those. Tapping a card ends the
// following; the wider lists stay reachable behind the "other profile" / "show all" actions.
export function Meter({ presets, variant, value, storedKey, onChange, onNext, onBack }) {
  const [expand, setExpand] = useState(0);   // 0 = fitting profiles, 1 = this variant's, 2 = all
  const [manual, setManual] = useState(false);
  const [live, setLive] = useState(null);
  const [t0] = useState(Date.now());
  const [now, setNow] = useState(Date.now());

  useEffect(() => {
    let stop = false;
    async function tick() {
      try { const l = await api.live(); if (!stop) setLive(l); } catch { /* keep polling */ }
      if (!stop) setNow(Date.now());
    }
    tick();
    const id = setInterval(tick, POLL_MS);
    return () => { stop = true; clearInterval(id); };
  }, []);

  const sniffs = !!live && live.detect !== undefined;   // firmware with the sniffer at all
  const det = live?.detect?.protocol ? live.detect : null;
  const mine = presets.filter((p) => p.variant === variant);
  const fit = det ? mine.filter((p) => p.protocol === det.protocol && (det.encrypted == null || p.encrypted === det.encrypted)) : [];
  const sel = presets.find((p) => p.id === value?.preset);

  // Follow the detection until the user picks by hand: whenever the selection is not a fitting
  // profile, select the first one -- including over the device's stored profile, which is exactly
  // the "change meter profile" case after a wrong first setup.
  const fitIds = fit.map((p) => p.id).join(",");
  useEffect(() => {
    if (manual || fit.length === 0 || fit.some((p) => p.id === sel?.id)) return;
    onChange({ ...value, preset: fit[0].id, key: value?.key || "" });
  }, [manual, fitIds, sel?.id]);

  function pick(p) {
    setManual(true);
    onChange({ ...value, preset: p.id, key: value?.key || "" });
  }

  const keep = storedKey && value?.keepKey;
  const keyOk = !sel?.encrypted || keep || HEX32.test(value?.key || "");

  // Which list is on screen: the narrowest tier that has something in it, widened on request. A
  // tier that would show the same cards as the one before it is skipped.
  const tiers = [];
  if (fit.length) tiers.push(fit);
  if (mine.length && mine.length !== fit.length) tiers.push(mine);
  if (presets.length !== (tiers.length ? tiers[tiers.length - 1].length : 0)) tiers.push(presets);
  const level = Math.min(expand, tiers.length - 1);
  const list = tiers[level];
  const more = level < tiers.length - 1;
  const moreLabel = level === 0 && fit.length ? S.otherProfile : S.showAll;

  const waitS = Math.round((now - t0) / 1000);
  const signal = live?.rx_age != null && live.rx_age < 60;
  const protoLabel = (d) => (d.protocol === "dsmr" ? S.detectProtoDsmr : S.detectProtoDlms) +
    (d.protocol === "dlms" && d.encrypted != null ? ", " + (d.encrypted ? S.detectEncrypted : S.detectPlain) : "");

  return html`
    <h2>${S.meter}</h2>
    <p>${sniffs || !live ? S.meterText : S.meterTextManual}</p>
    ${sniffs && det && html`<div class="card">
      <div class="lbl">${S.detectTitle}</div>
      <p style="margin:10px 0 0"><span class="badge ok">${S.detectFound}</span> ${protoLabel(det)}</p>
      ${det.encrypted && html`<p class="hint" style="margin:6px 0 0">${S.detectKeyNeeded}</p>`}
      ${fit.length === 0 && html`<p class="hint" style="margin:6px 0 0">${S.detectNoFit}</p>`}
      ${fit.length > 1 && html`<p class="hint" style="margin:6px 0 0">${S.detectPick}</p>`}
    </div>`}
    ${sniffs && !det && html`<div class="card">
      <div class="lbl">${S.detectTitle}</div>
      <p style="margin:10px 0 0"><span class="spin"></span> ${signal ? S.detectSignal : S.detectWaiting}
        <span class="num muted"> ${waitS} s</span></p>
      <p class="hint" style="margin:6px 0 0">${!signal && waitS >= LONG_WAIT_S ? S.detectWaitingLong : S.detectWaitingHint}</p>
    </div>`}
    ${mine.length === 0 && html`<p class="hint">${S.noPresetForVariant}</p>`}
    ${list.map((p) => html`
      <div class="card click ${sel?.id === p.id ? "sel" : ""}" onClick=${() => pick(p)}>
        <div class="t">${presetLabel(p)}</div>
        <div class="s">
          ${p.protocol.toUpperCase()} · ${p.baud} Bd · ${p.obis.length} ${S.values}
          ${p.encrypted && html` · <span class="badge warn">${S.encrypted}</span>`}
        </div>
      </div>`)}
    ${more && html`<button onClick=${() => setExpand(level + 1)}>${moreLabel}</button>`}
    ${sel?.encrypted && storedKey && html`
      <div class="card switchrow" style="margin-top:14px">
        <div><div class="t">${S.keepKey}</div><div class="s">${S.keepKeyHint}</div></div>
        <button class="switch ${keep ? "on" : ""}" role="switch" aria-checked=${!!keep}
          onClick=${() => onChange({ ...value, keepKey: !value.keepKey })}><span></span></button>
      </div>`}
    ${sel?.encrypted && !keep && html`
      <label>${S.key}</label>
      <input class="num" type="text" autocomplete="off" spellcheck="false" maxlength="32"
        value=${value.key} placeholder="0123456789ABCDEF0123456789ABCDEF"
        onInput=${(e) => onChange({ ...value, key: e.target.value.trim() })} />
      <p class="hint">${S.keyHint}</p>
      ${value.key && !keyOk && html`<div class="err">${S.keyInvalid}</div>`}`}
    ${sel && html`
      <details>
        <summary>${S.values} (${sel.obis.length})</summary>
        <div class="kv num">
          ${sel.obis.map((o) => html`<b>${o.obis}</b><span>${o.name}${o.unit ? " [" + o.unit + "]" : ""}</span>`)}
        </div>
      </details>`}
    <div class="nav">
      <button onClick=${onBack}>${S.back}</button>
      <button class="primary" disabled=${!sel || !keyOk} onClick=${onNext}>${S.next}</button>
    </div>`;
}
