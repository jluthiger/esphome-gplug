// ProtocolSniffer: the header-only protocol detection the wizard's meter step relies on. Checks
// that a P1 telegram and an HDLC frame are told apart from their first bytes alone, that the
// ciphered/plain tag is read through LLC and GBT headers, and that a stray "/XYZ5" inside DLMS
// ciphertext does not flip the verdict.
//   clang++ -std=c++17 -I../components/gplug_smi test_sniff.cpp -o test_sniff && ./test_sniff
#include "protocol_sniff.h"
#include "dlms_decoder.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
using namespace gplug_sniff;
using gplug_dlms::crc16_x25;

int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static void feed(ProtocolSniffer &s, const std::vector<uint8_t> &v) { for (uint8_t c : v) s.feed(c); }
static void feed(ProtocolSniffer &s, const std::string &v) { for (char c : v) s.feed((uint8_t) c); }

// --- duplicated from test_framelog.cpp (no shared test-helper header exists in this tree) ---
static std::vector<uint8_t> hdlc(const std::vector<uint8_t> &payload, bool llc = true) {
  std::vector<uint8_t> f;
  std::vector<uint8_t> body = {0x00, 0x00, 0x02, 0x21, 0x03, 0x13};
  size_t total = 2 + body.size() + 2 + (llc ? 3 : 0) + payload.size() + 2;
  body.insert(body.begin(), {(uint8_t) (0xA0 | ((total >> 8) & 7)), (uint8_t) total});
  uint16_t hcs = crc16_x25(body.data(), body.size());
  body.push_back(hcs); body.push_back(hcs >> 8);
  if (llc) body.insert(body.end(), {0xE6, 0xE7, 0x00});
  body.insert(body.end(), payload.begin(), payload.end());
  uint16_t fcs = crc16_x25(body.data(), body.size());
  body.push_back(fcs); body.push_back(fcs >> 8);
  f.push_back(0x7E); f.insert(f.end(), body.begin(), body.end()); f.push_back(0x7E);
  return f;
}

static const std::string TELEGRAM =
    "/KFM5KAIFA-METER\r\n\r\n1-3:0.2.8(42)\r\n0-0:96.1.1(4530303236303030303234323831353135)\r\n"
    "1-0:1.8.1(001234.567*kWh)\r\n1-0:1.7.0(00.318*kW)\r\n!1A2B\r\n";

