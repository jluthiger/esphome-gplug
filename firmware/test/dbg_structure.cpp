// Diagnostic: dump the descriptor array and trailing value array of a capture-list APDU in full,
// with byte offsets, so the real index mapping can be read off instead of guessed.
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

int main(int argc, char **argv) {
  DlmsDecoder d;
  d.set_max_frame(700);
  bool ok = false;
  for (int i = 1; i < argc; i++) for (uint8_t c : load_hex(argv[i])) ok |= d.feed(c);
  printf("decoded=%d len=%zu\n", ok, d.plaintext_len());
  if (!ok) return 1;
  const uint8_t *buf = d.plaintext();
  size_t len = d.plaintext_len();

  // find anchor
  size_t a = 0; uint8_t n = 0;
  for (; a + 4 <= len; a++) {
    if (buf[a] == 0x02 && buf[a + 2] == 0x01 && buf[a + 3] == buf[a + 1] && buf[a+1] > 0 && buf[a+1] <= 64) { n = buf[a + 1]; break; }
  }
  if (a + 4 > len) { printf("no anchor found\n"); return 1; }
  printf("anchor at %zu, n=%u\n", a, n);

  size_t pos = a + 4;
  for (uint8_t k = 0; k < n; k++) {
    if (buf[pos] != 0x02) { printf("k=%u pos=%zu: expected structure tag, got %02x\n", k, pos, buf[pos]); break; }
    uint8_t m = buf[pos + 1];
    size_t start = pos;
    pos += 2;
    printf("descriptor[%u] at %zu, %u elements:", k, start, m);
    const uint8_t *obis = nullptr;
    for (uint8_t e = 0; e < m; e++) {
      if (buf[pos] == 0x09 && buf[pos+1] == 6) obis = buf + pos + 2;
      size_t sub = DlmsDecoder::skip_len(buf + pos, len - pos);
      printf(" [tag=%02x len=%zu]", buf[pos], sub);
      if (!sub) { printf(" ABORT\n"); return 1; }
      pos += sub;
    }
    if (obis) printf("  obis=%02x.%02x.%02x.%02x.%02x.%02x", obis[0],obis[1],obis[2],obis[3],obis[4],obis[5]);
    printf("\n");
  }
  printf("values start at %zu\n", pos);
  int idx = 0;
  while (pos < len) {
    auto v = DlmsDecoder::decode_value(buf + pos, len - pos);
    size_t sub = DlmsDecoder::skip_len(buf + pos, len - pos);
    if (!sub) { printf("value[%d] at %zu: tag=%02x UNRECOGNISED, stopping\n", idx, pos, buf[pos]); break; }
    if (v.found && !v.is_string) printf("value[%d] at %zu: tag=%02x num=%g\n", idx, pos, buf[pos], v.num);
    else if (v.found) printf("value[%d] at %zu: tag=%02x string len=%u\n", idx, pos, buf[pos], v.str_len);
    else printf("value[%d] at %zu: tag=%02x (nested/unhandled by decode_value)\n", idx, pos, buf[pos]);
    pos += sub;
    idx++;
  }
  return 0;
}
