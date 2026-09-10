// Validates find_capture_list() (the gPlugM/L+G capture-list decoder) against two real captures
// from different push-message shapes (14-element and 18-element descriptor arrays), cross-checked
// against the device's own "GEAG pattern matched at pos N: ... value: V" debug log and, for the
// 14-element capture, the corresponding MQTT tele/.../SENSOR payload.
#include "dlms_decoder.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>
using namespace gplug_dlms;

int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

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

static double get(const std::vector<uint8_t> &buf, const char *obis) {
  uint8_t pat[8]; size_t n = DlmsDecoder::obis_pattern(obis, pat);
  auto v = DlmsDecoder::find(buf.data(), buf.size(), pat, n);
  if (!v.found) { printf("MISSING obis %s\n", obis); fails++; return -999999; }
  return v.num;
}

int main() {
  // Capture A: 14-element descriptor array, three real HDLC segments, GEAG-confirmed values,
  // matching MQTT tele/gPlugM_18A254/SENSOR @ 23:01:34.603 (Ei/Eo exact match).
  {
    DlmsDecoder d;
    d.set_max_frame(700);
    bool ok = false;
    for (const char *f : {"captures/gplugm3_seg1.hex", "captures/gplugm3_seg2.hex", "captures/gplugm3_seg3.hex"})
      for (uint8_t c : load_hex(f)) ok |= d.feed(c);
    CHECK(ok);
    auto buf = std::vector<uint8_t>(d.plaintext(), d.plaintext() + d.plaintext_len());

    CHECK(get(buf, "1.7.0") == 505);      // Pi
    CHECK(get(buf, "2.7.0") == 0);        // Po
    CHECK(get(buf, "31.7.0") == 77);      // I1
    CHECK(get(buf, "51.7.0") == 99);      // I2
    CHECK(get(buf, "71.7.0") == 233);     // I3
    CHECK(get(buf, "1.8.0") == 19806822); // Ei
    CHECK(get(buf, "2.8.0") == 735969);   // Eo
    CHECK(get(buf, "5.8.0") == 2124);     // Q5
    CHECK(get(buf, "6.8.0") == 0);        // Q6
    CHECK(get(buf, "7.8.0") == 1262116);  // Q7
    CHECK(get(buf, "8.8.0") == 17015612); // Q8

    // SM-ID: 8-byte ASCII octet string, second distinct descriptor (rank 1).
    uint8_t pat[8]; size_t n = DlmsDecoder::obis_pattern("96.1.0", pat);
    auto v = DlmsDecoder::find(buf.data(), buf.size(), pat, n);
    CHECK(v.found && v.is_string && v.str_len == 8 && !memcmp(v.str, "59268755", 8));
  }

  // Capture B: 18-element descriptor array (4 sub-indices each of 18.1.0 and 18.2.1, each OBIS
  // repeated twice with a different attribute-id). User-confirmed: 18.2.1 = 431192 (a water meter
  // reading, "Wasser":431.192 in the matching SENSOR payload from the other capture).
  {
    DlmsDecoder d;
    d.set_max_frame(700);
    bool ok = false;
    for (const char *f : {"captures/gplugm2_seg1.hex", "captures/gplugm2_seg2.hex",
                           "captures/gplugm2_seg3.hex", "captures/gplugm2_seg4.hex"})
      for (uint8_t c : load_hex(f)) ok |= d.feed(c);
    CHECK(ok);
    auto buf = std::vector<uint8_t>(d.plaintext(), d.plaintext() + d.plaintext_len());
    CHECK(get(buf, "24.2.1") == 431192);
  }

  printf(fails ? "%d FAILED\n" : "all ok\n", fails);
  return fails ? 1 : 0;
}
