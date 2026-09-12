// event_log.h: the ring that answers "why did it reboot", including the two behaviours that are
// easy to get wrong -- folding a flapping event instead of letting it push the history out, and
// back-dating entries written before the clock synced.
#include "event_log.h"
#include <cstdio>
#include <string>
#include <vector>
using namespace gplug_log;

int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main() {
  // --- 1. empty, then oldest-first ordering ---
  {
    EventLog l;
    CHECK(l.count() == 0);
    CHECK(!l.dirty());
    CHECK(l.append(EV_BOOT, RR_POWERON, 180, 1000, 1));
    CHECK(l.dirty());
    CHECK(l.append(EV_WIFI_UP, 0, 51, 1005, 6));
    CHECK(l.count() == 2);
    CHECK(l.at(0).code == EV_BOOT && l.at(0).uptime_s == 1);
    CHECK(l.at(1).code == EV_WIFI_UP && l.at(1).value == 51);
  }

  // --- 2. a flapping event folds instead of flooding the ring ---
  {
    EventLog l;
    l.append(EV_BOOT, RR_SW, 180, 1000, 1);
    CHECK(l.append(EV_WIFI_LOST, 0, 0, 1010, 10));          // new record
    for (int i = 1; i <= 40; i++)
      CHECK(!l.append(EV_WIFI_LOST, 0, 0, 1010 + i * 5, 10 + i * 5));   // all folded
    CHECK(l.count() == 2);                                   // boot entry survived the storm
    CHECK(l.at(0).code == EV_BOOT);
    CHECK(l.at(1).repeat == 40);
    CHECK(l.at(1).uptime_s == 10 + 40 * 5);                  // carries the latest occurrence
    // outside the window it becomes a record of its own again
    CHECK(l.append(EV_WIFI_LOST, 0, 0, 2000, 10 + 40 * 5 + FOLD_WINDOW_S));
    CHECK(l.count() == 3);
    // a different detail is a different event, never folded
    CHECK(l.append(EV_CONFIG, 1, 0, 2100, 900));
    CHECK(l.append(EV_CONFIG, 2, 0, 2101, 901));
    CHECK(l.count() == 5);
  }

  // --- 3. repeat counter saturates rather than wrapping to zero ---
  {
    EventLog l;
    l.append(EV_STORAGE, 1, 0, 0, 0);
    for (int i = 0; i < 400; i++) l.append(EV_STORAGE, 1, 0, 0, 0);
    CHECK(l.count() == 1);
    CHECK(l.at(0).repeat == 255);
  }

  // --- 4. wrap: oldest falls out, order stays oldest-first ---
  {
    EventLog l;
    for (int i = 0; i < LOG_CAP + 5; i++)
      l.append(EV_CONFIG, (uint8_t) (i % 3 + 1), (uint8_t) i, 1000 + i, (uint32_t) i * 1000);
    CHECK(l.count() == LOG_CAP);
    CHECK(l.at(0).uptime_s == 5 * 1000);                     // first five dropped
    CHECK(l.at(LOG_CAP - 1).uptime_s == (LOG_CAP + 4) * 1000);
    for (uint8_t i = 1; i < LOG_CAP; i++) CHECK(l.at(i).uptime_s > l.at(i - 1).uptime_s);
  }

  // --- 5. back-dating entries written before the clock synced ---
  {
    EventLog l;
    l.append(EV_BOOT, RR_PANIC, 170, 0, 2);        // no clock yet
    l.append(EV_WIFI_UP, 0, 45, 0, 9);             // still none
    l.backdate(1789000000u, 30);                   // clock arrives at uptime 30
    CHECK(l.at(0).epoch == 1789000000u - 28);
    CHECK(l.at(1).epoch == 1789000000u - 21);
    // an already-dated entry is never touched, and neither is anything older than it
    uint32_t was = l.at(0).epoch;
    l.backdate(1789009999u, 300);
    CHECK(l.at(0).epoch == was);
  }

  // --- 6. entries from a previous boot are left undated ---
  {
    EventLog l;
    l.append(EV_BOOT, RR_BROWNOUT, 150, 0, 5000);  // long uptime, previous boot, no clock
    l.append(EV_BOOT, RR_POWERON, 180, 0, 3);      // this boot
    l.backdate(1789000000u, 10);
    CHECK(l.at(1).epoch == 1789000000u - 7);       // this boot's entry: dated
    CHECK(l.at(0).epoch == 0);                     // uptime beyond now: left alone
  }

  // --- 7. round-trip through the blob, and rejection of anything else ---
  {
    EventLog a;
    a.append(EV_BOOT, RR_TASK_WDT, 160, 1700000000u, 4);
    a.append(EV_METER_LOST, 3, 0, 1700000100u, 104);
    a.mark_clean();
    CHECK(!a.dirty());

    std::string blob((const char *) a.data(), a.size());
    EventLog b;
    CHECK(b.load(blob.data(), blob.size()));
    CHECK(b.count() == 2);
    CHECK(b.at(0).code == EV_BOOT && b.at(0).detail == RR_TASK_WDT && b.at(0).value == 160);
    CHECK(b.at(1).code == EV_METER_LOST && b.at(1).epoch == 1700000100u);
    CHECK(!b.dirty());

    EventLog c;
    CHECK(!c.load(blob.data(), blob.size() - 1));            // truncated
    std::string bad = blob; bad[0] = 99;
    CHECK(!c.load(bad.data(), bad.size()));                  // unknown version
    bad = blob; bad[1] = (char) (LOG_CAP + 1);
    CHECK(!c.load(bad.data(), bad.size()));                  // impossible count
    bad = blob; bad[2] = (char) LOG_CAP;
    CHECK(!c.load(bad.data(), bad.size()));                  // head out of range
    CHECK(c.count() == 0);                                   // a rejected blob leaves it empty
  }

  // --- 8. a full ring survives the round trip in the right order ---
  {
    EventLog a;
    // spaced beyond the fold window, or they would collapse into a single entry -- which is the
    // behaviour test 2 pins, and exactly what makes a naive "fill the ring" loop misleading
    for (int i = 0; i < LOG_CAP + 3; i++)
      a.append(EV_WIFI_UP, 0, (uint8_t) i, 1000 + i, (uint32_t) i * (FOLD_WINDOW_S + 1));
    std::string blob((const char *) a.data(), a.size());
    EventLog b;
    CHECK(b.load(blob.data(), blob.size()));
    CHECK(b.count() == LOG_CAP);
    for (uint8_t i = 0; i < LOG_CAP; i++) CHECK(b.at(i).value == a.at(i).value);
  }

  printf(fails ? "%d FAILED\n" : "all ok\n", fails);
  return fails ? 1 : 0;
}
