import { useEffect, useMemo, useState } from "preact/hooks";
import { html } from "../h.js";
import { S } from "../strings.js";
import { api } from "../api.js";
import { Collapsible } from "./collapsible.js";
import { compile, render, itemOk, toFields, PRESETS, TOPIC_BUF, PAYLOAD_BUF } from "./mqtt-template.js";

// MQTT publishing (GET/POST /api/config/mqtt, firmware README "MQTT"): broker settings and the
// topic/payload templates, with a preview rendered here from the page's /api/live values by a port
// of the firmware's template code. The device compiles again on save and has the last word.
//
// The draft lives in SetupTab (`state`/`setState`), because a closed Collapsible unmounts its body
// and a half-typed template must not vanish with it. Hidden on firmware without the route: its
// /api/status has no `mqtt` block.

const STATUS_POLL_MS = 5000;
const CONN_ERR = { tcp: "mqttErrTcp", refused: "mqttErrRefused", auth: "mqttErrAuth" };
const TPL_ERR = { tpl_syntax: "tplSyntax", tpl_unknown: "tplUnknown", tpl_context: "tplContext", tpl_topic: "tplTopic",
  tpl_empty: "tplEmpty", tpl_too_long: "tplTooLong", tpl_overflow: "tplOverflow" };
const PRESET_LABEL = { json: "mqttPresetJson", each: "mqttPresetEach", influx: "mqttPresetInflux" };

function badgeOf(m) {
  if (!m || m.state === "off") return ["", S.mqttOff];
  if (m.state === "connected") return ["ok", S.mqttConnected];
  if (m.state === "error") return ["err", S.mqttError];
  return [m.error ? "err" : "warn", m.error ? S[CONN_ERR[m.error]] || m.error : S.mqttConnecting];
}

export function MqttCard({ status, live, state, setState }) {
  if (!status?.mqtt) return null;
  const [cls, text] = badgeOf(status.mqtt);
  return html`
    <${Collapsible} id="mqtt" title=${S.mqttTitle} summary=${html`<span class="badge ${cls}">${text}</span>`}>
      <${MqttBody} status=${status} live=${live} state=${state} setState=${setState} />
    <//>`;
}

// A template error as one sentence: what is wrong, and where (byte offset + 1, which is the
// character position for the ASCII templates users type).
const tplMsg = (field, code, pos) =>
  S.tplAt(field === "topic" ? S.mqttTopic : S.mqttPayload, S[TPL_ERR[code]] || code, pos + 1);

