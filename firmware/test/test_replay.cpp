// Replay test against a REAL Kamstrup gPlugK capture (already-decrypted DLMS plaintext, captured via
// Tasmota `sensor53 d1`). No key involved: this validates obis_pattern()/find() and COSEM value decode
// against real device bytes, cross-checked against the matching MQTT tele/SENSOR payload.
// See captures/gplugk_plaintext.hex for provenance and the expected values.
#include "dlms_decoder.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>
using namespace gplug_dlms;

int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
#define CHECK_NEAR(a, b, eps) do { double _a=(a),_b=(b); if (!(_a > _b-(eps) && _a < _b+(eps))) { \
  printf("FAIL %s:%d %s (%g) not near %s (%g)\n", __FILE__, __LINE__, #a, _a, #b, _b); fails++; } } while (0)

static std::vector<uint8_t> load_hex(const char *path) {
  std::ifstream f(path);
  std::vector<uint8_t> out;
  std::string line;
  while (std::getline(f, line)) {
    if (!line.empty() && line[0] == '#') continue;
    std::istringstream ls(line);
    std::string tok;
    while (ls >> tok) out.push_back((uint8_t) strtoul(tok.c_str(), nullptr, 16));
  }
  return out;
}

static double get(const std::vector<uint8_t> &buf, const char *obis, double scale) {
  uint8_t pat[8];
  size_t n = DlmsDecoder::obis_pattern(obis, pat);
  auto v = DlmsDecoder::find(buf.data(), buf.size(), pat, n);
  if (!v.found) { printf("MISSING obis %s\n", obis); fails++; return NAN; }
  return v.num / scale;
}

int main() {
  auto buf = load_hex("captures/gplugk_plaintext.hex");
  CHECK(buf.size() > 300);   // sanity: file loaded

  // SM-ID: numeric OBIS 0.0.1, must equal the meter serial from the matching MQTT payload exactly.
  { uint8_t pat[8]; size_t n = DlmsDecoder::obis_pattern("0.0.1", pat);
    auto v = DlmsDecoder::find(buf.data(), buf.size(), pat, n);
    CHECK(v.found && !v.is_string && v.num == 32942200); }

  // Voltages: essentially static within 100 ms, must match exactly.
  CHECK(get(buf, "32.7.0", 1) == 236);   // V1
  CHECK(get(buf, "52.7.0", 1) == 235);   // V2
  CHECK(get(buf, "72.7.0", 1) == 236);   // V3

  // Import power was 0 at capture time.
  CHECK(get(buf, "1.7.0", 1000) == 0);   // Pi

  // Energy counters: monotonic, published ~90 ms after this capture -> exact or single-digit-Wh drift.
  CHECK(get(buf, "1.8.0", 1000) == 55978.625);          // Ei: exact
  CHECK_NEAR(get(buf, "2.8.0", 1000), 42242.794, 0.05);  // Eo: +6 Wh drift
  CHECK(get(buf, "3.8.0", 1000) == 10469.437);           // rEi: exact
  CHECK_NEAR(get(buf, "4.8.0", 1000), 26107.675, 0.05);  // rEo: +8 Wh drift

  // Fast-changing values (export power, currents): real instantaneous drift expected over the gap
  // between capture and MQTT publish. Just confirm they decode to a plausible range, don't pin exactly.
  double po = get(buf, "2.7.0", 1000);
  CHECK(po > 0 && po < 2.0);
  double i1 = get(buf, "31.7.0", 100);
  CHECK(i1 > 0 && i1 < 10.0);

  printf(fails ? "%d FAILED\n" : "all ok\n", fails);
  return fails ? 1 : 0;
}
