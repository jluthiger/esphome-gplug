// Host unit test: clang++ -std=c++17 -I../components/gplug_smi test_dsmr.cpp -o test_dsmr && ./test_dsmr
#include "dsmr_parser.h"
#include <cstdio>
#include <map>
#include <string>

using namespace gplug_dsmr;

static std::string telegram(bool with_crc) {
  std::string t =
      "/ISk5\\2MT382-1000\r\n\r\n"
      "1-3:0.2.8(50)\r\n"
      "0-0:1.0.0(101209113020W)\r\n"
      "0-0:96.1.1(4B384547303034303436333935353037)\r\n"
      "1-0:1.8.1(123456.789*kWh)\r\n"
      "1-0:1.8.2(123456.789*kWh)\r\n"
      "1-0:2.8.1(123456.789*kWh)\r\n"
      "1-0:2.8.2(123456.789*kWh)\r\n"
      "0-0:96.14.0(0002)\r\n"
      "1-0:1.7.0(01.193*kW)\r\n"
      "1-0:2.7.0(00.000*kW)\r\n"
      "0-0:96.7.21(00004)\r\n"
      "1-0:32.7.0(220.1*V)\r\n"
      "1-0:31.7.0(001*A)\r\n"
      "1-0:21.7.0(01.111*kW)\r\n"
      "1-0:22.7.0(00.000*kW)\r\n"
      "0-1:24.1.0(003)\r\n"
      "0-1:96.1.0(3232323241424344313233343536373839)\r\n"
      "0-1:24.2.1(101209112500W)(12785.123*m3)\r\n"
      "!";
  if (with_crc) {
    char crc[8];
    snprintf(crc, sizeof crc, "%04X", DsmrParser::crc16(t.data(), t.size()));
    t += crc;
  }
  t += "\r\n";
  return t;
}

int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main() {
  // 1. valid telegram with CRC
  {
    DsmrParser p;
    std::map<std::string, std::pair<std::string, std::string>> got;
    bool done = false;
    for (char c : telegram(true))
      done |= p.feed(c, [&](const DsmrValue &v) { got[v.obis] = {v.value, v.unit}; });
    CHECK(done);
    CHECK(p.telegrams == 1 && p.crc_errors == 0);
    CHECK(got.size() == 18);
    CHECK(got["1-0:1.7.0"].first == "01.193" && got["1-0:1.7.0"].second == "kW");
    CHECK(got["0-0:96.1.1"].first == "4B384547303034303436333935353037" && got["0-0:96.1.1"].second == "");
    CHECK(got["0-1:24.2.1"].first == "12785.123" && got["0-1:24.2.1"].second == "m3");  // last group
    CHECK(got["1-0:32.7.0"].second == "V");
  }
  // 2. corrupted CRC → rejected
  {
    DsmrParser p;
    auto t = telegram(true);
    t[t.size() - 3] = (t[t.size() - 3] == '0') ? '1' : '0';
    int n = 0;
    for (char c : t) p.feed(c, [&](const DsmrValue &) { n++; });
    CHECK(n == 0 && p.crc_errors == 1);
  }
  // 3. garbage before telegram + two telegrams back to back
  {
    DsmrParser p;
    int n = 0;
    std::string s = "\xff\x00junk" + telegram(true) + telegram(true);
    for (char c : s) p.feed(c, [&](const DsmrValue &) { n++; });
    CHECK(n == 36 && p.telegrams == 2);
  }
  // 4. DSMR 2.2 style without CRC ("!\r\n")
  {
    DsmrParser p;
    int n = 0;
    for (char c : telegram(false)) p.feed(c, [&](const DsmrValue &) { n++; });
    CHECK(n == 18 && p.crc_errors == 0);
  }
  // 5. raw capture hook: fires once per telegram with the pristine bytes, before parse_() rewrites
  //    the buffer in place -- and fires on a bad CRC too, since that is exactly the telegram a user
  //    needs to look at in the Datenstrom view.
  {
    DsmrParser p;
    std::string captured;
    int calls = 0;
    bool last_ok = false;
    p.set_raw_callback([&](const char *d, size_t n, bool ok) { captured.assign(d, n); calls++; last_ok = ok; });
    auto t = telegram(true);
    for (char c : t) p.feed(c, [](const DsmrValue &) {});
    CHECK(calls == 1 && last_ok);
    CHECK(captured == t);                       // byte-identical, separators intact
    CHECK(captured.find("1-0:1.7.0") != std::string::npos);

    auto bad = telegram(true);
    bad[bad.size() - 3] = (bad[bad.size() - 3] == '0') ? '1' : '0';
    for (char c : bad) p.feed(c, [](const DsmrValue &) {});
    CHECK(calls == 2 && !last_ok);
    CHECK(captured == bad);
  }
  printf(fails ? "%d FAILED\n" : "all ok\n", fails);
  return fails ? 1 : 0;
}
