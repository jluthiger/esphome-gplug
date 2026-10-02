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

// captures/ is gitignored and per-machine: a capture whose files are not here is skipped, not
// failed, so the test still checks the captures this machine does have.
static bool have(std::initializer_list<const char *> paths) {
  for (const char *p : paths)
    if (!std::ifstream(p).good()) return false;
  return true;
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
  if (!have({"captures/gplugm3_seg1.hex", "captures/gplugm3_seg2.hex", "captures/gplugm3_seg3.hex"})) {
    printf("skip capture A (captures/gplugm3_seg*.hex)\n");
  } else {
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
  if (!have({"captures/gplugm2_seg1.hex", "captures/gplugm2_seg2.hex", "captures/gplugm2_seg3.hex",
             "captures/gplugm2_seg4.hex"})) {
    printf("skip capture B (captures/gplugm2_seg*.hex)\n");
  } else {
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

  // Capture C: six consecutive push cycles of one gPlugM (L+G E450, Romande Energie) from a Tasmota
  // "weblog 4" dump, 2026-03-28. Unlike A and B this meter pushes *unencrypted*, so these are the
  // only captures that exercise the plaintext GBT path end to end (HDLC -> LLC -> GBT reassembly ->
  // capture-list decode) instead of starting at a decrypted APDU. The meter alternates two push
  // shapes: five totals pushes of four frames each, then one tariff-register push of two frames,
  // which is why no single cycle carries every register.
  //
  // Expected numbers are what the device's own GEAG decoder logged for the same bytes, in the
  // meter's raw units (W, Wh, varh, 10 mA). Scaling is the preset's business, not the decoder's,
  // and is deliberately not asserted here.
  if (!have({"captures/gplugm4_c1.hex", "captures/gplugm4_c6.hex"})) {
    printf("skip capture C (captures/gplugm4_c*.hex)\n");
  } else {
    struct Field { const char *obis; double value; };
    struct Cycle { int n; Field f[12]; };
    static const Cycle cycles[] = {
      {1, {{"1.7.0", 0}, {"2.7.0", 8198}, {"31.7.0", 1212}, {"51.7.0", 1105}, {"71.7.0", 1159}, {"130.7.0", 579}, {"1.8.0", 31535634}, {"2.8.0", 7049818}, {"5.8.0", 7058667}, {"6.8.0", 310355}, {"7.8.0", 3188222}, {"8.8.0", 13716054}}},
      {2, {{"1.7.0", 0}, {"2.7.0", 7905}, {"31.7.0", 1177}, {"51.7.0", 1071}, {"71.7.0", 1127}, {"130.7.0", 585}, {"1.8.0", 31535634}, {"2.8.0", 7049840}, {"5.8.0", 7058667}, {"6.8.0", 310355}, {"7.8.0", 3188223}, {"8.8.0", 13716054}}},
      {3, {{"1.7.0", 0}, {"2.7.0", 7740}, {"31.7.0", 1154}, {"51.7.0", 1047}, {"71.7.0", 1103}, {"130.7.0", 584}, {"1.8.0", 31535634}, {"2.8.0", 7049864}, {"5.8.0", 7058667}, {"6.8.0", 310355}, {"7.8.0", 3188224}, {"8.8.0", 13716054}}},
      {4, {{"1.7.0", 0}, {"2.7.0", 5853}, {"31.7.0", 901}, {"51.7.0", 799}, {"71.7.0", 849}, {"130.7.0", 573}, {"1.8.0", 31535634}, {"2.8.0", 7049881}, {"5.8.0", 7058667}, {"6.8.0", 310355}, {"7.8.0", 3188225}, {"8.8.0", 13716054}}},
      {5, {{"1.7.0", 0}, {"2.7.0", 5553}, {"31.7.0", 522}, {"51.7.0", 957}, {"71.7.0", 902}, {"130.7.0", 509}, {"1.8.0", 31535634}, {"2.8.0", 7049900}, {"5.8.0", 7058667}, {"6.8.0", 310355}, {"7.8.0", 3188226}, {"8.8.0", 13716054}}},
      {6, {{"1.8.1", 12480690}, {"1.8.2", 19054944}, {"2.8.1", 5191543}, {"2.8.2", 1858374}}},
    };
    for (const Cycle &c : cycles) {
      char path[64];
      snprintf(path, sizeof path, "captures/gplugm4_c%d.hex", c.n);
      DlmsDecoder d;
      d.set_max_frame(600);
      bool ok = false;
      for (uint8_t b : load_hex(path)) ok |= d.feed(b);
      CHECK(ok);
      if (!ok) continue;
      // Every frame of every cycle passes both checksums -- the dump is byte-exact, and the
      // 2-byte-tagged frames the log prints need no fixup on the way in.
      CHECK(d.stats.fcs_errors == 0);
      CHECK(d.stats.hcs_errors == 0);
      auto buf = std::vector<uint8_t>(d.plaintext(), d.plaintext() + d.plaintext_len());
      for (const Field &f : c.f) {
        if (!f.obis) break;
        CHECK(get(buf, f.obis) == f.value);
      }
    }

    // SM-ID travels in the totals push only, as an 8-byte ASCII octet string.
    DlmsDecoder d;
    d.set_max_frame(600);
    bool ok = false;
    for (uint8_t b : load_hex("captures/gplugm4_c1.hex")) ok |= d.feed(b);
    CHECK(ok);
    uint8_t pat[8]; size_t n = DlmsDecoder::obis_pattern("96.1.0", pat);
    auto v = DlmsDecoder::find(d.plaintext(), d.plaintext_len(), pat, n);
    CHECK(v.found && v.is_string && v.str_len == 8 && !memcmp(v.str, "56449395", 8));
  }

  printf(fails ? "%d FAILED\n" : "all ok\n", fails);
  return fails ? 1 : 0;
}
