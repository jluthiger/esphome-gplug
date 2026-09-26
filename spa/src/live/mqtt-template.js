// Port of firmware/components/gplug_smi/mqtt_template.h, for the MQTT card's live preview and its
// error line. The device is the authority -- it compiles again on save and its answer is what
// counts -- but a preview that disagrees with what gets published is worse than none, so this
// follows the C++ step for step: the same keys, error codes, byte positions and worst-case rule.
// Both run against firmware/test/mqtt_template_vectors.tsv (the host test and build.mjs).
//
// Works on UTF-8 bytes like the firmware, so positions and lengths are byte counts: a "³" in a unit
// counts two, as it does in the render buffer. Also imported by mock/server.mjs to validate a POST.

export const TOPIC_TPL_MAX = 128;
export const PAYLOAD_TPL_MAX = 512;
export const MAX_TOKENS = 64;
export const TOPIC_BUF = 256;
export const PAYLOAD_BUF = 2048;
const NUM_MAX = 20;
const SMID_MAX = 39;
const KEY_MAX = 32;

export const PRESETS = [
  { id: "json", each: false, topic: "gplug/{device}/state",
    payload: '{"device":"{device}","meter":"{meter}","ts":{ts},"values":{values}}' },
  { id: "each", each: true, topic: "gplug/{device}/{name}", payload: "{value}" },
  { id: "influx", each: false, topic: "gplug/{device}/influx",
    payload: "energy,device={device} {values_lp} {ts}000000000" },
];

const enc = new TextEncoder();
const dec = new TextDecoder();
const bytes = (s) => enc.encode(s || "");
const QUOTE = 34, BSL = 92, COMMA = 44, EQ = 61, SPACE = 32, PLUS = 43, HASH = 35, LBR = 123, RBR = 125;

function escLen(b, e) {
  let n = 0;
  for (const c of b) {
    if (e === "json" && (c === QUOTE || c === BSL)) n++;
    else if (e === "lp" && (c === COMMA || c === EQ || c === SPACE)) n++;
    n++;
  }
  return n;
}

// f: {names, obis, units, prec, string (bool[]), device, mac}, as GET /api/config/mqtt returns them.
function findReg(f, x) {
  let i = f.names.indexOf(x);
  if (i < 0) i = f.obis.indexOf(x);
  return i;
}

export function compile(text, each, topic, f) {
  const t = bytes(text);
  const len = t.length;
  const fail = (code, pos, worst = 0) => ({ ok: false, code, pos, worst });
  if (len > (topic ? TOPIC_TPL_MAX : PAYLOAD_TPL_MAX)) return fail("tpl_too_long", len);
  if (len === 0) return fail("tpl_empty", 0);
  const esc = topic ? "topic" : "json";
  let maxName = 0, maxObis = 0, maxUnit = 0, values = 2, valuesLp = 0;
  f.names.forEach((name, i) => {
    if (f.string[i]) return;
    maxName = Math.max(maxName, escLen(bytes(name), esc));
    maxObis = Math.max(maxObis, escLen(bytes(f.obis[i]), esc));
    maxUnit = Math.max(maxUnit, escLen(bytes(f.units[i]), esc));
    values += 1 + escLen(bytes(name), "json") + 3 + NUM_MAX;
    valuesLp += 1 + escLen(bytes(name), "lp") + 1 + NUM_MAX;
  });
  const tok = [];
  let worst = 0, usesTime = false, itemTopic = false;
  const push = (key, idx, off, l) => {
    const last = tok[tok.length - 1];
    if (key === "lit" && last && last.key === "lit" && last.off + last.len === off) { last.len += l; return true; }
    if (tok.length >= MAX_TOKENS) return false;
    tok.push({ key, idx, off, len: l });
    return true;
  };
  let i = 0;
  while (i < len) {
    const ch = t[i];
    if (ch === 0) return fail("tpl_syntax", i);
    if (ch !== LBR || i + 1 >= len || t[i + 1] < 97 || t[i + 1] > 122) {
      if (topic && (ch === PLUS || ch === HASH || ch < 0x20)) return fail("tpl_topic", i);
      if (!push("lit", 0, i, 1)) return fail("tpl_too_long", i);
      worst += 1;
      i++;
      continue;
    }
    const start = i;
    let k = i + 1;
    while (k < len && t[k] !== RBR && t[k] !== LBR && t[k] !== QUOTE && t[k] > 0x20) k++;
    if (k >= len || t[k] !== RBR) return fail("tpl_syntax", start);
    const kl = k - i - 1;
    if (kl > KEY_MAX) return fail("tpl_unknown", start);
    const key = dec.decode(t.subarray(i + 1, k));
    let kind, idx = 0, bound;
    if (key === "device") { kind = key; bound = escLen(bytes(f.device), esc); }
    else if (key === "mac") { kind = key; bound = bytes(f.mac).length; }
    else if (key === "meter") { kind = key; bound = topic ? SMID_MAX : 2 * SMID_MAX; }
    else if (key === "ts") { kind = key; bound = 10; }
    else if (key === "iso") { kind = key; bound = 20; }
    else if (key === "values" || key === "values_lp") {
      if (each || topic) return fail("tpl_context", start);
      kind = key;
      bound = key === "values" ? values : valuesLp;
    } else if (key === "name" || key === "obis" || key === "value" || key === "unit") {
      if (!each) return fail("tpl_context", start);
      kind = "i_" + key;
      bound = { name: maxName, obis: maxObis, value: NUM_MAX, unit: maxUnit }[key];
      if (topic && (key === "name" || key === "obis")) itemTopic = true;
    } else if (kl > 2 && (key[0] === "v" || key[0] === "u") && key[1] === ":") {
      const r = findReg(f, key.slice(2));
      if (r < 0 || (key[0] === "v" && f.string[r])) return fail("tpl_unknown", start);
      idx = r;
      if (key[0] === "v") { kind = "val"; bound = NUM_MAX; }
      else { kind = "unit"; bound = escLen(bytes(f.units[r]), esc); }
    } else {
      return fail("tpl_unknown", start);
    }
    if (kind === "ts" || kind === "iso") usesTime = true;
    if (!push(kind, idx, start, k + 1 - start)) return fail("tpl_too_long", start);
    worst += bound;
    i = k + 1;
  }
  if (topic && each && !itemTopic) return fail("tpl_topic", 0);
  if (worst + 1 > (topic ? TOPIC_BUF : PAYLOAD_BUF)) return fail("tpl_overflow", 0, worst);
  return { ok: true, tok, worst, usesTime, text: t };
}

