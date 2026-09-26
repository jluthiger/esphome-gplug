// MQTT topic and payload templates: compiled once when the user saves them, filled in per publish.
//
// Consumers want different shapes (one JSON object per period for Node-RED, one topic per value for
// ioBroker, line protocol for InfluxDB), so the layout is user data like the meter profile rather
// than code. The template language is plain substitution -- `{key}` placeholders, no loops,
// conditions or expressions -- so the whole of it fits in one compile step and a render that never
// allocates. A placeholder is a `{` directly followed by a lowercase letter; every other brace is
// literal text, so a JSON payload is written as-is ({"p":{v:Pi}}) instead of doubling its braces.
//
// Everything that can fail is decided in compile(): syntax, unknown keys and register names (they
// are resolved to register indices against the current profile), keys used in the wrong mode, and
// the worst-case rendered length. That bound is exact per key -- the escaped length of the device
// name and of every register name and unit, 20 characters per number, 2 x 39 for the meter ID --
// so a template whose worst case does not fit the fixed render buffer is rejected when it is saved
// rather than truncated at 3 a.m. render() only substitutes. When the profile changes the firmware
// compiles again; a template that no longer fits or names a register that is gone pauses publishing
// instead of blocking the meter save.
//
// Escaping is one rule per context, not per key: in a payload every substituted string is JSON-
// escaped (`"` and `\` get a backslash, control bytes become `?`), in a topic `+`, `#` and control
// bytes become `_` so a register or meter ID can never turn a topic into a wildcard (the meter ID,
// which comes off the HAN line unchecked, also loses bytes >= 0x80: `_` / `?`), and line-
// protocol field keys in {values_lp} escape `,`, `=` and space. Literal text is checked instead:
// a topic literal with `+`, `#` or a control byte is rejected at compile time.
//
// spa/src/live/mqtt-template.js is a port of this file for the live preview in the setup screen;
// test/mqtt_template_vectors.tsv is run against both (test/test_mqtt_template.cpp and spa/build.mjs)
// so the preview cannot drift from what the device publishes.
//
// Header-only, no ESPHome or IDF dependency, tested in test/test_mqtt_template.cpp.
#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace gplug_mqtt {

static constexpr size_t TOPIC_TPL_MAX = 128;
static constexpr size_t PAYLOAD_TPL_MAX = 512;
static constexpr size_t MAX_TOKENS = 64;
// Render buffers, NUL included. The default payload over the largest profile (48 registers with
// 11-character names) needs ~1.8 kB, so it always compiles.
static constexpr size_t TOPIC_BUF = 256;
static constexpr size_t PAYLOAD_BUF = 2048;
static constexpr size_t NUM_MAX = 20;    // "-999999999999.999999"; larger magnitudes render null
static constexpr size_t SMID_MAX = 39;   // GplugSmi::smid_[40]
static constexpr size_t KEY_MAX = 32;    // "v:" + a 23-character OBIS string fits

enum Key : uint8_t {
  LIT, DEVICE, MAC, METER, TS, ISO, VAL, UNIT,
  VALUES, VALUES_LP,                     // period mode only
  I_NAME, I_OBIS, I_VALUE, I_UNIT,       // each mode only
};

struct Token {
  uint8_t key;
  uint8_t idx;      // register index for VAL/UNIT
  uint16_t off;     // LIT: slice of the template text, so literals are not copied
  uint16_t len;
};

struct Compiled {
  Token tok[MAX_TOKENS];
  uint8_t n{0};
  uint16_t worst{0};       // rendered length bound, NUL excluded
  bool uses_time{false};   // {ts} or {iso}: skip the publish until the clock is set
};

// The profile as the decoder holds it (desc_.obis[i]): parallel arrays like gplug_ha::HaInput.
struct Fields {
  const char *const *names;
  const char *const *obis;
  const char *const *units;
  const uint8_t *prec;
  const bool *is_string;
  uint8_t n;
  const char *device;
  const char *mac;         // 12 lowercase hex digits
};

// One copy of the live values, taken under the decoder's lock and rendered after releasing it.
struct Snapshot {
  const float *v;
  const bool *have;
  const char *smid;        // "" when the meter sent no ID
  uint32_t epoch;          // wall time; only read when Compiled::uses_time
};

struct Error {
  const char *code{nullptr};   // API error token, see firmware/README.md "MQTT"
  uint16_t pos{0};             // byte offset in the template
  uint32_t worst{0};           // tpl_overflow: the computed bound, for the error message
};

