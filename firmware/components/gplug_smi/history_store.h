// Persistent 15-minute history: an append-only log of fixed-size records over a raw flash
// partition, organised as rotating 4 kB sector "buckets". Appending costs one 20 B write; a sector
// is erased only when it is recycled (once per ~2.6 days at 15 min/record). No filesystem.
//
// Header-only, no ESPHome deps, host-testable (firmware/test/test_history.cpp) -- same convention
// as dlms_decoder.h / frame_log.h. All flash I/O goes through an injected backend so the *shipping*
// code path is what the host tests exercise (see PartitionFlash in partition_flash.h for the device
// backend, FakeFlash in the test for a fault-injecting one).
//
// Layout, per 4 kB sector: 16 B header + 204 records x 20 B = 4096 B exactly.
// With the 704 kB `data` partition (176 sectors) that is 176*204 = 35904 records ~= 374 days.
//
// Two flash facts drive the record layout:
//   * Programming can only clear bits (1 -> 0); raising a bit needs a 4 kB erase. Hence "unknown"
//     is encoded as all-ones, never as zero -- an all-ones field can still be filled in later.
//   * A record must be self-validating, or a power cut mid-write is indistinguishable from data.
//     Hence the crc8 over the payload.
// Together they give the two-phase write: bytes 0..15 (payload + crc) are written when the interval
// closes; bytes 16..19 (the timestamp) are written separately and are *excluded from the crc*, so a
// record stored before NTP synced can be back-dated later by a second write to the same slot, with
// no erase and no crc recomputation.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace gplug_hist {

static constexpr uint32_t HIST_EPOCH = 1577836800u;   // 2020-01-01T00:00:00Z, base for qh indices
static constexpr uint32_t HIST_INTERVAL_S = 900;      // quarter hour
static constexpr size_t HIST_SECTOR = 4096;
static constexpr size_t HIST_HDR = 16;
static constexpr size_t HIST_REC = 20;
static constexpr size_t HIST_SLOTS = (HIST_SECTOR - HIST_HDR) / HIST_REC;   // 204, no slack
static constexpr uint32_t HIST_MAGIC = 0x48514C47u;   // 'GLQH'
static constexpr uint8_t HIST_VERSION = 1;

static constexpr uint32_t QH_UNKNOWN = 0xFFFFFFFFu;   // erased qh_tag: time was not known
static constexpr uint32_t WH_ABSENT = 0xFFFFFFFFu;    // meter sends no such counter
static constexpr uint32_t QH_MAX = 0xFFFFFEu;         // 0xFFFFFF reserved so a tag never reads all-ones

enum : uint8_t {
  HF_LOCAL_ENERGY = 1 << 0,   // ei/eo were integrated from the 10 s samples, not read from the meter
  HF_PARTIAL = 1 << 1,        // interval shorter than 15 min (boot, clock step, config change)
  HF_BOOT_BEFORE = 1 << 2,    // a reboot happened between the previous record and this one
  HF_CONFIG_CHANGE = 1 << 3,  // meter config was replaced -- the counter chain is broken here
  HF_NO_DATA = 1 << 4,        // no sample in this interval carried meter data
  HF_RESERVED = 0xE0,         // must be zero; part of the structural sanity check
};

inline uint8_t crc8_07(const uint8_t *p, size_t n) {
  uint8_t c = 0;
  for (size_t i = 0; i < n; i++) {
    c ^= p[i];
    for (int b = 0; b < 8; b++) c = (c & 0x80) ? (uint8_t) ((c << 1) ^ 0x07) : (uint8_t) (c << 1);
  }
  return c;
}

inline uint16_t crc16_ccitt(const uint8_t *p, size_t n) {
  uint16_t c = 0xFFFF;
  for (size_t i = 0; i < n; i++) {
    c ^= (uint16_t) p[i] << 8;
    for (int b = 0; b < 8; b++) c = (c & 0x8000) ? (uint16_t) ((c << 1) ^ 0x1021) : (uint16_t) (c << 1);
  }
  return c;
}

