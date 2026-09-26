// mqtt_template.h: what a user's template compiles to and what the device then publishes. The
// promise that matters is the worst-case bound -- a template accepted at save time must never
// overflow the render buffer later, whatever the meter sends -- so besides the shared vectors
// (mqtt_template_vectors.tsv, also run by the SPA build) this checks the bound against hostile
// inputs and the largest profile the firmware allows.
#include "mqtt_template.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
using namespace gplug_mqtt;

int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

// The fixture described in mqtt_template_vectors.tsv.
static const char *NAMES[] = {"Ei", "Eo", "Pi", "Po", "SMid", "U1"};
static const char *OBIS[] = {"1.8.0", "2.8.0", "1.7.0", "2.7.0", "96.1.0", "32.7.0"};
static const char *UNITS[] = {"kWh", "kWh", "kW", "kW", "", "V"};
static const uint8_t PREC[] = {3, 3, 3, 3, 0, 1};
static const bool STR[] = {false, false, false, false, true, false};
static const float VALS[] = {1234.5f, 0.25f, 1.5f, 0.0f, 0.0f, 0.0f};
static const bool HAVE[] = {true, true, true, true, false, false};
static const Fields FX{NAMES, OBIS, UNITS, PREC, STR, 6, "gplug-a1b2c3", "a1b2c3d4e5f6"};
static const Snapshot SX{VALS, HAVE, "1234\"5\\6", 1790000000u};

struct Result {
  bool ok;
  std::string out;   // rendered text, or "code:pos"
};

static Result run(const char *tpl, bool each, bool topic, const Fields &f = FX, const Snapshot &s = SX,
                  int item = 0) {
  Compiled c;
  Error e;
  if (!compile(tpl, strlen(tpl), each, topic, f, c, e)) return {false, std::string(e.code) + ":" + std::to_string(e.pos)};
  char buf[PAYLOAD_BUF];
  size_t len = 0;
  bool ok = render(tpl, c, topic, f, s, each ? item : -1, buf, topic ? TOPIC_BUF : PAYLOAD_BUF, len);
  if (!ok) return {false, "render"};
  return {true, std::string(buf, len)};
}

static std::string unescape(const std::string &s) {
  std::string o;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '\\' && i + 1 < s.size() && (s[i + 1] == 't' || s[i + 1] == 'n')) {
      o += s[i + 1] == 't' ? '\t' : '\n';
      i++;
    } else {
      o += s[i];
    }
  }
  return o;
}

static void vectors() {
  FILE *fp = fopen("mqtt_template_vectors.tsv", "r");
  CHECK(fp != nullptr);
  if (!fp) return;
  char line[2048];
  int rows = 0, lineno = 0;
  while (fgets(line, sizeof line, fp)) {
    lineno++;
    std::string l(line);
    while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
    if (l.empty() || l[0] == '#') continue;
    std::vector<std::string> col;
    size_t a = 0;
    for (;;) {
      size_t b = l.find('\t', a);
      col.push_back(l.substr(a, b == std::string::npos ? std::string::npos : b - a));
      if (b == std::string::npos) break;
      a = b + 1;
    }
    if (col.size() != 4) { printf("FAIL vectors line %d: %zu columns\n", lineno, col.size()); fails++; continue; }
    std::string tpl = unescape(col[2]), want = unescape(col[3]);
    Result r = run(tpl.c_str(), col[0] == "each", col[1] == "topic");
    std::string got = r.ok ? "ok:" + r.out : "err:" + r.out;
    if (got != want) {
      printf("FAIL vectors line %d: %s\n  want %s\n  got  %s\n", lineno, col[2].c_str(), want.c_str(), got.c_str());
      fails++;
    }
    rows++;
  }
  fclose(fp);
  CHECK(rows > 40);
}

// A profile of n registers with 11-character names, the most the descriptor stores.
struct Big {
  char names[48][12], obis[48][24], units[48][8];
  const char *np[48], *op[48], *up[48];
  uint8_t prec[48];
  bool str[48], have[48];
  float v[48];
  Fields f;
  Big(uint8_t n, char fill, float val) {
    for (uint8_t i = 0; i < n; i++) {
      snprintf(names[i], sizeof names[i], "%c%c%09u", fill, fill, i);   // 11 chars
      snprintf(obis[i], sizeof obis[i], "1-0:%u.8.0*255", i);
      snprintf(units[i], sizeof units[i], "%c%c%c%c%c%c%c", fill, fill, fill, fill, fill, fill, fill);
      np[i] = names[i]; op[i] = obis[i]; up[i] = units[i];
      prec[i] = 6; str[i] = false; have[i] = true; v[i] = val;
    }
    f = Fields{np, op, up, prec, str, n, "gplug-\"\\\"\\\"\\", "a1b2c3d4e5f6"};
  }
};

