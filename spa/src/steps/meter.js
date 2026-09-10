import { useState } from "preact/hooks";
import { html } from "../h.js";
import { S } from "../strings.js";

const HEX32 = /^[0-9a-fA-F]{32}$/;

export function Meter({ presets, variant, value, onChange, onNext, onBack }) {
  const [all, setAll] = useState(false);
  const mine = presets.filter((p) => p.variant === variant);
  const list = all || mine.length === 0 ? presets : mine;
  const sel = presets.find((p) => p.id === value?.preset);
  const keyOk = !sel?.encrypted || HEX32.test(value?.key || "");

  return html`
    <h2>${S.meter}</h2>
    <p>${S.meterText}</p>
    ${mine.length === 0 && html`<p class="hint">${S.noPresetForVariant}</p>`}
    ${list.map((p) => html`
      <div class="card click ${sel?.id === p.id ? "sel" : ""}"
           onClick=${() => onChange({ preset: p.id, key: value?.key || "" })}>
        <div class="t">${p.name}</div>
        <div class="s">
          ${p.protocol.toUpperCase()} · ${p.baud} Bd · ${p.obis.length} ${S.values}
          ${p.encrypted && html` · <span class="badge warn">${S.encrypted}</span>`}
        </div>
      </div>`)}
    ${!all && mine.length > 0 && mine.length < presets.length && html`
      <button onClick=${() => setAll(true)}>${S.showAll}</button>`}
    ${sel?.encrypted && html`
      <label>${S.key}</label>
      <input class="mono" type="text" autocomplete="off" spellcheck="false" maxlength="32"
        value=${value.key} placeholder="0123456789ABCDEF0123456789ABCDEF"
        onInput=${(e) => onChange({ ...value, key: e.target.value.trim() })} />
      <p class="hint">${S.keyHint}</p>
      ${value.key && !keyOk && html`<div class="err">${S.keyInvalid}</div>`}`}
    ${sel && html`
      <details>
        <summary>${S.values} (${sel.obis.length})</summary>
        <div class="kv mono">
          ${sel.obis.map((o) => html`<b>${o.obis}</b><span>${o.name}${o.unit ? " [" + o.unit + "]" : ""}</span>`)}
        </div>
      </details>`}
    <div class="nav">
      <button onClick=${onBack}>${S.back}</button>
      <button class="primary" disabled=${!sel || !keyOk} onClick=${onNext}>${S.next}</button>
    </div>`;
}