// One interval. Field order is the on-flash layout; little-endian, packed, never padded.
struct __attribute__((packed)) HistRecord {
  uint32_t ei_wh{WH_ABSENT};   //  0  cumulative import, Wh
  uint32_t eo_wh{WH_ABSENT};   //  4  cumulative export, Wh
  int16_t p_min{0};            //  8  net power over the interval, W
  int16_t p_max{0};            // 10
  int16_t p_avg{0};            // 12  mean of the interval's 10 s samples (NOT derived from ei/eo:
                               //     values_[] is float, so a 15-min counter delta carries ~+-32 W)
  uint8_t flags{0};            // 14
  uint8_t crc{0};              // 15  crc8 over bytes 0..14 -- phase 1 ends here
  uint32_t qh_tag{QH_UNKNOWN}; // 16  phase 2, written separately, excluded from the crc
};
static_assert(sizeof(HistRecord) == HIST_REC, "HistRecord must be exactly 20 B");

struct __attribute__((packed)) HistSecHdr {
  uint32_t magic{0};      //  0
  uint8_t version{0};     //  4
  uint8_t rsvd{0};        //  5
  uint16_t crc{0};        //  6  crc16 over the header with these two bytes zeroed
  uint32_t seq{0};        //  8  global monotone sector sequence; 0 and 0xFFFFFFFF are invalid
  uint32_t first_qh{0};   // 12  qh when the header was written -- a hint for skipping sectors
};
static_assert(sizeof(HistSecHdr) == HIST_HDR, "HistSecHdr must be exactly 16 B");

// qh_tag: bits 0..23 quarter-hour index since HIST_EPOCH, bits 24..30 crc7, bit 31 "estimated"
// (back-dated after a late clock sync, so the value is approximate).
inline uint32_t qh_tag_make(uint32_t qh, bool estimated) {
  if (qh > QH_MAX) return QH_UNKNOWN;
  uint8_t b[3] = {(uint8_t) qh, (uint8_t) (qh >> 8), (uint8_t) (qh >> 16)};
  uint32_t c7 = (uint32_t) (crc8_07(b, 3) >> 1) & 0x7Fu;
  return qh | (c7 << 24) | (estimated ? 0x80000000u : 0u);
}

inline bool qh_tag_parse(uint32_t tag, uint32_t *qh, bool *estimated) {
  if (tag == QH_UNKNOWN) return false;
  uint32_t v = tag & 0xFFFFFFu;
  uint8_t b[3] = {(uint8_t) v, (uint8_t) (v >> 8), (uint8_t) (v >> 16)};
  if ((((uint32_t) crc8_07(b, 3) >> 1) & 0x7Fu) != ((tag >> 24) & 0x7Fu)) return false;
  if (qh) *qh = v;
  if (estimated) *estimated = (tag >> 31) != 0;
  return true;
}

inline void record_finalize(HistRecord &r) { r.crc = crc8_07(reinterpret_cast<const uint8_t *>(&r), 15); }

// crc plus cheap structural checks: together these make a torn write vanishingly unlikely to be
// accepted as data, which one byte of crc alone would not.
inline bool record_valid(const HistRecord &r) {
  if (r.crc != crc8_07(reinterpret_cast<const uint8_t *>(&r), 15)) return false;
  if (r.flags & HF_RESERVED) return false;
  if (r.p_min > r.p_avg || r.p_avg > r.p_max) return false;
  return true;
}

inline bool record_erased(const HistRecord &r) {
  const uint8_t *p = reinterpret_cast<const uint8_t *>(&r);
  for (size_t i = 0; i < HIST_REC; i++)
    if (p[i] != 0xFF) return false;
  return true;
}

// Accumulates one interval's 10 s samples. Takes no clock of its own -- the caller passes the
// timestamp in, exactly as FrameLog does.
struct QhAccum {
  int32_t sum{0};
  uint32_t n{0};          // samples that carried meter data
  uint32_t ticks{0};      // samples offered, meter data or not
  int16_t lo{0}, hi{0};
  uint8_t flags{0};

  void reset(uint8_t carry_flags) { sum = 0; n = 0; ticks = 0; lo = 0; hi = 0; flags = carry_flags; }

  void add(int16_t p_net, bool meter_ok) {
    ticks++;
    if (!meter_ok) return;   // a dead meter reads 0 W; counting it would drag min/avg to zero
    if (!n) { lo = hi = p_net; }
    else { if (p_net < lo) lo = p_net; if (p_net > hi) hi = p_net; }
    sum += p_net;
    n++;
  }

  HistRecord close(uint32_t ei_wh, uint32_t eo_wh, uint32_t qh_tag, bool expect_full) const {
    HistRecord r;
    r.ei_wh = ei_wh;
    r.eo_wh = eo_wh;
    r.qh_tag = qh_tag;
    r.p_min = n ? lo : 0;
    r.p_max = n ? hi : 0;
    r.p_avg = n ? (int16_t) (sum / (int32_t) n) : 0;   // always within [lo, hi], so record_valid holds
    r.flags = flags;
    if (!n) r.flags |= HF_NO_DATA;
    if (!expect_full) r.flags |= HF_PARTIAL;
    record_finalize(r);
    return r;
  }
};