enum Esc : uint8_t { ESC_JSON, ESC_TOPIC, ESC_LP };

inline size_t esc_len_(const char *s, Esc e) {
  size_t n = 0;
  for (; *s; s++) {
    char c = *s;
    if (e == ESC_JSON && (c == '"' || c == '\\')) n++;
    else if (e == ESC_LP && (c == ',' || c == '=' || c == ' ')) n++;
    n++;
  }
  return n;
}

// Numeric register: present in {values}, iterated in each mode, allowed in {v:X}.
inline bool numeric_(const Fields &f, uint8_t i) { return !f.is_string[i]; }

inline int find_reg_(const Fields &f, const char *x, size_t xl) {
  for (uint8_t i = 0; i < f.n; i++)
    if (strlen(f.names[i]) == xl && memcmp(f.names[i], x, xl) == 0) return i;
  for (uint8_t i = 0; i < f.n; i++)
    if (strlen(f.obis[i]) == xl && memcmp(f.obis[i], x, xl) == 0) return i;
  return -1;
}

inline bool fail_(Error &err, const char *code, size_t pos) {
  err.code = code;
  err.pos = (uint16_t) pos;
  return false;
}

// Parses and checks `tpl` for one context (each: one message per value; topic: the topic rather
// than the payload) and computes the worst-case rendered length.
inline bool compile(const char *tpl, size_t len, bool each, bool topic, const Fields &f, Compiled &c,
                    Error &err) {
  c.n = 0;
  c.worst = 0;
  c.uses_time = false;
  err = Error{};
  if (len > (topic ? TOPIC_TPL_MAX : PAYLOAD_TPL_MAX)) return fail_(err, "tpl_too_long", len);
  if (len == 0) return fail_(err, "tpl_empty", 0);
  Esc esc = topic ? ESC_TOPIC : ESC_JSON;

  // Per-key bounds over the profile, computed once.
  size_t max_name = 0, max_obis = 0, max_unit = 0, values = 2, values_lp = 0;
  for (uint8_t i = 0; i < f.n; i++) {
    if (!numeric_(f, i)) continue;
    size_t nl = esc_len_(f.names[i], esc), ol = esc_len_(f.obis[i], esc), ul = esc_len_(f.units[i], esc);
    if (nl > max_name) max_name = nl;
    if (ol > max_obis) max_obis = ol;
    if (ul > max_unit) max_unit = ul;
    values += 1 + esc_len_(f.names[i], ESC_JSON) + 3 + NUM_MAX;       // ,"name":num
    values_lp += 1 + esc_len_(f.names[i], ESC_LP) + 1 + NUM_MAX;      // ,name=num
  }

  uint32_t worst = 0;
  bool has_item_topic = false;
  auto push = [&](uint8_t key, uint8_t idx, size_t off, size_t l, size_t pos) -> bool {
    // Adjacent literal characters merge into one slice of the template text.
    if (key == LIT && c.n > 0 && c.tok[c.n - 1].key == LIT && c.tok[c.n - 1].off + c.tok[c.n - 1].len == off) {
      c.tok[c.n - 1].len += (uint16_t) l;
      return true;
    }
    if (c.n >= MAX_TOKENS) return fail_(err, "tpl_too_long", pos);
    c.tok[c.n++] = Token{key, idx, (uint16_t) off, (uint16_t) l};
    return true;
  };

  size_t i = 0;
  while (i < len) {
    char ch = tpl[i];
    if (ch == '\0') return fail_(err, "tpl_syntax", i);
    if (ch != '{' || i + 1 >= len || tpl[i + 1] < 'a' || tpl[i + 1] > 'z') {
      if (topic && (ch == '+' || ch == '#' || (uint8_t) ch < 0x20)) return fail_(err, "tpl_topic", i);
      if (!push(LIT, 0, i, 1, i)) return false;
      worst += 1;
      i++;
      continue;
    }
    // A placeholder runs to the next `}`. Anything that cannot be part of a key before it (a brace,
    // a quote, a space, the end of the text) means the `}` was forgotten.
    size_t start = i, k = i + 1;
    while (k < len && tpl[k] != '}' && tpl[k] != '{' && tpl[k] != '"' && (uint8_t) tpl[k] > 0x20) k++;
    if (k >= len || tpl[k] != '}') return fail_(err, "tpl_syntax", start);
    const char *key = tpl + i + 1;
    size_t kl = k - i - 1;
    if (kl > KEY_MAX) return fail_(err, "tpl_unknown", start);
    auto is = [&](const char *s) { return strlen(s) == kl && memcmp(key, s, kl) == 0; };

    uint8_t kind, idx = 0;
    uint32_t bound;
    if (is("device")) { kind = DEVICE; bound = esc_len_(f.device, esc); }
    else if (is("mac")) { kind = MAC; bound = strlen(f.mac); }
    else if (is("meter")) { kind = METER; bound = topic ? SMID_MAX : 2 * SMID_MAX; }
    else if (is("ts")) { kind = TS; bound = 10; }
    else if (is("iso")) { kind = ISO; bound = 20; }
    else if (is("values") || is("values_lp")) {
      if (each || topic) return fail_(err, "tpl_context", start);
      kind = is("values") ? VALUES : VALUES_LP;
      bound = kind == VALUES ? values : values_lp;
    } else if (is("name") || is("obis") || is("value") || is("unit")) {
      if (!each) return fail_(err, "tpl_context", start);
      if (is("name")) { kind = I_NAME; bound = max_name; }
      else if (is("obis")) { kind = I_OBIS; bound = max_obis; }
      else if (is("value")) { kind = I_VALUE; bound = NUM_MAX; }
      else { kind = I_UNIT; bound = max_unit; }
      if (topic && (kind == I_NAME || kind == I_OBIS)) has_item_topic = true;
    } else if (kl > 2 && (key[0] == 'v' || key[0] == 'u') && key[1] == ':') {
      int r = find_reg_(f, key + 2, kl - 2);
      if (r < 0 || (key[0] == 'v' && !numeric_(f, (uint8_t) r))) return fail_(err, "tpl_unknown", start);
      idx = (uint8_t) r;
      if (key[0] == 'v') { kind = VAL; bound = NUM_MAX; }
      else { kind = UNIT; bound = esc_len_(f.units[r], esc); }
    } else {
      return fail_(err, "tpl_unknown", start);
    }
    if (kind == TS || kind == ISO) c.uses_time = true;
    if (!push(kind, idx, start, k + 1 - start, start)) return false;
    worst += bound;
    i = k + 1;
  }
  // In each mode every value would otherwise land on the same topic and overwrite the others.
  if (topic && each && !has_item_topic) return fail_(err, "tpl_topic", 0);
  size_t cap = topic ? TOPIC_BUF : PAYLOAD_BUF;
  if (worst + 1 > cap) {
    err.worst = worst;
    return fail_(err, "tpl_overflow", 0);
  }
  c.worst = (uint16_t) worst;
  return true;
}

