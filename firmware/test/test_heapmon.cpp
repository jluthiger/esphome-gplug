// heap_monitor.h: the RAM trend ring and the low-heap latch. The latch is the part with flash cost
// (each firing is an NVS event record), so its edge, hysteresis and hold-off are pinned here.
#include "heap_monitor.h"
#include <cstdio>
using namespace gplug_mem;

int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static Sample S(uint32_t up, uint16_t fr) { return Sample{up, fr, fr, fr, 0}; }

int main() {
  // --- 1. ring: empty, partial, oldest first ---
  {
    Ring<4> r;
    CHECK(r.count() == 0);
    r.push(S(10, 100));
    r.push(S(20, 99));
    CHECK(r.count() == 2);
    CHECK(r.at(0).up_s == 10 && r.at(1).up_s == 20);
  }

  // --- 2. ring: wraps, keeps the newest N in order ---
  {
    Ring<4> r;
    for (uint32_t i = 0; i < 11; i++) r.push(S(i, (uint16_t) (200 - i)));
    CHECK(r.count() == 4);
    for (size_t i = 0; i < 4; i++) CHECK(r.at(i).up_s == 7 + i);
    CHECK(r.at(3).free_kb == 190);
    CHECK(r.pushed() == 11);
    CHECK(r.at(0).up_s == r.pushed() - r.count());   // sequence of at(0), as handle_heap_ uses it
  }

  // --- 3. latch: healthy heap never fires; a drop fires once ---
  {
    LowLatch l;
    CHECK(l.update(180, 60, 300) == LOW_NONE);
    CHECK(l.update(47, 60, 600) == LOW_FREE);
    CHECK(l.update(40, 60, 900) == LOW_NONE);      // still low: no repeat
    CHECK(l.update(10, 5, 99999) == LOW_NONE);     // disarmed, hold-off irrelevant
  }

  // --- 4. hysteresis: back above the trigger but inside the band does not re-arm ---
  {
    LowLatch l;
    CHECK(l.update(40, 60, 300) == LOW_FREE);
    CHECK(l.update(60, 60, 10000) == LOW_NONE);    // 60 > 48 but not > 64
    CHECK(l.update(40, 60, 20000) == LOW_NONE);    // so this is not a new excursion
    CHECK(l.update(70, 25, 30000) == LOW_NONE);    // clear recovery re-arms
    CHECK(l.update(40, 25, 40000) == LOW_FREE);
  }

  // --- 5. largest block: fires on fragmentation alone; re-arm needs both figures ---
  {
    LowLatch l;
    CHECK(l.update(150, 11, 300) == LOW_LARGEST);
    CHECK(l.update(150, 18, 10000) == LOW_NONE);   // largest not > 20: stays disarmed
    CHECK(l.update(150, 21, 10300) == LOW_NONE);   // re-armed
    CHECK(l.update(30, 8, 20000) == LOW_FREE);     // both low: free wins
  }

  // --- 6. hold-off: a quick re-arm and drop waits, then still reports ---
  {
    LowLatch l;
    CHECK(l.update(40, 60, 1000) == LOW_FREE);
    CHECK(l.update(100, 60, 1300) == LOW_NONE);    // re-armed
    CHECK(l.update(40, 60, 1600) == LOW_NONE);     // 600 s after firing: held off
    CHECK(l.update(40, 60, 1000 + LOW_HOLDOFF_S - 1) == LOW_NONE);
    CHECK(l.update(40, 60, 1000 + LOW_HOLDOFF_S) == LOW_FREE);   // stayed armed, now reported
  }

  printf(fails ? "%d FAILED\n" : "all ok\n", fails);
  return fails ? 1 : 0;
}