// Aggregates consecutive records into one output point. Lives here (rather than in the HTTP
// handler) so the downsampling maths is host-testable.
struct HistBucket {
  uint32_t key{0};
  bool has_key{false};
  bool started{false};
  int16_t p_min{0}, p_max{0};
  int32_t p_sum{0};
  uint32_t n{0};
  uint8_t flags{0};
  uint32_t last_ei{WH_ABSENT}, last_eo{WH_ABSENT};
  uint32_t qh{0};

  void begin(uint32_t k, bool hk) {
    key = k; has_key = hk; started = true;
    p_min = p_max = 0; p_sum = 0; n = 0; flags = 0;
    last_ei = last_eo = WH_ABSENT; qh = 0;
  }

  void add(const HistRecord &r, uint32_t r_qh, bool have_qh) {
    if (!n) { p_min = r.p_min; p_max = r.p_max; }
    else { if (r.p_min < p_min) p_min = r.p_min; if (r.p_max > p_max) p_max = r.p_max; }
    p_sum += r.p_avg;
    n++;
    flags |= r.flags;
    if (r.ei_wh != WH_ABSENT) last_ei = r.ei_wh;
    if (r.eo_wh != WH_ABSENT) last_eo = r.eo_wh;
    if (have_qh) qh = r_qh;
  }

  int16_t avg() const { return n ? (int16_t) (p_sum / (int32_t) n) : 0; }

  // Counter delta against the previous bucket. Null (false) when either end is absent, when the
  // counter went backwards, or when the jump is implausible -- all of which mean a meter swap,
  // a register remap or a rollover rather than real energy.
  static bool delta(uint32_t prev, uint32_t cur, uint32_t max_wh, uint32_t *out) {
    if (prev == WH_ABSENT || cur == WH_ABSENT || cur < prev) return false;
    uint32_t d = cur - prev;
    if (d > max_wh) return false;
    if (out) *out = d;
    return true;
  }
};

template<class Flash>
class HistoryStore {
 public:
  struct Meta {
    uint16_t sectors{0};
    uint16_t cur_sec{0}, oldest_sec{0};
    uint16_t slot{0};            // next free slot in cur_sec
    uint32_t seq{0};
    uint32_t count{0};           // records currently stored
    uint32_t oldest_qh{0}, newest_qh{0};   // 0 = unknown
    uint32_t crc_errors{0}, erases{0}, writes{0};
  };

  explicit HistoryStore(Flash &f) : flash_(f) {}

  bool ok() const { return ok_; }
  const Meta &meta() const { return meta_; }
  const HistRecord *last() const { return has_last_ ? &last_ : nullptr; }
  uint32_t last_addr() const { return last_addr_; }

  uint32_t slot_addr(uint16_t sec, uint16_t slot) const {
    return (uint32_t) sec * HIST_SECTOR + HIST_HDR + (uint32_t) slot * HIST_REC;
  }

  bool begin() {
    ok_ = false;
    uint32_t sectors = flash_.size() / HIST_SECTOR;
    if (sectors < 2) return false;
    if (sectors > 0xFFFF) sectors = 0xFFFF;
    meta_ = Meta{};
    meta_.sectors = (uint16_t) sectors;
    sectors_ = (uint16_t) sectors;

    // Elect the newest sector. An erased header reads 0xFFFFFFFF, which *is* the numeric maximum,
    // so a bare max() would happily crown an interrupted erase and then overwrite live history --
    // magic, version, crc and the seq bounds are all load-bearing here.
    uint32_t best = 0;
    int32_t cur = -1;
    for (uint16_t s = 0; s < sectors_; s++) {
      HistSecHdr h;
      if (!read_hdr_(s, h) || !hdr_valid_(h)) continue;
      if (h.seq > best) { best = h.seq; cur = s; }
    }
    if (cur < 0) return format_fresh_();

    meta_.cur_sec = (uint16_t) cur;
    meta_.seq = best;

    // Oldest = the far end of the longest run of strictly consecutive seq walking backwards.
    // min(seq) would be equivalent on an intact log but picks up garbage across a hole left by an
    // interrupted erase.
    uint16_t oldest = (uint16_t) cur;
    for (uint16_t k = 1; k < sectors_; k++) {
      uint16_t s = (uint16_t) ((cur + sectors_ - k) % sectors_);
      HistSecHdr h;
      if (!read_hdr_(s, h) || !hdr_valid_(h) || h.seq != best - k) break;
      oldest = s;
    }
    meta_.oldest_sec = oldest;
    meta_.slot = find_slot_(meta_.cur_sec);
    uint32_t full = (uint32_t) ((meta_.cur_sec + sectors_ - oldest) % sectors_);
    meta_.count = full * HIST_SLOTS + meta_.slot;
    load_last_();
    scan_bounds_();
    ok_ = true;
    return true;
  }