// Bounded writer: never passes cap - 1, and remembers if it would have.
struct Out_ {
  char *p;
  size_t cap, n{0};
  bool ovf{false};
  void put(char ch) {
    if (n + 1 < cap) p[n++] = ch;
    else ovf = true;
  }
  void raw(const char *s, size_t l) { for (size_t i = 0; i < l; i++) put(s[i]); }
  // ascii: also replace bytes >= 0x80. Only for the meter ID, which comes off the HAN line
  // unchecked (DSMR copies it verbatim): a topic that is not valid UTF-8 makes an MQTT 3.1.1 broker
  // drop the connection, and it would make a JSON payload invalid. Profile names and units are
  // the user's own UTF-8 ("m³") and pass unchanged. One byte in, one byte out, so the bound holds.
  void str(const char *s, Esc e, bool ascii = false) {
    for (; *s; s++) {
      char ch = *s;
      if (ascii && (uint8_t) ch >= 0x80) { put(e == ESC_TOPIC ? '_' : '?'); continue; }
      if (e == ESC_TOPIC) { put(ch == '+' || ch == '#' || (uint8_t) ch < 0x20 ? '_' : ch); continue; }
      if ((uint8_t) ch < 0x20) { put('?'); continue; }
      if (e == ESC_JSON && (ch == '"' || ch == '\\')) put('\\');
      if (e == ESC_LP && (ch == ',' || ch == '=' || ch == ' ')) put('\\');
      put(ch);
    }
  }
  void num(float v, uint8_t prec) {
    if (std::isnan(v) || std::isinf(v) || std::fabs(v) >= 1e12f) { raw("null", 4); return; }
    char b[32];
    int l = snprintf(b, sizeof b, "%.*f", prec > 6 ? 6 : prec, (double) v);
    raw(b, l > 0 ? (size_t) l : 0);
  }
  void u32(uint32_t v) {
    char b[12];
    int l = snprintf(b, sizeof b, "%u", (unsigned) v);
    raw(b, (size_t) l);
  }
};