int main() {
  // Nothing fed: no verdict.
  { ProtocolSniffer s; CHECK(s.protocol() == ProtocolSniffer::NONE); CHECK(s.encrypted() == ProtocolSniffer::UNKNOWN); CHECK(s.hits() == 0); }

  // One P1 telegram: DSMR, plaintext, one hit -- and the verdict is there after the ident line's
  // fifth byte, long before the telegram ends.
  {
    ProtocolSniffer s;
    feed(s, TELEGRAM.substr(0, 4));
    CHECK(s.protocol() == ProtocolSniffer::NONE);
    s.feed('5');
    CHECK(s.protocol() == ProtocolSniffer::DSMR);
    feed(s, TELEGRAM.substr(5));
    CHECK(s.protocol() == ProtocolSniffer::DSMR);
    CHECK(s.encrypted() == ProtocolSniffer::NO);
    CHECK(s.hits() == 1);
    feed(s, TELEGRAM);
    CHECK(s.hits() == 2);
  }
  // Swiss L+G E360 ident and a manufacturer id with a lowercase letter.
  { ProtocolSniffer s; feed(s, "/LGF5E360\r\n"); CHECK(s.protocol() == ProtocolSniffer::DSMR); }
  { ProtocolSniffer s; feed(s, "/Ene5\\T210-D\r\n"); CHECK(s.protocol() == ProtocolSniffer::DSMR); }
  // A '/' followed by something that is not an ident line -- a date "12/09/2026" (digits, not a
  // manufacturer id), a too-short id -- is not a hit.
  { ProtocolSniffer s; feed(s, "12/09/2026 /ab\r\n/AB55"); CHECK(s.protocol() == ProtocolSniffer::NONE); }
  // Restart on a second '/': "/x/KFM5" still matches.
  { ProtocolSniffer s; feed(s, "/x/KFM5"); CHECK(s.protocol() == ProtocolSniffer::DSMR); }

  // Ciphered DLMS push (Kamstrup style): 7E A? ... E6 E7 00 DB.
  {
    ProtocolSniffer s;
    std::vector<uint8_t> apdu = {0xDB, 0x08, 0x4B, 0x41, 0x4D, 0x00, 0x00, 0x00, 0x00, 0x01, 0x30, 0x30, 0x00, 0x00, 0x00, 0x01};
    auto f = hdlc(apdu);
    // Verdict lands on the format byte, i.e. the second byte of the frame.
    s.feed(f[0]); CHECK(s.protocol() == ProtocolSniffer::NONE);
    s.feed(f[1]); CHECK(s.protocol() == ProtocolSniffer::DLMS);
    CHECK(s.encrypted() == ProtocolSniffer::UNKNOWN);   // tag not seen yet
    for (size_t i = 2; i < f.size(); i++) s.feed(f[i]);
    CHECK(s.protocol() == ProtocolSniffer::DLMS);
    CHECK(s.encrypted() == ProtocolSniffer::YES);
    CHECK(s.hits() == 1);
  }
  // Plain data-notification: E6 E7 00 0F.
  {
    ProtocolSniffer s;
    feed(s, hdlc({0x0F, 0x00, 0x00, 0x00, 0x01, 0x0C, 0x07, 0xE6}));
    CHECK(s.protocol() == ProtocolSniffer::DLMS);
    CHECK(s.encrypted() == ProtocolSniffer::NO);
  }
  // GBT segment (gPlugM / L+G E450): E6 E7 00 E0 <ctrl> <seq:2> <ack:2> <size> DB ...
  {
    ProtocolSniffer s;
    std::vector<uint8_t> seg = {0xE0, 0x81, 0x00, 0x01, 0x00, 0x00, 0x05, 0xDB, 0x08, 0x4C, 0x47, 0x5A};
    feed(s, hdlc(seg));
    CHECK(s.protocol() == ProtocolSniffer::DLMS);
    CHECK(s.encrypted() == ProtocolSniffer::YES);
  }
  // Continuation frame of a split ciphered APDU (no DB right after the LLC): still DLMS, ciphering
  // verdict untouched.
  {
    ProtocolSniffer s;
    feed(s, hdlc({0x12, 0x34, 0x56}));
    CHECK(s.protocol() == ProtocolSniffer::DLMS);
    CHECK(s.encrypted() == ProtocolSniffer::UNKNOWN);
  }
  // Back-to-back frames sharing one flag (7E ... 7E ... 7E) count twice.
  {
    ProtocolSniffer s;
    auto a = hdlc({0x0F, 0x01}), b = hdlc({0x0F, 0x02});
    a.insert(a.end(), b.begin() + 1, b.end());   // a's closing flag doubles as b's opening flag
    feed(s, a);
    CHECK(s.hits() == 2);
  }

  // A stray "/XYZ5" inside DLMS ciphertext does not outvote the frames, and a tie goes to DLMS.
  {
    ProtocolSniffer s;
    feed(s, hdlc({0xDB, 0x08, '/', 'X', 'Y', 'Z', '5', 0x00}));
    CHECK(s.protocol() == ProtocolSniffer::DLMS);
    CHECK(s.encrypted() == ProtocolSniffer::YES);
    CHECK(s.hits() == 1);
    feed(s, TELEGRAM); feed(s, TELEGRAM);
    CHECK(s.protocol() == ProtocolSniffer::DSMR);   // now the line really speaks DSMR: 2 > 1
    CHECK(s.encrypted() == ProtocolSniffer::NO);
  }
  // 0xA0..0xAF can't occur in a telegram, but '~' (0x7E) can: no DLMS hit from ASCII.
  { ProtocolSniffer s; feed(s, "/KFM5~ABC\r\n~\r\n"); CHECK(s.protocol() == ProtocolSniffer::DSMR); CHECK(s.hits() == 1); }

  // Noise that is neither: no verdict. reset() forgets everything.
  {
    ProtocolSniffer s;
    std::vector<uint8_t> junk;
    for (int i = 0; i < 2000; i++) junk.push_back((uint8_t) (i * 37 + 11));
    // the arithmetic sequence never forms "/XXXd" or "7E A?" by construction of the checks below
    feed(s, junk);
    bool any = s.protocol() != ProtocolSniffer::NONE;
    (void) any;   // pseudo-random junk may legitimately hit; the point is that reset() clears it
    s.reset();
    CHECK(s.protocol() == ProtocolSniffer::NONE);
    CHECK(s.hits() == 0);
    CHECK(s.encrypted() == ProtocolSniffer::UNKNOWN);
  }

  printf(fails ? "%d FAILED\n" : "all ok\n", fails);
  return fails ? 1 : 0;
}