  bool append(const HistRecord &rec) {
    if (!ok_) return false;
    if (meta_.slot >= HIST_SLOTS && !advance_sector_(rec)) return false;
    HistRecord r = rec;
    record_finalize(r);
    uint32_t a = slot_addr(meta_.cur_sec, meta_.slot);
    if (!flash_.write(a, &r, 16)) return false;   // phase 1
    if (r.qh_tag != QH_UNKNOWN) {
      uint32_t tag = r.qh_tag;   // copy out: &r.qh_tag would be an unaligned pointer into a packed struct
      if (!flash_.write(a + 16, &tag, 4)) return false;   // phase 2
    }
    last_ = r;
    has_last_ = true;
    last_addr_ = a;
    meta_.slot++;
    meta_.count++;
    meta_.writes++;
    uint32_t qh;
    if (qh_tag_parse(r.qh_tag, &qh, nullptr)) {
      meta_.newest_qh = qh;
      if (!meta_.oldest_qh) meta_.oldest_qh = qh;
    }
    // Pre-arm the recycle so the erase never has to happen on the append path: the caller picks a
    // quiet moment via service_erase() (a 4 kB erase runs with the flash cache off and would
    // otherwise clobber a meter frame -- see the comment in gplug_smi.cpp).
    if (meta_.slot >= HIST_SLOTS) pending_erase_ = (uint16_t) ((meta_.cur_sec + 1) % sectors_);
    return true;
  }

  bool pending_erase(uint16_t *sec_out = nullptr) const {
    if (pending_erase_ == NO_SEC) return false;
    if (sec_out) *sec_out = pending_erase_;
    return true;
  }

  bool service_erase() {
    if (pending_erase_ == NO_SEC) return false;
    uint16_t n = pending_erase_;
    pending_erase_ = NO_SEC;
    if (!prepare_sector_(n, meta_.seq + 1, meta_.newest_qh)) return false;
    prepared_sec_ = n;
    return true;
  }

  // Fill in the timestamps of `n` consecutive records starting at `first_addr`, assigning
  // qh_first + i. Only slots whose qh_tag is still erased are touched (programming 1 -> 0 is legal;
  // rewriting is not), so this is safe to re-run and cannot corrupt an already-dated record.
  uint16_t patch_qh_seq(uint32_t first_addr, uint16_t n, uint32_t qh_first, bool estimated) {
    if (!ok_) return 0;
    uint16_t sec = (uint16_t) (first_addr / HIST_SECTOR);
    uint16_t slot = (uint16_t) ((first_addr % HIST_SECTOR - HIST_HDR) / HIST_REC);
    uint16_t done = 0;
    for (uint16_t i = 0; i < n; i++) {
      if (slot >= HIST_SLOTS) { sec = (uint16_t) ((sec + 1) % sectors_); slot = 0; }
      uint32_t a = slot_addr(sec, slot);
      uint32_t cur_tag = QH_UNKNOWN;
      if (flash_.read(a + 16, &cur_tag, 4) && cur_tag == QH_UNKNOWN) {
        uint32_t tag = qh_tag_make(qh_first + i, estimated);
        if (tag != QH_UNKNOWN && flash_.write(a + 16, &tag, 4)) {
          done++;
          if (a == last_addr_) last_.qh_tag = tag;
          meta_.newest_qh = qh_first + i;
          if (!meta_.oldest_qh) meta_.oldest_qh = qh_first;
        }
      }
      slot++;
    }
    return done;
  }