// Epoch seconds to YYYY-MM-DDTHH:MM:SSZ (UTC), without gmtime so host and device agree.
// Days-to-civil after Howard Hinnant's algorithm.
inline void iso_utc(uint32_t epoch, char out[21]) {
  uint32_t days = epoch / 86400, sod = epoch % 86400;
  int64_t z = (int64_t) days + 719468;
  int64_t era = z / 146097;
  uint32_t doe = (uint32_t) (z - era * 146097);
  uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int64_t y = (int64_t) yoe + era * 400;
  uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  uint32_t mp = (5 * doy + 2) / 153;
  uint32_t d = doy - (153 * mp + 2) / 5 + 1;
  uint32_t m = mp < 10 ? mp + 3 : mp - 9;
  if (m <= 2) y++;
  snprintf(out, 21, "%04d-%02u-%02uT%02u:%02u:%02uZ", (int) y, (unsigned) m, (unsigned) d,
           (unsigned) (sod / 3600), (unsigned) (sod / 60 % 60), (unsigned) (sod % 60));
}

// Whether register i produces a message in each mode.
inline bool item_ok(const Fields &f, const Snapshot &s, uint8_t i) { return numeric_(f, i) && s.have[i]; }

// Fills in a compiled template. item is the register index in each mode, -1 in period mode.
// Writes a NUL-terminated string of at most cap - 1 bytes; false only if the compile-time bound was
// wrong, which the tests rule out -- the caller drops the message rather than send a cut one.
inline bool render(const char *tpl, const Compiled &c, bool topic, const Fields &f, const Snapshot &s,
                   int item, char *out, size_t cap, size_t &len) {
  Out_ w{out, cap};
  Esc esc = topic ? ESC_TOPIC : ESC_JSON;
  for (uint8_t t = 0; t < c.n; t++) {
    const Token &k = c.tok[t];
    switch (k.key) {
      case LIT: w.raw(tpl + k.off, k.len); break;
      case DEVICE: w.str(f.device, esc); break;
      case MAC: w.raw(f.mac, strlen(f.mac)); break;
      case METER: w.str(s.smid, esc, true); break;
      case TS: w.u32(s.epoch); break;
      case ISO: { char b[21]; iso_utc(s.epoch, b); w.raw(b, 20); break; }
      case VAL:
        if (s.have[k.idx]) w.num(s.v[k.idx], f.prec[k.idx]);
        else w.raw("null", 4);
        break;
      case UNIT: w.str(f.units[k.idx], esc); break;
      case VALUES: {
        w.put('{');
        bool first = true;
        for (uint8_t i = 0; i < f.n; i++) {
          if (!item_ok(f, s, i)) continue;
          if (!first) w.put(',');
          first = false;
          w.put('"');
          w.str(f.names[i], ESC_JSON);
          w.raw("\":", 2);
          w.num(s.v[i], f.prec[i]);
        }
        w.put('}');
        break;
      }
      case VALUES_LP: {
        bool first = true;
        for (uint8_t i = 0; i < f.n; i++) {
          if (!item_ok(f, s, i)) continue;
          if (!first) w.put(',');
          first = false;
          w.str(f.names[i], ESC_LP);
          w.put('=');
          w.num(s.v[i], f.prec[i]);
        }
        break;
      }
      default:
        if (item < 0 || item >= f.n) return false;
        if (k.key == I_NAME) w.str(f.names[item], esc);
        else if (k.key == I_OBIS) w.str(f.obis[item], esc);
        else if (k.key == I_UNIT) w.str(f.units[item], esc);
        else w.num(s.v[item], f.prec[item]);
        break;
    }
  }
  if (cap > 0) out[w.n] = '\0';
  len = w.n;
  return !w.ovf && w.n <= c.worst;
}

// The presets the setup screen offers; the first is the default before anything is saved.
// spa/src/live/mqtt-template.js repeats them and the tests pin their output.
struct Preset {
  const char *id;
  bool each;
  const char *topic;
  const char *payload;
};
static constexpr Preset PRESETS[] = {
    {"json", false, "gplug/{device}/state",
     "{\"device\":\"{device}\",\"meter\":\"{meter}\",\"ts\":{ts},\"values\":{values}}"},
    {"each", true, "gplug/{device}/{name}", "{value}"},
    {"influx", false, "gplug/{device}/influx", "energy,device={device} {values_lp} {ts}000000000"},
};

}  // namespace gplug_mqtt
