// A small ring of events that survives a reboot, so the answer to "it restarted in the night, why?"
// is on the device rather than in a serial log nobody was watching. This is deliberately *not* a
// log of messages: it is a fixed-size array of 12 B records that fits in one NVS blob, because the
// interesting events are rare (a boot, a lost Wi-Fi, a meter that went quiet) and their text is
// better rendered by the SPA, in the user's language, than stored on flash in German.
//
// Header-only, no ESPHome or IDF dependency, so test/test_eventlog.cpp exercises exactly the code
// the device runs. The storage backend is the caller's problem: gplug_smi.cpp hands the serialised
// blob to NVS, which does its own wear levelling -- appropriate here because the write volume is a
// handful of records a day, not a stream.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace gplug_log {

static constexpr uint8_t LOG_CAP = 32;        // 32 x 12 B + 4 B header = 388 B in NVS
static constexpr uint8_t LOG_VERSION = 1;
static constexpr uint32_t FOLD_WINDOW_S = 300;   // identical events within 5 min collapse into one

// What happened. Numbers are stored on flash, so they are append-only: never renumber, only add.
enum : uint8_t {
  EV_BOOT = 1,        // detail = reset reason (RR_* below), value = free heap in kB
  EV_OTA = 2,         // the running image changed across a reboot, i.e. an update took effect
  EV_WIFI_UP = 3,     // value = |RSSI| in dBm
  EV_WIFI_LOST = 4,
  EV_METER_LOST = 5,  // detail = the diag verdict that replaced "ok" (DIAG_* as in the API)
  EV_METER_OK = 6,
  EV_CONFIG = 7,      // detail: 1 hardware, 2 meter, 3 wifi
  EV_STORAGE = 8,     // detail: 1 history append failed, 2 history unavailable at boot
  EV_BUTTON = 9,      // AP button held: Wi-Fi credentials erased
};

// Mirrors esp_reset_reason_t, but decoupled from it: the numbers here go to flash and must not
// move if IDF ever renumbers its enum.
enum : uint8_t {
  RR_UNKNOWN = 0,
  RR_POWERON = 1,
  RR_EXT = 2,        // reset pin
  RR_SW = 3,         // esp_restart(), which is also what an OTA and a config save do
  RR_PANIC = 4,      // exception or panic -- the one that matters
  RR_INT_WDT = 5,
  RR_TASK_WDT = 6,
  RR_WDT = 7,
  RR_DEEPSLEEP = 8,
  RR_BROWNOUT = 9,   // supply dipped: a power problem, not a firmware problem
  RR_SDIO = 10,
  RR_USB = 11,
  RR_JTAG = 12,
};

struct __attribute__((packed)) Entry {
  uint32_t epoch;      // wall clock, 0 when the clock had never synced at that point
  uint32_t uptime_s;   // always meaningful, and the only ordering available before a sync
  uint8_t code;
  uint8_t detail;
  uint8_t value;
  uint8_t repeat;      // further identical events folded in; 0 = it happened once
};
static_assert(sizeof(Entry) == 12, "Entry must stay 12 B: the NVS blob layout depends on it");

struct __attribute__((packed)) Blob {
  uint8_t version;
  uint8_t count;       // entries in use, <= LOG_CAP
  uint8_t head;        // where the next entry goes
  uint8_t rsvd;
  Entry e[LOG_CAP];
};
static_assert(sizeof(Blob) == 4 + LOG_CAP * 12, "Blob must be packed with no padding");

class EventLog {
 public:
  EventLog() { clear(); }

  void clear() {
    memset(&b_, 0, sizeof b_);
    b_.version = LOG_VERSION;
  }

  uint8_t count() const { return b_.count; }
  bool dirty() const { return dirty_; }
  void mark_clean() { dirty_ = false; }

  // Oldest first, which is the order a reader wants and the order the SPA renders.
  const Entry &at(uint8_t i) const {
    uint8_t start = (uint8_t) ((b_.head + LOG_CAP - b_.count) % LOG_CAP);
    return b_.e[(start + i) % LOG_CAP];
  }

  // A repeat of the newest entry inside FOLD_WINDOW_S does not grow the ring, it bumps a counter:
  // a flapping Wi-Fi would otherwise push every other event out within minutes and write NVS each
  // time. Returns true when a new record was added (the caller flushes immediately for those and
  // rate-limits the folded ones).
  bool append(uint8_t code, uint8_t detail, uint8_t value, uint32_t epoch, uint32_t uptime_s) {
    Entry *last = b_.count ? &b_.e[(b_.head + LOG_CAP - 1) % LOG_CAP] : nullptr;
    if (last && last->code == code && last->detail == detail && uptime_s >= last->uptime_s &&
        uptime_s - last->uptime_s < FOLD_WINDOW_S) {
      if (last->repeat < 255) last->repeat++;
      last->uptime_s = uptime_s;
      if (epoch) last->epoch = epoch;
      last->value = value;
      dirty_ = true;
      return false;
    }
    Entry &e = b_.e[b_.head];
    e.epoch = epoch;
    e.uptime_s = uptime_s;
    e.code = code;
    e.detail = detail;
    e.value = value;
    e.repeat = 0;
    b_.head = (uint8_t) ((b_.head + 1) % LOG_CAP);
    if (b_.count < LOG_CAP) b_.count++;
    dirty_ = true;
    return true;
  }

  // Back-date the entries written before the clock synced, the same idea as the history store's
  // timestamp back-patching: uptime is known, so the wall clock of an earlier event is
  // now - (uptime_now - uptime_then). Only this boot's entries can be dated, and only once.
  void backdate(uint32_t epoch_now, uint32_t uptime_now) {
    if (!epoch_now) return;
    for (uint8_t i = 0; i < b_.count; i++) {
      Entry &e = b_.e[(b_.head + LOG_CAP - 1 - i) % LOG_CAP];
      if (e.epoch) break;                    // everything older already carries a clock
      if (e.uptime_s > uptime_now) break;    // from a previous boot, unknowable
      e.epoch = epoch_now - (uptime_now - e.uptime_s);
      dirty_ = true;
    }
  }

  // Serialisation is a straight copy: the struct is packed and only ever read back by the same
  // firmware, which checks the version byte and starts fresh on anything it does not recognise.
  const void *data() const { return &b_; }
  size_t size() const { return sizeof b_; }

  bool load(const void *p, size_t n) {
    if (n != sizeof b_) return false;
    Blob in;
    memcpy(&in, p, sizeof in);
    if (in.version != LOG_VERSION || in.count > LOG_CAP || in.head >= LOG_CAP) return false;
    b_ = in;
    dirty_ = false;
    return true;
  }

 private:
  Blob b_{};
  bool dirty_{false};
};

}  // namespace gplug_log