function escStr(s, e) {
  let o = "";
  for (const ch of s) {
    const c = ch.codePointAt(0);
    if (e === "topic") { o += c === PLUS || c === HASH || c < 0x20 ? "_" : ch; continue; }
    if (c < 0x20) { o += "?"; continue; }
    if (e === "json" && (c === QUOTE || c === BSL)) o += "\\";
    if (e === "lp" && (c === COMMA || c === EQ || c === SPACE)) o += "\\";
    o += ch;
  }
  return o;
}

// The meter ID comes off the HAN line unchecked; like the firmware, every byte >= 0x80 of its UTF-8
// form becomes one `_` (topic) or `?` (payload), then the usual escaping applies.
function escMeter(s, e) {
  let a = "";
  for (const c of bytes(s)) a += c >= 0x80 ? (e === "topic" ? "_" : "?") : String.fromCharCode(c);
  return escStr(a, e);
}

function num(v, prec) {
  if (typeof v !== "number" || !isFinite(v) || Math.abs(v) >= 1e12) return "null";
  return v.toFixed(Math.min(prec || 0, 6));
}

export function iso(epoch) {
  return new Date(epoch * 1000).toISOString().slice(0, 19) + "Z";
}

// Whether register i produces a message in each mode.
export const itemOk = (f, s, i) => !f.string[i] && !!s.have[i];

// s: {v: number[], have: bool[], smid, epoch}. item: register index in each mode, -1 otherwise.
export function render(c, topic, f, s, item) {
  const esc = topic ? "topic" : "json";
  let o = "";
  const list = (lp) => f.names.map((n, i) => itemOk(f, s, i)
    ? (lp ? `${escStr(n, "lp")}=` : `"${escStr(n, "json")}":`) + num(s.v[i], f.prec[i]) : null).filter((x) => x !== null).join(",");
  for (const k of c.tok) {
    switch (k.key) {
      case "lit": o += dec.decode(c.text.subarray(k.off, k.off + k.len)); break;
      case "device": o += escStr(f.device, esc); break;
      case "mac": o += f.mac; break;
      case "meter": o += escMeter(s.smid || "", esc); break;
      case "ts": o += String(s.epoch >>> 0); break;
      case "iso": o += iso(s.epoch); break;
      case "val": o += s.have[k.idx] ? num(s.v[k.idx], f.prec[k.idx]) : "null"; break;
      case "unit": o += escStr(f.units[k.idx], esc); break;
      case "values": o += "{" + list(false) + "}"; break;
      case "values_lp": o += list(true); break;
      case "i_name": o += escStr(f.names[item], esc); break;
      case "i_obis": o += escStr(f.obis[item], esc); break;
      case "i_unit": o += escStr(f.units[item], esc); break;
      case "i_value": o += num(s.v[item], f.prec[item]); break;
    }
  }
  return o;
}

// GET /api/config/mqtt's `fields` and `ctx` in the shape compile() and render() take.
export function toFields(cfg) {
  const fl = cfg.fields || [];
  return {
    names: fl.map((x) => x.name), obis: fl.map((x) => x.obis), units: fl.map((x) => x.unit || ""),
    prec: fl.map((x) => x.prec || 0), string: fl.map((x) => !!x.string),
    device: (cfg.ctx && cfg.ctx.device) || "", mac: (cfg.ctx && cfg.ctx.mac) || "",
  };
}
