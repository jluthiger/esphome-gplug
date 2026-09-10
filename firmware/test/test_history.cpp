// HistoryStore: append/rotate/wrap, crash recovery, and the aggregation maths.
//
// FakeFlash models the flash semantics that matter and that a std::vector stub would silently get
// wrong: programming only clears bits, erase works on whole 4 kB sectors, and any operation can be
// cut short mid-byte by a power cut. Every 0 -> 1 programming attempt is counted, so a
// missing-erase bug fails the test rather than quietly producing garbage (which is exactly what
// esp_partition_write does on the device).
#include "history_store.h"
#include <cstdio>
#include <cstring>
#include <vector>
using namespace gplug_hist;

int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

struct FakeFlash {
  std::vector<uint8_t> mem;
  long budget{-1};        // -1 = unlimited, else bytes until a simulated power cut
  int zero_to_one{0};     // programming a 1 over a 0 -- always a bug
  int bad_erase{0};       // misaligned / mis-sized erase -- always a bug

  explicit FakeFlash(size_t sectors) : mem(sectors * HIST_SECTOR, 0xFF) {}

  uint32_t size() const { return (uint32_t) mem.size(); }

  bool read(uint32_t off, void *dst, size_t n) {
    if ((size_t) off + n > mem.size()) return false;
    memcpy(dst, mem.data() + off, n);
    return true;
  }

  bool write(uint32_t off, const void *src, size_t n) {
    if ((size_t) off + n > mem.size()) return false;
    const uint8_t *s = (const uint8_t *) src;
    for (size_t i = 0; i < n; i++) {
      if (budget == 0) return false;
      if (budget > 0) budget--;
      if ((uint8_t) (mem[off + i] & s[i]) != s[i]) zero_to_one++;
      mem[off + i] = (uint8_t) (mem[off + i] & s[i]);
    }
    return true;
  }

  bool erase(uint32_t off, size_t n) {
    if (off % HIST_SECTOR || n % HIST_SECTOR || (size_t) off + n > mem.size()) { bad_erase++; return false; }
    size_t done = n;
    if (budget >= 0 && (size_t) budget < n) { done = (size_t) budget; budget = 0; }
    else if (budget > 0) budget -= (long) n;
    memset(mem.data() + off, 0xFF, done);
    return done == n;
  }
};

using Store = HistoryStore<FakeFlash>;

static HistRecord mk(int32_t base, uint32_t qh, uint8_t flags = 0) {
  QhAccum a;
  a.reset(flags);
  a.add((int16_t) base, true);
  a.add((int16_t) (base + 100), true);
  return a.close((uint32_t) (1000 + base), 500, qh_tag_make(qh, false), true);
}

