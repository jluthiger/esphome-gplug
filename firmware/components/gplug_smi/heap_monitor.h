// Heap trend in RAM, so a slow leak shows up as a falling line days before the device runs out of
// heap and resets. A single `heap` figure in /api/status only shows a leak to someone who polls it by
// hand for hours and compares numbers. The ring is deliberately RAM-only: it is lost on reboot, but
// a reboot is exactly the moment the leak's damage is undone, and persisting it would add a
// periodic flash write for a diagnostic nobody reads most of the time.
//
// Header-only, no ESPHome or IDF dependency: gplug_smi.cpp reads heap_caps_* and feeds the numbers
// in, so test/test_heapmon.cpp exercises the ring and the low-heap latch exactly as the device runs
// them.
#pragma once
#include <cstddef>
#include <cstdint>

namespace gplug_mem {

// kB in uint16 rather than bytes in uint32: the C3 has ~400 kB of SRAM, so kB loses nothing a trend
// needs and keeps a sample at 12 B -- 288 samples (24 h at 5 min) cost 3.4 kB instead of 5.6 kB.
struct Sample {
  uint32_t up_s;
  uint16_t free_kb;
  uint16_t min_kb;       // minimum free heap since boot, as IDF tracks it
  uint16_t largest_kb;   // largest free block: falls with fragmentation even when free does not
  uint16_t rsvd;
};
static_assert(sizeof(Sample) == 12, "Sample is budgeted at 12 B in firmware/MEMORY.md");

template <size_t N> class Ring {
 public:
  void push(const Sample &s) {
    buf_[head_] = s;
    head_ = (head_ + 1) % N;
    if (count_ < N) count_++;
    pushed_++;
  }
  size_t count() const { return count_; }
  // Samples ever pushed. Sample i sits at sequence pushed() - count() + i, which lets a reader that
  // releases the lock between batches resume where it stopped even if a push moved the ring.
  uint32_t pushed() const { return pushed_; }
  // Oldest first, the order the SPA draws the line in.
  const Sample &at(size_t i) const { return buf_[(head_ + N - count_ + i) % N]; }

 private:
  Sample buf_[N]{};
  size_t head_{0};
  size_t count_{0};
  uint32_t pushed_{0};
};

// Thresholds against the intent target of >= 80 kB free after 24 h. The largest block matters as
// much as the free total: TLS and the httpd need contiguous buffers of several kB, and a request
// fails once no block is big enough, however much free heap is scattered around.
static constexpr uint16_t LOW_FREE_KB = 48;
static constexpr uint16_t LOW_LARGEST_KB = 12;
// Re-arm only after a clear recovery, so a heap hovering at the threshold does not log each sample.
static constexpr uint16_t REARM_FREE_KB = 64;
static constexpr uint16_t REARM_LARGEST_KB = 20;
// The event log lives in NVS, and each new record rewrites its 388 B blob (~14 NVS entries). Hold-off
// bounds the worst case -- heap oscillating across the whole hysteresis band -- at 24 records a day:
// ~340 entries, under three of the 448 kB partition's 112 pages of 126 entries, so over five years
// each page is erased ~45 times against a flash endurance in the tens of thousands. In a healthy
// device it never fires at all.
static constexpr uint32_t LOW_HOLDOFF_S = 3600;

enum Low : uint8_t { LOW_NONE = 0, LOW_FREE = 1, LOW_LARGEST = 2 };

class LowLatch {
 public:
  // Returns which figure crossed, once per excursion; LOW_NONE otherwise. Free is reported in
  // preference to largest when both are low, since it is the more fundamental of the two.
  Low update(uint16_t free_kb, uint16_t largest_kb, uint32_t up_s) {
    if (!armed_) {
      if (free_kb > REARM_FREE_KB && largest_kb > REARM_LARGEST_KB) armed_ = true;
      return LOW_NONE;
    }
    Low which = free_kb < LOW_FREE_KB ? LOW_FREE : largest_kb < LOW_LARGEST_KB ? LOW_LARGEST : LOW_NONE;
    if (which == LOW_NONE) return LOW_NONE;
    // Still armed inside the hold-off: stay armed, so a heap that stays low is reported once the
    // hold-off has passed rather than never.
    if (fired_ && up_s - last_s_ < LOW_HOLDOFF_S) return LOW_NONE;
    armed_ = false;
    fired_ = true;
    last_s_ = up_s;
    return which;
  }

 private:
  bool armed_{true};
  bool fired_{false};
  uint32_t last_s_{0};
};

}  // namespace gplug_mem
