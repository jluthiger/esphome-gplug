// Validates decode_structure() -- the algorithm that actually ships in gplug_smi.cpp (matching
// esphome-gplugk's decode_cosem_) -- against the same real gPlugK captures test_raw.cpp already
// proved decrypt correctly. test_raw.cpp/test_replay.cpp still exercise find() (kept as a
// documented fallback for buffers decode_structure() doesn't recognise); this is the test that
// matches production.
#include "dlms_decoder.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
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

static bool load_key(const char *path, uint8_t key[16]) {
  std::ifstream f(path);
  std::string hex;
  f >> hex;
  if (hex.size() != 32) return false;
  for (int i = 0; i < 16; i++) key[i] = (uint8_t) strtoul(hex.substr(2 * i, 2).c_str(), nullptr, 16);
  return true;
}

static double lastval(const std::map<std::string, double> &m, const char *obis, double def) {
  auto it = m.find(obis);
  return it == m.end() ? def : it->second;
}

int main() {
  uint8_t key[16];
  if (!load_key("captures/gplugk.key", key)) { printf("skip: captures/gplugk.key not present\n"); return 0; }

  DlmsDecoder d;
  d.set_key(key);
  d.set_max_frame(600);
  bool ok = false;
  for (uint8_t c : load_hex("captures/gplugk_raw1.hex")) ok |= d.feed(c);
  CHECK(ok);

  std::map<std::string, double> got_num;
  std::map<std::string, std::string> got_str;
  int pairs = 0;
  bool parsed = DlmsDecoder::decode_structure(
      d.plaintext(), d.plaintext_len(),
      [&](const uint8_t obis6[6], const Value &v) {
        char key[32];
        snprintf(key, sizeof key, "%d.%d.%d", obis6[2], obis6[3], obis6[4]);
        pairs++;
        if (v.is_string) got_str[key] = std::string((const char *) v.str, v.str_len);
        else got_num[key] = v.num;
      });
  CHECK(parsed);
  CHECK(pairs >= 15);   // this preset (dlms-push-1) carries ~22 OBIS entries

  // Same values test_raw.cpp validated against the real MQTT payload, now via decode_structure().
  CHECK(lastval(got_num, "1.7.0", -1) == 0);          // Pi
  CHECK(lastval(got_num, "32.7.0", -1) == 236);        // V1
  CHECK(lastval(got_num, "52.7.0", -1) == 235);        // V2
  CHECK(lastval(got_num, "72.7.0", -1) == 236);        // V3
  CHECK(lastval(got_num, "1.8.0", -1) == 55978626);    // Ei (raw, unscaled uint32)
  CHECK(lastval(got_num, "3.8.0", -1) == 10469437);    // rEi

  printf(fails ? "%d FAILED\n" : "all ok\n", fails);
  return fails ? 1 : 0;
}
