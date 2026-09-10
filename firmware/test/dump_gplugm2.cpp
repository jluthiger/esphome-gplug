#include "dlms_decoder.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>
using namespace gplug_dlms;

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

int main() {
  DlmsDecoder d;
  d.set_max_frame(600);
  bool ok = false;
  for (const char *f : {"captures/gplugm2_seg1.hex", "captures/gplugm2_seg2.hex", "captures/gplugm2_seg3.hex", "captures/gplugm2_seg4.hex"}) {
    for (uint8_t c : load_hex(f)) ok |= d.feed(c);
  }
  printf("decoded=%d frames=%u apdus=%u len=%zu\n", ok, d.stats.frames, d.stats.apdus, d.plaintext_len());
  if (!ok) return 1;
  uint8_t pat[8]; size_t n = DlmsDecoder::obis_pattern("18.2.1", pat);
  auto v = DlmsDecoder::find(d.plaintext(), d.plaintext_len(), pat, n);
  if (!v.found) { printf("18.2.1 NOT FOUND\n"); return 1; }
  printf("18.2.1 = %g  (expect 431192)  %s\n", v.num, v.num == 431192 ? "MATCH" : "MISMATCH");
  return v.num == 431192 ? 0 : 1;
}
