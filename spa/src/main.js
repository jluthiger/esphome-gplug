import { render } from "preact";
import { useEffect, useState } from "preact/hooks";
import { html } from "./h.js";
import { S } from "./strings.js";
import { api } from "./api.js";
import { Welcome } from "./steps/welcome.js";
import { Hardware } from "./steps/hardware.js";
import { Meter } from "./steps/meter.js";
import { Done } from "./steps/done.js";
import { Live } from "./live/index.js";

// No "wifi" step: this SPA is only ever reachable after the device has already joined WiFi
// (the stock captive_portal handles that first join, before gplug_smi's own handler -- and
// thus this SPA -- resumes serving "/"). A "connect to your network" step here would always be
// a no-op re-confirmation of a connection that already exists. Changing to a *different* network
// later is still possible, from the Live view's "WLAN ändern" panel.
const STEPS = ["welcome", "hardware", "meter", "done"];

// Two top-level routes, kept in location.hash so each is a real, bookmarkable screen rather than
// a full page reload dressed up as navigation:
//   #setup -> the wizard (STEPS above)
//   #live  -> the live view (steps/live.js), the day-to-day home screen once configured
// The very first load has no hash yet; default to whichever screen makes sense once /api/status
// answers (configured + on WiFi -> live, otherwise the wizard starts a fresh device's setup).
function initialRoute() {
  if (location.hash === "#live") return "live";
  if (location.hash === "#setup") return "wizard";
  return null;
}

function App() {
  const [route, setRoute] = useState(initialRoute);
  const [step, setStep] = useState(0);
  const [status, setStatus] = useState(null);
  const [data, setData] = useState(null); // {variants, presets}
  const [err, setErr] = useState(null);
  const [busy, setBusy] = useState(false);

  const [hw, setHw] = useState(null);                 // {variant, pins}
  const [meter, setMeter] = useState({ preset: null, key: "" });

  async function load() {
    setErr(null);
    try {
      const [st, d] = await Promise.all([api.status(), api.presets()]);
      setStatus(st); setData(d);
      // Pre-select what the device already knows
      if (st.hardware?.variant && !hw) setHw({ variant: st.hardware.variant, pins: st.hardware.pins });
      if (st.meter?.preset) setMeter((m) => ({ ...m, preset: st.meter.preset }));
    } catch (e) { setErr(String(e.message || e)); }
  }
  useEffect(() => { load(); }, []);

  // Route not decided from the URL yet: pick one once status is in.
  useEffect(() => {
    if (route !== null || !status) return;
    const configured = status.hardware && status.meter;
    openRoute(configured && status.wifi?.connected ? "live" : "wizard");
  }, [route, status]);

  function openRoute(r) {
    location.hash = r === "live" ? "#live" : "#setup";
    setRoute(r);
  }
  const openLive = () => openRoute("live");

  // Persist each step to the device as the user leaves it.
  async function commit(from) {
    setBusy(true); setErr(null);
    try {
      if (from === "hardware") await api.setHardware(hw);
      if (from === "meter") {
        const p = data.presets.find((x) => x.id === meter.preset);
        await api.setMeter({ preset: p.id, key: p.encrypted ? meter.key : undefined, descriptor: toDescriptor(p, hw) });
      }
      setStep((s) => s + 1);
    } catch (e) { setErr(String(e.message || e)); }
    setBusy(false);
  }
  const back = () => setStep((s) => Math.max(0, s - 1));

  const name = STEPS[step];
  let body;
  if (err && !data) body = html`<div class="err">${err}</div><button onClick=${load}>${S.retry}</button>`;
  else if (!data || route === null) body = html`<p><span class="spin"></span> ${S.loading}</p>`;
  else if (route === "live") body = html`<${Live} status=${status} presets=${data.presets} />`;
  else if (name === "welcome") body = html`<${Welcome} status=${status} onNext=${() => setStep(1)} />`;
  else if (name === "hardware") body = html`<${Hardware} variants=${data.variants} value=${hw} onChange=${setHw}
      onBack=${back} onNext=${() => commit("hardware")} />`;
  else if (name === "meter") body = html`<${Meter} presets=${data.presets} variant=${hw?.variant} value=${meter}
      onChange=${setMeter} onBack=${back} onNext=${() => commit("meter")} />`;
  else body = html`<${Done} onOpenLive=${openLive} />`;

  // The live screen draws its own header (status bar, screen title, data pill -- see live/index.js);
  // the brand/title/progress chrome is wizard-only.
  const showSteps = route === "wizard" && data;
  return html`
    ${route !== "live" && html`<div class="brand">${S.brand}</div><h1>${S.title}</h1>`}
    ${showSteps && html`<div class="steps">${STEPS.map((_, i) => html`<i class=${i < step ? "done" : i === step ? "cur" : ""}></i>`)}</div>`}
    ${body}
    ${busy && html`<p style="text-align:center"><span class="spin"></span></p>`}
    ${err && data && html`<div class="err">${err}</div>`}`;
}

// Device-side descriptor: preset minus SPA-only fields, pins from hardware step.
function toDescriptor(p, hw) {
  return {
    schema: 1,
    protocol: p.protocol,
    mode: p.mode,
    baud: p.baud,
    rx: hw?.pins?.rx ?? p.rx,
    serial_flags: p.serial_flags,
    buffer: p.buffer,
    obis: p.obis,
  };
}

render(html`<${App} />`, document.getElementById("app"));