function MqttBody({ status, live, state, setState }) {
  const [st, setSt] = useState(status.mqtt);
  const [busy, setBusy] = useState(false);

  // Load once per page; the draft then survives closing the card.
  useEffect(() => {
    if (state) return;
    api.mqttConfig()
      .then((cfg) => setState({ cfg, draft: { ...cfg, password: "" }, err: "", saved: false }))
      .catch((e) => setState({ loadErr: String(e.message) }));
  }, []);

  // /api/status is otherwise loaded once per page; the badge and counters refresh while open.
  useEffect(() => {
    let stop = false;
    const id = setInterval(() => api.status().then((r) => { if (!stop && r.mqtt) setSt(r.mqtt); }).catch(() => {}), STATUS_POLL_MS);
    return () => { stop = true; clearInterval(id); };
  }, []);

  const d = state?.draft;
  const f = useMemo(() => state?.cfg && toFields(state.cfg), [state?.cfg]);
  const each = d?.mode === "each";
  const topicC = useMemo(() => d && compile(d.topic, each, true, f), [d?.topic, each, f]);
  const payloadC = useMemo(() => d && compile(d.payload, each, false, f), [d?.payload, each, f]);

  if (state?.loadErr) return html`<div class="err">${state.loadErr}</div>`;
  if (!d) return html`<p class="hint">${S.loading}</p>`;

  const set = (patch) => setState({ ...state, draft: { ...d, ...patch }, err: "", saved: false });
  const num = (k) => (e) => set({ [k]: e.target.value === "" ? "" : Number(e.target.value) });
  const str = (k) => (e) => set({ [k]: e.target.value });
  const preset = PRESETS.find((p) => p.topic === d.topic && p.payload === d.payload && p.each === each);
  const tplErr = !topicC.ok ? tplMsg("topic", topicC.code, topicC.pos) : !payloadC.ok ? tplMsg("payload", payloadC.code, payloadC.pos) : "";

  // The preview uses the values the page already polls; register order and precision come from
  // the device's `fields`, so it matches what the device renders byte for byte.
  let preview = null;
  if (!tplErr && live?.values) {
    const snap = {
      // A register the device holds as NaN arrives as null: present, rendered "null", as on the device.
      v: f.names.map((n) => live.values[n]), have: f.names.map((n) => n in live.values),
      smid: live.smid || "", epoch: Math.floor(Date.now() / 1000),
    };
    const items = each ? f.names.map((_, i) => i).filter((i) => itemOk(f, snap, i)).slice(0, 3) : [-1];
    preview = items.map((i) => `${render(topicC, true, f, snap, i)}\n${render(payloadC, false, f, snap, i)}`).join("\n\n");
  }

  async function save() {
    setBusy(true);
    const body = { enabled: d.enabled, host: d.host.trim(), port: d.port, client_id: d.client_id, user: d.user,
      mode: d.mode, topic: d.topic, payload: d.payload, period: d.period, qos: d.qos, retain: d.retain };
    // Left out = keep the stored one; the device never hands it back, so an empty field means "unchanged".
    if (d.password) body.password = d.password;
    if (d.clearPassword) body.password = "";
    try {
      await api.setMqtt(body);
      const cfg = await api.mqttConfig();
      setState({ cfg, draft: { ...cfg, password: "" }, err: "", saved: true });
      api.status().then((r) => r.mqtt && setSt(r.mqtt)).catch(() => {});
    } catch (e) {
      const b = e.body || {};
      setState({ ...state, err: b.field && TPL_ERR[b.error] ? tplMsg(b.field, b.error, b.pos || 0) : String(e.message) });
    } finally {
      setBusy(false);
    }
  }

  const [cls, text] = badgeOf(st);
  const regs = f.names.filter((_, i) => !f.string[i]);
  return html`
    <div class="kv" style="margin-bottom:6px">
      <b>${S.mqttState}</b><span><span class="badge ${cls}">${st?.state === "error" ? S[TPL_ERR[st.error]] || st.error : text}</span></span>
      ${st?.state !== "off" && html`<b>${S.mqttSent}</b><span class="num">${S.mqttStats(st.sent, st.dropped)}</span>`}
    </div>
    <div class="switchrow" style="padding:4px 0;margin:6px 0">
      <div><div class="t">${S.mqttEnable}</div><div class="s">${S.mqttHint}</div></div>
      <button class="switch ${d.enabled ? "on" : ""}" role="switch" aria-checked=${!!d.enabled}
        onClick=${() => set({ enabled: !d.enabled })}><span></span></button>
    </div>
    <div class="row">
      <div style="flex:3"><label>${S.mqttHost}</label>
        <input type="text" autocomplete="off" spellcheck="false" value=${d.host} onInput=${str("host")} /></div>
      <div><label>${S.mqttPort}</label>
        <input type="number" min="1" max="65535" value=${d.port} onInput=${num("port")} /></div>
    </div>
    <div class="row">
      <div><label>${S.mqttUser}</label>
        <input type="text" autocomplete="off" spellcheck="false" value=${d.user} onInput=${str("user")} /></div>
      <div><label>${S.mqttPassword}</label>
        <input type="password" autocomplete="new-password" value=${d.password} disabled=${d.clearPassword}
          placeholder=${state.cfg.password_set ? S.mqttPasswordKeep : ""} onInput=${str("password")} /></div>
    </div>
    ${state.cfg.password_set && html`
      <label style="display:flex;gap:8px;align-items:center">
        <input type="checkbox" checked=${!!d.clearPassword} onChange=${(e) => set({ clearPassword: e.target.checked, password: "" })} />
        ${S.mqttPasswordClear}</label>`}
    <div class="row">
      <div><label>${S.mqttPeriod}</label>
        <input type="number" min="5" max="3600" value=${d.period} onInput=${num("period")} /></div>
      <div><label>${S.mqttQos}</label>
        <select value=${d.qos} onChange=${(e) => set({ qos: Number(e.target.value) })}>
          <option value="0">0</option><option value="1">1</option>
        </select></div>
    </div>
    <label>${S.mqttClientId}</label>
    <input type="text" autocomplete="off" spellcheck="false" value=${d.client_id} placeholder=${state.cfg.ctx?.device || ""}
      onInput=${str("client_id")} />
    <label style="display:flex;gap:8px;align-items:center">
      <input type="checkbox" checked=${!!d.retain} onChange=${(e) => set({ retain: e.target.checked })} />
      ${S.mqttRetain}</label>

    <label>${S.mqttPreset}</label>
    <select value=${preset ? preset.id : ""} onChange=${(e) => {
      const p = PRESETS.find((x) => x.id === e.target.value);
      if (p) set({ mode: p.each ? "each" : "period", topic: p.topic, payload: p.payload });
    }}>
      ${PRESETS.map((p) => html`<option value=${p.id}>${S[PRESET_LABEL[p.id]]}</option>`)}
      ${!preset && html`<option value="">${S.mqttPresetCustom}</option>`}
    </select>
    <div class="seg" role="radiogroup">
      ${[["period", S.mqttModePeriod], ["each", S.mqttModeEach]].map(([m, label]) => html`
        <button role="radio" aria-checked=${d.mode === m} class=${d.mode === m ? "active" : ""}
          onClick=${() => set({ mode: m })}>${label}</button>`)}
    </div>
    <label>${S.mqttTopic}</label>
    <input class="tpl" type="text" autocomplete="off" spellcheck="false" value=${d.topic} onInput=${str("topic")} />
    <label>${S.mqttPayload}</label>
    <textarea class="tpl" rows="3" spellcheck="false" value=${d.payload} onInput=${str("payload")}></textarea>
    <details>
      <summary>${S.mqttKeys}</summary>
      <p class="hint" style="margin:0">
        <code>{device} {mac} {meter} {ts} {iso} {v:…} {u:…}</code>
        ${each ? html` · <code>{name} {obis} {value} {unit}</code>` : html` · <code>{values} {values_lp}</code>`}
        <br />${S.mqttKeysHint}${regs.length ? ` ${regs.join(", ")}` : ""}</p>
    </details>
    ${tplErr && html`<div class="err">${tplErr}</div>`}

    ${!tplErr && html`
      <label>${S.mqttPreview}</label>
      ${preview ? html`<div class="hex text">${preview}</div>` : html`<p class="hint" style="margin:0">${S.mqttPreviewNone}</p>`}
      <p class="hint" style="margin:6px 0 0">${S.mqttWorst(payloadC.worst, PAYLOAD_BUF - 1)} · ${S.mqttWorstTopic(topicC.worst, TOPIC_BUF - 1)}</p>`}

    ${state.err && html`<div class="err">${state.err}</div>`}
    <div class="nav">
      <button class="primary" disabled=${busy || !!tplErr} onClick=${save}>${state.saved ? S.mqttSaved : S.mqttSave}</button>
    </div>`;
}
