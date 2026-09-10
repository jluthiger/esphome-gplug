// Inspection tool, not a pass/fail test: feed the real gPlugM capture through the decoder and print
// exactly what find() produces for every OBIS in the preset, so we see ground truth instead of guessing.
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
  for (const char *f : {"captures/gplugm_seg1.hex", "captures/gplugm_seg2.hex", "captures/gplugm_seg3.hex"}) {
    for (uint8_t c : load_hex(f)) ok |= d.feed(c);
  }
  printf("decoded=%d frames=%u fcs_err=%u hcs_err=%u apdus=%u len=%zu\n",
         ok, d.stats.frames, d.stats.fcs_errors, d.stats.hcs_errors, d.stats.apdus, d.plaintext_len());
  if (!ok) return 1;

  struct { const char *obis; const char *name; double scale; double expect; } fields[] = {
    {"0.0.1", "SMid(as num, should fail-string)", 1, 0},
    {"1.7.0", "Pi", 1000, 0.015},
    {"2.7.0", "Po", 1000, 0.000},
    {"31.7.0", "I1", 1, 61},   // raw units per GEAG log, unscaled
    {"51.7.0", "I2", 1, 33},
    {"71.7.0", "I3", 1, 64},
    {"1.8.0", "Ei", 1000, 16570.267},
    {"2.8.0", "Eo", 1000, 5263.351},
    {"1.8.1", "Ei1", 1000, 7971.601},
    {"1.8.2", "Ei2", 1000, 8598.138},
    {"2.8.1", "Eo1", 1000, 3918.952},
    {"2.8.2", "Eo2", 1000, 1344.144},
  };
  for (auto &f : fields) {
    uint8_t pat[8]; size_t n = DlmsDecoder::obis_pattern(f.obis, pat);
    auto v = DlmsDecoder::find(d.plaintext(), d.plaintext_len(), pat, n);
    if (!v.found) { printf("%-8s %-6s NOT FOUND (expect %g)\n", f.obis, f.name, f.expect); continue; }
    if (v.is_string) {
      printf("%-8s %-6s STRING len=%u '%.*s'\n", f.obis, f.name, v.str_len, v.str_len, v.str);
    } else {
      double got = v.num / f.scale;
      printf("%-8s %-6s = %g  (raw %g)  expect %g  %s\n", f.obis, f.name, got, v.num, f.expect,
             got == f.expect ? "MATCH" : "MISMATCH");
    }
  }
  // SM-ID as string (correct type for this meter)
  uint8_t pat[8]; size_t n = DlmsDecoder::obis_pattern("96.9.0", pat);
  return 0;
}
