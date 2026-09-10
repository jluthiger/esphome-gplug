import { useState } from "preact/hooks";
import { html } from "../h.js";
import { S } from "../strings.js";

const PIN_FIELDS = [
  ["rx", S.pinRx], ["red", S.pinRed], ["green", S.pinGreen], ["blue", S.pinBlue], ["button", S.pinButton],
];

export function Hardware({ variants, value, onChange, onNext, onBack }) {
  const [adv, setAdv] = useState(false);
  const sel = variants.find((v) => v.id === value?.variant);

  function pick(v) {
    onChange({ variant: v.id, pins: { ...v.pins } });
  }
  function setPin(k, raw) {
    const n = raw === "" ? null : Number(raw);
    onChange({ ...value, pins: { ...value.pins, [k]: Number.isFinite(n) ? n : null } });
  }

  return html`
    <h2>${S.hardware}</h2>
    <p>${S.hardwareText}</p>
    ${variants.map((v) => html`
      <div class="card click ${sel?.id === v.id ? "sel" : ""}" onClick=${() => pick(v)}>
        <div class="t">${v.name}</div>
        <div class="s">${v.interface} · ${v.baud} Bd · RX ${v.pins.rx}</div>
      </div>`)}
    ${sel && html`
      <details open=${adv} onToggle=${(e) => setAdv(e.target.open)}>
        <summary>${S.pins}</summary>
        ${PIN_FIELDS.map(([k, label]) => html`
          <label>${label}</label>
          <input type="number" min="0" max="21" inputmode="numeric"
            value=${value.pins[k] ?? ""} placeholder=${S.none}
            onInput=${(e) => setPin(k, e.target.value)} />`)}
        <p class="hint">ESP32-C3: GPIO 0–21. Leer = nicht vorhanden.</p>
      </details>`}
    <div class="nav">
      <button onClick=${onBack}>${S.back}</button>
      <button class="primary" disabled=${!sel} onClick=${onNext}>${S.next}</button>
    </div>`;
}