int main() {
  // --- 1. fresh store, round-trip ---
  {
    FakeFlash f(4);
    Store st(f);
    CHECK(st.begin());
    CHECK(st.meta().count == 0);
    CHECK(st.last() == nullptr);
    CHECK(st.meta().seq == 1);

    HistRecord r = mk(200, 1000);
    CHECK(st.append(r));
    CHECK(st.meta().count == 1);
    const HistRecord *l = st.last();
    CHECK(l && l->ei_wh == 1200 && l->eo_wh == 500);
    CHECK(l->p_min == 200 && l->p_max == 300 && l->p_avg == 250);
    uint32_t qh = 0; bool est = true;
    CHECK(qh_tag_parse(l->qh_tag, &qh, &est) && qh == 1000 && !est);
    CHECK(st.meta().newest_qh == 1000 && st.meta().oldest_qh == 1000);
    CHECK(f.zero_to_one == 0 && f.bad_erase == 0);
  }

  // --- 2. sector rotation at exactly HIST_SLOTS ---
  {
    FakeFlash f(4);
    Store st(f);
    CHECK(st.begin());
    for (uint32_t i = 0; i < HIST_SLOTS; i++) CHECK(st.append(mk(100, 2000 + i)));
    CHECK(st.meta().cur_sec == 0 && st.meta().slot == HIST_SLOTS);
    CHECK(st.meta().count == HIST_SLOTS);
    CHECK(st.pending_erase());   // recycle pre-armed, not performed on the append path
    CHECK(st.append(mk(100, 2000 + HIST_SLOTS)));
    CHECK(st.meta().cur_sec == 1 && st.meta().slot == 1);
    CHECK(st.meta().seq == 2);
    CHECK(st.meta().count == HIST_SLOTS + 1);
    CHECK(f.zero_to_one == 0 && f.bad_erase == 0);
  }

  // --- 3. full wrap: oldest advances, count caps, for_each stays ordered ---
  {
    const size_t SECTORS = 4;
    FakeFlash f(SECTORS);
    Store st(f);
    CHECK(st.begin());
    uint32_t total = (uint32_t) (SECTORS * HIST_SLOTS + 10);
    for (uint32_t i = 0; i < total; i++) {
      CHECK(st.append(mk(100, 3000 + i)));
      st.service_erase();   // the device drives this from loop(); here, eagerly
    }
    CHECK(st.meta().count <= (SECTORS - 1) * HIST_SLOTS + HIST_SLOTS);
    CHECK(st.meta().oldest_sec != st.meta().cur_sec);
    CHECK(f.zero_to_one == 0 && f.bad_erase == 0);

    std::vector<uint8_t> buf(256);
    uint32_t seen = 0, prev_qh = 0;
    bool ordered = true;
    st.for_each(buf.data(), buf.size(), 0, [&](const HistRecord &, uint32_t qh, bool have) {
      if (have) { if (seen && qh <= prev_qh) ordered = false; prev_qh = qh; }
      seen++;
    });
    CHECK(ordered);
    CHECK(seen == st.meta().count);
    CHECK(prev_qh == 3000 + total - 1);   // newest record is last
  }

  // --- 4. recovery after a power cut mid-record ---
  {
    FakeFlash f(4);
    { Store st(f); CHECK(st.begin()); for (int i = 0; i < 5; i++) CHECK(st.append(mk(100, 4000 + i))); }
    f.budget = 7;                                   // die 7 bytes into the next record
    { Store st(f); st.begin(); st.append(mk(100, 4005)); }
    f.budget = -1;
    Store st(f);
    CHECK(st.begin());
    CHECK(st.meta().count >= 5);                    // nothing valid was lost
    const HistRecord *l = st.last();
    CHECK(l != nullptr && record_valid(*l));        // the torn record is rejected by the crc
    uint32_t qh = 0;
    CHECK(qh_tag_parse(l->qh_tag, &qh, nullptr) && qh == 4004);
    int before = f.zero_to_one;
    CHECK(st.append(mk(100, 4006)));                // resumes into virgin bytes, never over the tear
    CHECK(f.zero_to_one == before);
  }

  // --- 5. power cut between phase 1 and phase 2 leaves a valid, timeless record ---
  {
    FakeFlash f(4);
    { Store st(f); CHECK(st.begin()); CHECK(st.append(mk(100, 5000))); }
    f.budget = 16 + 2;   // payload lands, timestamp is cut in half
    { Store st(f); st.begin(); st.append(mk(100, 5001)); }
    f.budget = -1;
    Store st(f);
    CHECK(st.begin());
    const HistRecord *l = st.last();
    CHECK(l && record_valid(*l));                    // payload survived: crc excludes the timestamp
    CHECK(!qh_tag_parse(l->qh_tag, nullptr, nullptr));   // half-written tag fails crc7 -> "unknown"
  }

  // --- 6. interrupted erase must never win the seq election ---
  {
    FakeFlash f(4);
    { Store st(f); CHECK(st.begin()); for (int i = 0; i < 5; i++) CHECK(st.append(mk(100, 6000 + i))); }
    // Sector 2 is untouched (all 0xFF) -- its header reads seq 0xFFFFFFFF, the numeric maximum.
    Store st(f);
    CHECK(st.begin());
    CHECK(st.meta().cur_sec == 0);        // not sector 2
    CHECK(st.meta().seq == 1);
    CHECK(st.meta().count == 5);

    // A header with the right magic but a corrupt crc must also lose.
    FakeFlash g(4);
    { Store s2(g); CHECK(s2.begin()); for (int i = 0; i < 3; i++) CHECK(s2.append(mk(100, 6100 + i))); }
    HistSecHdr bogus;
    bogus.magic = HIST_MAGIC; bogus.version = HIST_VERSION; bogus.rsvd = 0;
    bogus.crc = 0x1234; bogus.seq = 99; bogus.first_qh = 0;
    memcpy(g.mem.data() + 2 * HIST_SECTOR, &bogus, HIST_HDR);
    Store s3(g);
    CHECK(s3.begin());
    CHECK(s3.meta().cur_sec == 0 && s3.meta().seq == 1);
  }

  // --- 7. back-patching timestamps written before the clock synced ---
  {
    FakeFlash f(4);
    Store st(f);
    CHECK(st.begin());
    uint32_t first = 0;
    for (int i = 0; i < 3; i++) {
      QhAccum a; a.reset(0); a.add(150, true);
      CHECK(st.append(a.close(900, 100, QH_UNKNOWN, true)));
      if (!i) first = st.last_addr();
    }
    CHECK(st.last() && !qh_tag_parse(st.last()->qh_tag, nullptr, nullptr));
    CHECK(st.patch_qh_seq(first, 3, 7000, true) == 3);
    CHECK(f.zero_to_one == 0);   // patching only clears bits

    std::vector<uint8_t> buf(256);
    uint32_t n = 0, got[3] = {0, 0, 0};
    st.for_each(buf.data(), buf.size(), 0, [&](const HistRecord &r, uint32_t qh, bool have) {
      CHECK(record_valid(r));   // the payload crc is untouched by the patch
      bool est = false;
      CHECK(have && qh_tag_parse(r.qh_tag, nullptr, &est) && est);
      if (n < 3) got[n] = qh;
      n++;
    });
    CHECK(n == 3 && got[0] == 7000 && got[1] == 7001 && got[2] == 7002);
    CHECK(st.patch_qh_seq(first, 3, 8000, true) == 0);   // already dated: refuses, silently and safely
  }

  // --- 8. QhAccum ---
  {
    QhAccum a;
    a.reset(0);
    a.add(-100, true); a.add(500, true); a.add(200, true);
    HistRecord r = a.close(10, 20, qh_tag_make(1, false), true);
    CHECK(r.p_min == -100 && r.p_max == 500 && r.p_avg == 200);
    CHECK(!(r.flags & HF_PARTIAL) && !(r.flags & HF_NO_DATA));
    CHECK(record_valid(r));

    a.reset(HF_BOOT_BEFORE);
    a.add(0, false); a.add(0, false);
    r = a.close(WH_ABSENT, WH_ABSENT, QH_UNKNOWN, false);
    CHECK((r.flags & HF_NO_DATA) && (r.flags & HF_PARTIAL) && (r.flags & HF_BOOT_BEFORE));
    CHECK(r.p_min == 0 && r.p_max == 0 && r.p_avg == 0);
    CHECK(record_valid(r));
    CHECK(a.ticks == 2 && a.n == 0);   // offered samples counted, dead-meter ones not aggregated

    a.reset(0);
    a.add(-32768, true); a.add(32767, true);
    r = a.close(0, 0, QH_UNKNOWN, true);
    CHECK(r.p_min == -32768 && r.p_max == 32767 && record_valid(r));
  }

  // --- 9. HistBucket aggregation and counter deltas ---
  {
    HistBucket b;
    b.begin(5, true);
    b.add(mk(100, 100), 100, true);
    b.add(mk(300, 101, HF_CONFIG_CHANGE), 101, true);
    CHECK(b.n == 2 && b.p_min == 100 && b.p_max == 400);
    CHECK(b.avg() == 250);   // mean of the two records' p_avg: (150 + 350) / 2
    CHECK(b.flags & HF_CONFIG_CHANGE);
    CHECK(b.last_ei == 1300);

    uint32_t d = 0;
    const uint32_t MAXD = 7500;
    CHECK(HistBucket::delta(1000, 1200, MAXD, &d) && d == 200);
    CHECK(!HistBucket::delta(WH_ABSENT, 1200, MAXD, &d));      // counter absent
    CHECK(!HistBucket::delta(1200, 1000, MAXD, &d));           // went backwards: meter swap / reset
    CHECK(!HistBucket::delta(0, 100000, MAXD, &d));            // implausible jump
  }

  // --- 10. qh_tag encoding ---
  {
    for (uint32_t qh = 0; qh < 3000; qh++) CHECK(qh_tag_make(qh, qh & 1) != QH_UNKNOWN);
    CHECK(qh_tag_make(QH_MAX, true) != QH_UNKNOWN);
    CHECK(qh_tag_make(QH_MAX + 1, false) == QH_UNKNOWN);   // out of range -> "unknown", never garbage

    uint32_t tag = qh_tag_make(123456, false);
    for (int bit = 0; bit < 31; bit++) {   // bit 31 is the "estimated" flag: flipping it is legal
      uint32_t qh = 0;
      bool ok = qh_tag_parse(tag ^ (1u << bit), &qh, nullptr);
      CHECK(!ok || qh != 123456);
    }
  }

  printf(fails ? "%d FAILED\n" : "all ok\n", fails);
  return fails ? 1 : 0;
}
