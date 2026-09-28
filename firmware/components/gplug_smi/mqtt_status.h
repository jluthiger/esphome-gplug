// The device-status MQTT message (issue 17): what /api/status says about the gPlug itself, for a
// consumer that has no Home Assistant and would otherwise poll the device to notice a leak, a weak
// signal or a meter that went quiet.
//
// A fixed JSON layout rather than a second template: the consumers of this message are dashboards
// and alerts that read a handful of named fields, not the varied shapes the meter values go to, and
// a fixed layout needs no compile step, no preview port and no worst-case arithmetic per user
// template. Its worst case is a constant (STATUS_WORST), checked against the render buffer at build
// time. Key names follow /api/status where the same fact appears there.
//
// Never carries secrets: no GUEK, MQTT or OTA password, not even the key fingerprint.
//
// Header-only, no ESPHome or IDF dependency, tested in test/test_mqtt_status.cpp.
#pragma once
#include "event_log.h"
#include "mqtt_template.h"

namespace gplug_mqtt {

// Everything the message carries, gathered on the loop task.
struct DeviceStatus {
  const char *fw;          // gPlug release; cut at FW_MAX
  const char *app;         // first 16 hex digits of the image's ELF SHA-256
  uint32_t build;          // ESPHome build time
  uint32_t uptime;         // s
  uint8_t reset;           // gplug_log::RR_*
  uint32_t epoch;          // 0 = clock not set: "ts":null
  uint32_t free, min_free, largest;   // internal heap, bytes
  uint8_t sockets, sockets_max;
  bool wifi;               // connected; ssid/ip/rssi only then
  const char *ssid;        // cut at 32 bytes
  const char *ip;          // dotted quad
  int8_t rssi;
  const char *diag;        // GplugSmi::diag_() token
  int32_t age;             // s since the last good frame, -1 = none yet
  uint32_t sent, dropped, skipped;
};

static constexpr size_t FW_MAX = 32;
static constexpr size_t SSID_MAX = 32;

inline const char *reset_name(uint8_t rr) {
  static const char *const N[] = {"unknown", "poweron", "ext", "sw", "panic", "int_wdt", "task_wdt",
                                  "wdt", "deepsleep", "brownout", "sdio", "usb", "jtag"};
  return rr < sizeof N / sizeof N[0] ? N[rr] : N[0];
}

// Worst case, NUL excluded: the literal skeleton plus every field at its longest -- each string
// twice its cut length (all escaped), 10 digits for each of the 12 numbers ("null" is shorter),
// 9 for the reset name and 4 for the RSSI.
static constexpr size_t STATUS_WORST =
    sizeof("{\"fw\":\"\",\"app\":\"\",\"build\":,\"uptime\":,\"reset\":\"\",\"ts\":,\"mem\":{\"free\":,"
           "\"min_free\":,\"largest\":},\"sockets\":,\"sockets_max\":,\"wifi\":{\"connected\":false,\"ssid\":\"\","
           "\"ip\":\"\",\"rssi\":},\"meter\":{\"diag\":\"\",\"age\":},\"mqtt\":{\"sent\":,\"dropped\":,\"skipped\":}}") - 1 +
    2 * (FW_MAX + 16 + SSID_MAX + 15 + 16) + 12 * 10 + 9 + 4;
static_assert(STATUS_WORST + 1 <= PAYLOAD_BUF, "the status message renders into the payload buffer");

// Bounded copy of an untrusted string (the SSID is whatever the access point broadcasts), cut at
// max bytes before escaping so the bound above holds.
inline void put_cut_(Out_ &w, const char *s, size_t max) {
  w.put('"');
  for (size_t i = 0; s && s[i] && i < max; i++) {
    char ch = s[i];
    if ((uint8_t) ch < 0x20) { w.put('?'); continue; }
    if (ch == '"' || ch == '\\') w.put('\\');
    w.put(ch);
  }
  w.put('"');
}

// Writes the message NUL-terminated; false only if the buffer is smaller than STATUS_WORST + 1.
inline bool render_status(const DeviceStatus &d, char *out, size_t cap, size_t &len) {
  Out_ w{out, cap};
  auto lit = [&](const char *s) { w.raw(s, strlen(s)); };
  lit("{\"fw\":");       put_cut_(w, d.fw, FW_MAX);
  lit(",\"app\":");      put_cut_(w, d.app, 16);
  lit(",\"build\":");    w.u32(d.build);
  lit(",\"uptime\":");   w.u32(d.uptime);
  lit(",\"reset\":\"");  lit(reset_name(d.reset));
  lit("\",\"ts\":");
  if (d.epoch) w.u32(d.epoch); else lit("null");
  lit(",\"mem\":{\"free\":"); w.u32(d.free);
  lit(",\"min_free\":");  w.u32(d.min_free);
  lit(",\"largest\":");   w.u32(d.largest);
  lit("},\"sockets\":");  w.u32(d.sockets);
  lit(",\"sockets_max\":"); w.u32(d.sockets_max);
  lit(",\"wifi\":{\"connected\":");
  lit(d.wifi ? "true" : "false");
  if (d.wifi) {
    lit(",\"ssid\":"); put_cut_(w, d.ssid, SSID_MAX);
    lit(",\"ip\":");   put_cut_(w, d.ip, 15);
    lit(",\"rssi\":");
    char b[8];
    int l = snprintf(b, sizeof b, "%d", (int) d.rssi);
    w.raw(b, l > 0 ? (size_t) l : 0);
  }
  lit("},\"meter\":{\"diag\":"); put_cut_(w, d.diag, 16);
  lit(",\"age\":");
  if (d.age >= 0) w.u32((uint32_t) d.age); else lit("null");
  lit("},\"mqtt\":{\"sent\":"); w.u32(d.sent);
  lit(",\"dropped\":");   w.u32(d.dropped);
  lit(",\"skipped\":");   w.u32(d.skipped);
  lit("}}");
  if (cap > 0) out[w.n] = '\0';
  len = w.n;
  return !w.ovf;
}

}  // namespace gplug_mqtt
