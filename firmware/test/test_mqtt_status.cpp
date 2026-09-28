// mqtt_status.h: the device-status message. What matters is that the layout is the documented one,
// that an access point's SSID cannot break the JSON, and that the constant worst case really bounds
// the output, since the render buffer is sized from it at compile time.
#include "mqtt_status.h"
#include <cstdio>
#include <string>
using namespace gplug_mqtt;

int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static std::string render_str(const DeviceStatus &d, bool *ok = nullptr) {
  char buf[PAYLOAD_BUF];
  size_t len = 0;
  bool r = render_status(d, buf, sizeof buf, len);
  if (ok) *ok = r;
  return std::string(buf, len);
}

static DeviceStatus sample() {
  DeviceStatus d{};
  d.fw = "0.4.0-dev";
  d.app = "0123456789abcdef";
  d.build = 1790000000u;
  d.uptime = 3600;
  d.reset = gplug_log::RR_PANIC;
  d.epoch = 1790003600u;
  d.free = 120000; d.min_free = 90000; d.largest = 60000;
  d.sockets = 9; d.sockets_max = 19;
  d.wifi = true; d.ssid = "home"; d.ip = "192.168.1.20"; d.rssi = -61;
  d.diag = "ok"; d.age = 2;
  d.sent = 100; d.dropped = 1; d.skipped = 0;
  return d;
}

int main() {
  // --- 1. layout ---
  {
    bool ok;
    std::string s = render_str(sample(), &ok);
    CHECK(ok);
    CHECK(s == "{\"fw\":\"0.4.0-dev\",\"app\":\"0123456789abcdef\",\"build\":1790000000,\"uptime\":3600,"
               "\"reset\":\"panic\",\"ts\":1790003600,\"mem\":{\"free\":120000,\"min_free\":90000,\"largest\":60000},"
               "\"sockets\":9,\"sockets_max\":19,\"wifi\":{\"connected\":true,\"ssid\":\"home\",\"ip\":\"192.168.1.20\","
               "\"rssi\":-61},\"meter\":{\"diag\":\"ok\",\"age\":2},\"mqtt\":{\"sent\":100,\"dropped\":1,\"skipped\":0}}");  }

  // --- 2. no clock, no Wi-Fi, no frame yet: nulls and no Wi-Fi details ---
  {
    DeviceStatus d = sample();
    d.epoch = 0; d.wifi = false; d.age = -1; d.reset = 200;
    std::string s = render_str(d);
    CHECK(s.find("\"ts\":null") != std::string::npos);
    CHECK(s.find("\"wifi\":{\"connected\":false}") != std::string::npos);
    CHECK(s.find("\"age\":null") != std::string::npos);
    CHECK(s.find("\"reset\":\"unknown\"") != std::string::npos);
  }

  // --- 3. hostile SSID: quotes, backslashes and control bytes stay inside the string ---
  {
    DeviceStatus d = sample();
    d.ssid = "a\"b\\c\x01";
    std::string s = render_str(d);
    CHECK(s.find("\"ssid\":\"a\\\"b\\\\c?\"") != std::string::npos);
  }

  // --- 4. the constant worst case bounds the longest possible message ---
  {
    std::string fw(100, '"'), ssid(100, '\\'), diag(40, '"');
    DeviceStatus d{};
    d.fw = fw.c_str(); d.app = "\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"";
    d.build = d.uptime = d.epoch = d.free = d.min_free = d.largest = 4294967295u;
    d.sent = d.dropped = d.skipped = 4294967295u;
    d.sockets = d.sockets_max = 255;
    d.reset = gplug_log::RR_DEEPSLEEP;
    d.wifi = true; d.ssid = ssid.c_str(); d.ip = "255.255.255.255.255"; d.rssi = -128;
    d.diag = diag.c_str(); d.age = 2147483647;
    bool ok;
    std::string s = render_str(d, &ok);
    CHECK(ok);
    CHECK(s.size() <= STATUS_WORST);
    // One byte short of the bound must still never write past the buffer.
    char small[64];
    size_t len = 0;
    CHECK(!render_status(d, small, sizeof small, len) && len < sizeof small && small[len] == '\0');
  }

  // --- 5. reset names follow gplug_log's numbering ---
  CHECK(strcmp(reset_name(gplug_log::RR_POWERON), "poweron") == 0);
  CHECK(strcmp(reset_name(gplug_log::RR_BROWNOUT), "brownout") == 0);
  CHECK(strcmp(reset_name(gplug_log::RR_JTAG), "jtag") == 0);

  printf("%s (%d failures)\n", fails ? "FAILED" : "all passed", fails);
  return fails ? 1 : 0;
}