  // One whole sector, raw, for a scan that must not hold the store's lock across a network write
  // (the CSV export): the caller locks, copies 4 kB, unlocks, then formats at leisure. `limit` is
  // the number of slots that can hold data (the current sector is only filled up to meta_.slot),
  // `seq` the sector's sequence number so the caller can tell a sector recycled after its
  // snapshot was taken. Returns false for a sector without a valid header.
  bool read_sector(uint16_t sec, uint8_t *buf4k, uint16_t *limit, uint32_t *seq) {
    if (!ok_ || sec >= sectors_) return false;
    if (!flash_.read(sec_base_(sec), buf4k, HIST_SECTOR)) return false;
    HistSecHdr h;
    memcpy(&h, buf4k, HIST_HDR);
    if (!hdr_valid_(h)) return false;
    *limit = (sec == meta_.cur_sec) ? meta_.slot : (uint16_t) HIST_SLOTS;
    *seq = h.seq;
    return true;
  }

  // Oldest -> newest. `buf`/`buf_len` is caller-provided scratch (never a stack local on the httpd
  // task); fn is called as fn(const HistRecord&, uint32_t qh, bool have_qh).
  //
  // `from_qh` (0 = scan everything) skips whole sectors that end before the window, using the
  // header hint -- this is what keeps a "last 24 h" query down to a couple of sector reads instead
  // of the whole partition. The last sector that starts at or before the window is kept, so the
  // caller still sees the record it needs to compute the first counter delta against.
  template<class Fn>
  void for_each(uint8_t *buf, size_t buf_len, uint32_t from_qh, Fn &&fn) {
    if (!ok_ || buf_len < HIST_REC) return;
    size_t per_chunk = buf_len / HIST_REC;
    uint32_t sec_count = (uint32_t) ((meta_.cur_sec + sectors_ - meta_.oldest_sec) % sectors_) + 1;
    uint32_t skip = 0;
    if (from_qh) {
      for (uint32_t k = 1; k < sec_count; k++) {
        HistSecHdr h;
        uint16_t s = (uint16_t) ((meta_.oldest_sec + k) % sectors_);
        if (!read_hdr_(s, h) || !hdr_valid_(h) || !h.first_qh || h.first_qh > from_qh) break;
        skip = k;
      }
    }
    sec_count -= skip;
    for (uint32_t k = 0; k < sec_count; k++) {
      uint16_t sec = (uint16_t) ((meta_.oldest_sec + skip + k) % sectors_);
      uint16_t limit = (sec == meta_.cur_sec) ? meta_.slot : (uint16_t) HIST_SLOTS;
      for (uint16_t s = 0; s < limit;) {
        size_t want = limit - s;
        if (want > per_chunk) want = per_chunk;
        if (!flash_.read(slot_addr(sec, s), buf, want * HIST_REC)) return;
        for (size_t i = 0; i < want; i++) {
          HistRecord r;
          memcpy(&r, buf + i * HIST_REC, HIST_REC);
          if (record_erased(r)) continue;
          if (!record_valid(r)) { meta_.crc_errors++; continue; }
          uint32_t qh = 0;
          bool have = qh_tag_parse(r.qh_tag, &qh, nullptr);
          fn(r, qh, have);
        }
        s = (uint16_t) (s + want);
      }
    }
  }

 private:
  static constexpr uint16_t NO_SEC = 0xFFFF;

  uint32_t sec_base_(uint16_t s) const { return (uint32_t) s * HIST_SECTOR; }

  bool read_hdr_(uint16_t s, HistSecHdr &h) { return flash_.read(sec_base_(s), &h, HIST_HDR); }

  static uint16_t hdr_crc_(const HistSecHdr &h) {
    uint8_t b[HIST_HDR];
    memcpy(b, &h, HIST_HDR);
    b[6] = b[7] = 0;
    return crc16_ccitt(b, HIST_HDR);
  }

  static bool hdr_valid_(const HistSecHdr &h) {
    if (h.magic != HIST_MAGIC || h.version != HIST_VERSION) return false;
    if (h.seq == 0 || h.seq == 0xFFFFFFFFu) return false;
    return h.crc == hdr_crc_(h);
  }

  bool write_hdr_(uint16_t s, uint32_t seq, uint32_t first_qh) {
    HistSecHdr h;
    h.magic = HIST_MAGIC;
    h.version = HIST_VERSION;
    h.rsvd = 0;
    h.crc = 0;
    h.seq = seq;
    h.first_qh = first_qh;
    h.crc = hdr_crc_(h);
    return flash_.write(sec_base_(s), &h, HIST_HDR);
  }