int main() {
  // --- 1. the shared vectors ---
  vectors();

  // --- 2. length limits: exactly at the limit is fine, one byte over is not ---
  {
    std::string t(TOPIC_TPL_MAX, 'a'), p(PAYLOAD_TPL_MAX, 'a');
    CHECK(run(t.c_str(), false, true).ok);
    CHECK(run((t + "a").c_str(), false, true).out == "tpl_too_long:129");
    CHECK(run(p.c_str(), false, false).ok);
    CHECK(run((p + "a").c_str(), false, false).out == "tpl_too_long:513");
    // Token count, not only bytes: 65 placeholders separated by literals.
    std::string many;
    for (int i = 0; i < 40; i++) many += "{ts},";
    Result r = run(many.c_str(), false, false);
    CHECK(!r.ok && r.out.rfind("tpl_too_long:", 0) == 0);
  }

  // --- 3. name before OBIS: a register named like another's OBIS code wins ---
  {
    const char *names[] = {"A", "1.8.0"};
    const char *obis[] = {"1.8.0", "9.9.9"};
    const char *units[] = {"a", "b"};
    const uint8_t prec[] = {0, 0};
    const bool str[] = {false, false}, have[] = {true, true};
    const float v[] = {1.0f, 2.0f};
    Fields f{names, obis, units, prec, str, 2, "d", "a1b2c3d4e5f6"};
    Snapshot s{v, have, "", 0};
    CHECK(run("{v:1.8.0}", false, false, f, s).out == "2");
    CHECK(run("{v:A}", false, false, f, s).out == "1");
  }

  // --- 4. topic escaping: a register name or meter ID cannot make a wildcard ---
  {
    const char *names[] = {"a+b#c"};
    const char *obis[] = {"1.8.0"};
    const char *units[] = {"x"};
    const uint8_t prec[] = {0};
    const bool str[] = {false}, have[] = {true};
    const float v[] = {1.0f};
    Fields f{names, obis, units, prec, str, 1, "d", "a1b2c3d4e5f6"};
    Snapshot s{v, have, "m+#\x01", 0};
    CHECK(run("t/{name}", true, true, f, s).out == "t/a_b_c");
    CHECK(run("t/{meter}", false, true, f, s).out == "t/m___");
    CHECK(run("{meter}", false, false, f, s).out == "m+#?");
    CHECK(run("{values_lp}", false, false, f, s).out == "a+b#c=1");
    // A meter ID that is not ASCII (garbage on a DSMR line) cannot produce invalid UTF-8.
    Snapshot s8{v, have, "a\xc3\xa9\xff", 0};
    CHECK(run("t/{meter}", false, true, f, s8).out == "t/a___");
    CHECK(run("{meter}", false, false, f, s8).out == "a???");
  }

  // --- 5. line-protocol field keys escape , = and space ---
  {
    const char *names[] = {"a,b=c d"};
    const char *obis[] = {"1.8.0"};
    const char *units[] = {""};
    const uint8_t prec[] = {1};
    const bool str[] = {false}, have[] = {true};
    const float v[] = {2.5f};
    Fields f{names, obis, units, prec, str, 1, "d", "a1b2c3d4e5f6"};
    Snapshot s{v, have, "", 0};
    CHECK(run("{values_lp}", false, false, f, s).out == "a\\,b\\=c\\ d=2.5");
    CHECK(run("{values}", false, false, f, s).out == "{\"a,b=c d\":2.5}");
  }

  // --- 6. numbers: precision, negatives, and null for what cannot be a JSON number ---
  {
    const char *names[] = {"a", "b", "c", "d", "e", "f"};
    const char *obis[] = {"1", "2", "3", "4", "5", "6"};
    const char *units[] = {"", "", "", "", "", ""};
    const uint8_t prec[] = {0, 2, 9, 3, 3, 3};
    const bool str[] = {false, false, false, false, false, false}, have[] = {true, true, true, true, true, true};
    const float v[] = {2.0f, -1.25f, 0.5f, NAN, INFINITY, 1e12f};
    Fields f{names, obis, units, prec, str, 6, "d", "a1b2c3d4e5f6"};
    Snapshot s{v, have, "", 0};
    CHECK(run("{v:a}", false, false, f, s).out == "2");
    CHECK(run("{v:b}", false, false, f, s).out == "-1.25");
    CHECK(run("{v:c}", false, false, f, s).out == "0.500000");   // precision clamps to 6
    CHECK(run("{v:d}|{v:e}|{v:f}", false, false, f, s).out == "null|null|null");
    const float w[] = {-999999999999.0f, 0, 0, 0, 0, 0};
    const uint8_t p6[] = {6, 0, 0, 0, 0, 0};
    Fields f6{names, obis, units, p6, str, 1, "d", "a1b2c3d4e5f6"};
    Snapshot s6{w, have, "", 0};
    Result r = run("{v:a}", false, false, f6, s6);
    CHECK(r.ok && r.out.size() <= NUM_MAX);
  }

  // --- 7. missing values: {values} leaves them out, and can end up empty ---
  {
    const bool none[] = {false, false, false, false, false, false};
    Snapshot s{VALS, none, "", 0};
    CHECK(run("{values}", false, false, FX, s).out == "{}");
    CHECK(run("{values_lp}", false, false, FX, s).out == "");
    CHECK(run("{v:Ei}", false, false, FX, s).out == "null");
    CHECK(!item_ok(FX, s, 0));
    CHECK(!item_ok(FX, SX, 4));   // string register
    CHECK(item_ok(FX, SX, 0));
  }

  // --- 8. uses_time and the date conversion around leap days ---
  {
    Compiled c;
    Error e;
    CHECK(compile("{iso}", 5, false, false, FX, c, e) && c.uses_time);
    CHECK(compile("{values}", 8, false, false, FX, c, e) && !c.uses_time);
    char b[21];
    iso_utc(0, b); CHECK(strcmp(b, "1970-01-01T00:00:00Z") == 0);
    iso_utc(951782400, b); CHECK(strcmp(b, "2000-02-29T00:00:00Z") == 0);
    iso_utc(1709251199, b); CHECK(strcmp(b, "2024-02-29T23:59:59Z") == 0);
    iso_utc(4102444800u, b); CHECK(strcmp(b, "2100-01-01T00:00:00Z") == 0);
  }

  // --- 9. the bound: the default template fits the largest profile, the worst case is honoured ---
  {
    Big big(48, 'x', -123456789012.0f);   // just under 1e12, the widest number that is not null
    Compiled c;
    Error e;
    const Preset &json = PRESETS[0];
    CHECK(compile(json.payload, strlen(json.payload), false, false, big.f, c, e));
    char buf[PAYLOAD_BUF];
    size_t len = 0;
    const char *smid = "\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"";   // 39 escapable chars
    CHECK(strlen(smid) == SMID_MAX);
    Snapshot s{big.v, big.have, smid, 4294967295u};
    CHECK(render(json.payload, c, false, big.f, s, -1, buf, sizeof buf, len));
    CHECK(len <= c.worst && c.worst < PAYLOAD_BUF);
    // Every key in one payload and one topic, rendered against hostile strings.
    const char *all = "{device}{mac}{meter}{ts}{iso}{v:xx000000047}{u:xx000000000}{values}";
    CHECK(compile(all, strlen(all), false, false, big.f, c, e));
    CHECK(render(all, c, false, big.f, s, -1, buf, sizeof buf, len) && len <= c.worst);
    const char *each_p = "{device}{mac}{meter}{ts}{iso}{name}{obis}{value}{unit}";
    CHECK(compile(each_p, strlen(each_p), true, false, big.f, c, e));
    for (uint8_t i = 0; i < 48; i++) CHECK(render(each_p, c, false, big.f, s, i, buf, sizeof buf, len) && len <= c.worst);
    const char *each_t = "{device}/{mac}/{meter}/{name}/{obis}/{unit}/{value}";
    CHECK(compile(each_t, strlen(each_t), true, true, big.f, c, e));
    for (uint8_t i = 0; i < 48; i++) CHECK(render(each_t, c, true, big.f, s, i, buf, TOPIC_BUF, len) && len <= c.worst);
  }

  // --- 10. overflow is refused at compile time, with the bound reported ---
  {
    Big big(48, 'x', 0);
    Compiled c;
    Error e;
    const char *two = "{values}{values}";
    CHECK(!compile(two, strlen(two), false, false, big.f, c, e));
    CHECK(strcmp(e.code, "tpl_overflow") == 0 && e.worst >= PAYLOAD_BUF);
    // Big literal plus {values} crosses 2048 even though each alone fits.
    std::string p(400, 'a');
    p += "{values}";
    const char *pl = p.c_str();
    CHECK(!compile(pl, p.size(), false, false, big.f, c, e) && strcmp(e.code, "tpl_overflow") == 0);
    Big small(8, 'x', 0);
    CHECK(compile(pl, p.size(), false, false, small.f, c, e));
    // The same topic that fits one device name overflows a (hypothetical) huge one.
    std::string dev(300, 'd');
    Fields f = FX;
    f.device = dev.c_str();
    CHECK(!compile("gplug/{device}", 14, false, true, f, c, e) && strcmp(e.code, "tpl_overflow") == 0);
  }

  // --- 11. every preset compiles in its own mode against the fixture ---
  for (const Preset &p : PRESETS) {
    Compiled c;
    Error e;
    CHECK(compile(p.topic, strlen(p.topic), p.each, true, FX, c, e));
    CHECK(compile(p.payload, strlen(p.payload), p.each, false, FX, c, e));
  }

  printf("%s (%d failures)\n", fails ? "FAILED" : "all passed", fails);
  return fails ? 1 : 0;
}
