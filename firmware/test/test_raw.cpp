// Full pipeline replay against REAL raw (pre-decryption) HDLC/AES-GCM frames from the gPlugK.
// This is the test that actually exercises HDLC framing + AES-128-GCM decrypt with the real key,
// not just the plaintext value decode (see test_replay.cpp for that). Key is read at runtime from
// captures/gplugk.key (gitignored, never embedded in source).
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

static bool load_key(const char *path, uint8_t key[16]) {
  std::ifstream f(path);
  std::string hex;
  f >> hex;
  if (hex.size() != 32) return false;
  for (int i = 0; i < 16; i++) key[i] = (uint8_t) strtoul(hex.substr(2 * i, 2).c_str(), nullptr, 16);
  return true;
}

static double get(const std::vector<uint8_t> &buf, const char *obis, double scale) {
  uint8_t pat[8];
  size_t n = DlmsDecoder::obis_pattern(obis, pat);
  auto v = DlmsDecoder::find(buf.data(), buf.size(), pat, n);
  if (!v.found) { printf("MISSING obis %s\n", obis); fails++; return NAN; }
  return v.num / scale;
}

static bool decode_one(DlmsDecoder &d, const char *path) {
  auto f = load_hex(path);
  bool ok = false;
  for (uint8_t c : f) ok |= d.feed(c);
  return ok;
}

int main() {
  uint8_t key[16];
  if (!load_key("captures/gplugk.key", key)) {
    printf("skip: captures/gplugk.key not present\n");
    return 0;
  }

  // Frame 1: decoded ~07:21:32.619, matches SENSOR @ 07:21:41.187
  {
    DlmsDecoder d;
    d.set_key(key);
    d.set_max_frame(600);
    bool ok = decode_one(d, "captures/gplugk_raw1.hex");
    CHECK(ok);
    CHECK(d.stats.frames == 1 && d.stats.fcs_errors == 0 && d.stats.hcs_errors == 0);
    CHECK(d.stats.apdus == 1);
    CHECK(!d.key_invalid());
    CHECK(d.frame_counter() == 0x0033ccfd);
    CHECK(d.system_title()[0] == 'K' && d.system_title()[1] == 'A' && d.system_title()[2] == 'M');
    auto buf = std::vector<uint8_t>(d.plaintext(), d.plaintext() + d.plaintext_len());

    { uint8_t pat[8]; size_t n = DlmsDecoder::obis_pattern("0.0.1", pat);
      auto v = DlmsDecoder::find(buf.data(), buf.size(), pat, n);
      CHECK(v.found && v.num == 32942200); }
    CHECK(get(buf, "32.7.0", 1) == 236);
    CHECK(get(buf, "52.7.0", 1) == 235);
    CHECK(get(buf, "72.7.0", 1) == 236);
    CHECK(get(buf, "1.7.0", 1000) == 0);
    CHECK(get(buf, "1.8.0", 1000) == 55978.626);
    CHECK_NEAR(get(buf, "2.8.0", 1000), 42242.865, 0.02);
    CHECK(get(buf, "3.8.0", 1000) == 10469.437);
    CHECK_NEAR(get(buf, "4.8.0", 1000), 26107.804, 0.02);
  }

  // Frame 2: decoded ~07:21:42.550, matches SENSOR @ 07:21:51.100. Independent decoder instance
  // (no key re-use of internal state) proves each frame decrypts correctly on its own IV/counter.
  {
    DlmsDecoder d;
    d.set_key(key);
    d.set_max_frame(600);
    bool ok = decode_one(d, "captures/gplugk_raw2.hex");
    CHECK(ok && !d.key_invalid());
    CHECK(d.frame_counter() == 0x0033ccfe);   // +1 vs frame 1
    auto buf = std::vector<uint8_t>(d.plaintext(), d.plaintext() + d.plaintext_len());
    CHECK(get(buf, "32.7.0", 1) == 236);
    CHECK(get(buf, "52.7.0", 1) == 235);
    CHECK(get(buf, "72.7.0", 1) == 237);
    CHECK(get(buf, "1.8.0", 1000) == 55978.626);
    CHECK_NEAR(get(buf, "2.8.0", 1000), 42242.866, 0.02);
    CHECK_NEAR(get(buf, "4.8.0", 1000), 26107.806, 0.02);
  }

  // Frame 3: no matching MQTT payload given. Just confirm continuity and successful decode
  // (counter must keep incrementing; wrong key/IV would fail the plaintext sanity check).
  {
    DlmsDecoder d;
    d.set_key(key);
    d.set_max_frame(600);
    bool ok = decode_one(d, "captures/gplugk_raw3.hex");
    CHECK(ok && !d.key_invalid());
    CHECK(d.frame_counter() == 0x0033ccff);   // +1 vs frame 2
  }

  // Negative control: same frame 1, wrong key -> must NOT decode as valid.
  {
    DlmsDecoder d;
    uint8_t bad[16]; memcpy(bad, key, 16); bad[0] ^= 0xFF;
    d.set_key(bad);
    d.set_max_frame(600);
    bool ok = decode_one(d, "captures/gplugk_raw1.hex");
    CHECK(!ok && d.key_invalid());
  }

  printf(fails ? "%d FAILED\n" : "all ok\n", fails);
  return fails ? 1 : 0;
}