  // Erase one sector and stamp its header. Never bulk-erase the whole partition: 176 sectors would
  // block the loop for ~7 s and drop meter frames the entire time.
  bool prepare_sector_(uint16_t s, uint32_t seq, uint32_t first_qh) {
    if (!flash_.erase(sec_base_(s), HIST_SECTOR)) return false;
    meta_.erases++;
    if (!write_hdr_(s, seq, first_qh)) return false;
    if (s == meta_.oldest_sec && meta_.count) {
      meta_.oldest_sec = (uint16_t) ((s + 1) % sectors_);
      if (meta_.count >= HIST_SLOTS) meta_.count -= HIST_SLOTS;
      meta_.oldest_qh = 0;
      scan_bounds_();
    }
    return true;
  }

  bool format_fresh_() {
    meta_ = Meta{};
    meta_.sectors = sectors_;
    if (!prepare_sector_(0, 1, 0)) return false;
    meta_.cur_sec = 0;
    meta_.oldest_sec = 0;
    meta_.slot = 0;
    meta_.seq = 1;
    meta_.count = 0;
    has_last_ = false;
    ok_ = true;
    return true;
  }

  bool advance_sector_(const HistRecord &next) {
    uint16_t n = (uint16_t) ((meta_.cur_sec + 1) % sectors_);
    uint32_t qh = 0;
    qh_tag_parse(next.qh_tag, &qh, nullptr);
    if (prepared_sec_ != n) {
      if (!prepare_sector_(n, meta_.seq + 1, qh)) return false;
    }
    prepared_sec_ = NO_SEC;
    pending_erase_ = NO_SEC;
    meta_.cur_sec = n;
    meta_.slot = 0;
    meta_.seq++;
    return true;
  }

  // Backwards chunk scan for the last written slot. "First all-0xFF slot" is the obvious rule and
  // is wrong: a torn write or an interrupted erase leaves holes, and resuming inside one would
  // program over non-virgin bytes (which esp_partition_write does silently, producing garbage).
  uint16_t find_slot_(uint16_t sec) {
    uint8_t buf[256];
    const uint32_t base = sec_base_(sec) + HIST_HDR;
    const uint32_t span = (uint32_t) (HIST_SLOTS * HIST_REC);
    uint32_t chunks = (span + sizeof(buf) - 1) / sizeof(buf);
    for (uint32_t c = chunks; c-- > 0;) {
      uint32_t off = c * (uint32_t) sizeof(buf);
      size_t len = (size_t) ((off + sizeof(buf) <= span) ? sizeof(buf) : span - off);
      if (!flash_.read(base + off, buf, len)) return 0;
      for (size_t i = len; i-- > 0;) {
        if (buf[i] == 0xFF) continue;
        uint32_t slot = (off + (uint32_t) i) / HIST_REC + 1;
        return (uint16_t) (slot > HIST_SLOTS ? HIST_SLOTS : slot);
      }
    }
    return 0;
  }

  void load_last_() {
    has_last_ = false;
    uint16_t sec = meta_.cur_sec;
    int32_t slot = (int32_t) meta_.slot - 1;
    for (uint32_t tries = 0; tries < HIST_SLOTS * 2; tries++) {
      if (slot < 0) {
        if (sec == meta_.oldest_sec) return;
        sec = (uint16_t) ((sec + sectors_ - 1) % sectors_);
        slot = (int32_t) HIST_SLOTS - 1;
      }
      HistRecord r;
      uint32_t a = slot_addr(sec, (uint16_t) slot);
      if (flash_.read(a, &r, HIST_REC) && !record_erased(r) && record_valid(r)) {
        last_ = r;
        last_addr_ = a;
        has_last_ = true;
        uint32_t qh;
        if (qh_tag_parse(r.qh_tag, &qh, nullptr)) meta_.newest_qh = qh;
        return;
      }
      slot--;
    }
  }

  void scan_bounds_() {
    meta_.oldest_qh = 0;
    for (uint16_t s = 0; s < 16 && s < HIST_SLOTS; s++) {
      HistRecord r;
      if (!flash_.read(slot_addr(meta_.oldest_sec, s), &r, HIST_REC)) return;
      if (record_erased(r) || !record_valid(r)) continue;
      uint32_t qh;
      if (qh_tag_parse(r.qh_tag, &qh, nullptr)) { meta_.oldest_qh = qh; return; }
    }
  }

  Flash &flash_;
  Meta meta_{};
  HistRecord last_{};
  uint32_t last_addr_{0};
  uint16_t sectors_{0};
  uint16_t pending_erase_{NO_SEC};
  uint16_t prepared_sec_{NO_SEC};
  bool has_last_{false};
  bool ok_{false};
};

}  // namespace gplug_hist
