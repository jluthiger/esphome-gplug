// Dynamic tariffs (issue 51): the price per quarter hour as published by a Swiss utility, parsed
// from the provider's JSON while it streams in, and the small table the device keeps them in.
//
// Two providers, two different tariff components -- which is why the UI never calls them "spot
// price" without saying which:
//   BKW  https://api.bkw.ch/api/dyntariffs/v1/Tariffs/energyreturn            feed-in remuneration
//        {"publication_timestamp":"…Z","prices":[{"start_timestamp":"2026-11-12T23:00:00Z",
//         "end_timestamp":…,"feed_in":[{"unit":"CHF_kWh","value":0.2}]},…]}
//        UTC ISO timestamps, numeric value, today's local day only (the endpoint refuses any query
//        parameter, checked 2026-11-13).
//   CKW  https://www.ckw.ch/api/network/tariff?start=YYYY-MM-DD&tariffType=grid_usage&tariffName=home_dynamic
//        {"data":[{"publication":"12.11.2026 10:19:27","start":"13.11.2026 00:00:00","end":…,
//         "unit":"CHF_kWh","value":"0.0347"},…]}                                   grid usage
//        Europe/Zurich wall time as DD.MM.YYYY HH:MM:SS, value as a string, one local day per
//        request; past days answer too, tomorrow from ~10:20, later days with {"data":[]}.
//   STH  https://swisstariffhub.ch/api/v1/prices?tariff_id=<id>&start_time=<local midnight, UTC>
//        with header X-API-Key   {"tariff_id":…,"start_time":…,"slot_count":96,
//         "energy_price_utc":[{"2026-11-12T23:00:00+00:00":0.12},…],"grid_price_utc":[…],
//         "residual_price_utc":[…]}                                     total of the three
//        Swiss Tariff Hub, a third party collecting several utilities' dynamic tariffs into one
//        format. Needs a key (free plan: one tariff, 100 requests a day, the last 7 days); the
//        timestamp is the object's key, the price its value. The three components are added up
//        per quarter hour (the hub's own example sums them to its total); a quarter hour missing
//        from any of the three is left out rather than shown too cheap. Its "residual" equals CKW's
//        own dynamic grid-usage value (checked 2026-11-13).
//
// Neither response is buffered: the fetch task hands each received chunk to TariffParser::feed(),
// which keeps only the current key, one scalar and the slot being assembled. That is not a JSON
// parser -- it does not check the structure -- but both shapes are flat enough that "the last
// start time seen, then a unit, then a value" is a slot, and anything else is counted as bad
// rather than guessed at.
//
// CKW sends 96 slots on a naive local grid even on the two days that do not have 96 quarter hours
// (checked on its 2026-03-29 and 2026-10-25 answers): in spring the 03:xx hour comes twice and
// 02:xx not at all, in autumn the 02:xx hour comes once. Each slot is placed at the UTC time its
// wall time means; a wall time that would land on or before the slot already placed is dropped
// (spring: the second 03:xx block), and the autumn hour that is never sent stays without a price.
// Guessing which of CKW's blocks is the "real" one would be inventing data.
//
// Prices are int32 micro-CHF per kWh: CKW's four decimals and BKW's three are exact, and +-2147 CHF
// is far beyond any tariff. Slots are quarter-hour indices since 2020-01-01Z, the same index the
// 15-min history uses, so a price and a stored energy record meet on one number.
//
// Europe/Zurich is built in (EU rule: summer time from the last Sunday of March to the last Sunday
// of October, 01:00 UTC both), like the `time:` timezone in gplug.yaml -- every provider here is
// Swiss, and a host test cannot use the device's TZ database.
//
// Header-only, no ESPHome or IDF dependency, tested in test/test_tariff.cpp.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace gplug_tariff {

static constexpr uint32_t QH_EPOCH = 1577836800u;   // 2020-01-01T00:00:00Z, = gplug_hist::HIST_EPOCH
static constexpr uint32_t QH_S = 900;
static constexpr int32_t PRICE_NONE = INT32_MIN;
static constexpr int64_t PRICE_LIMIT = 100000000;   // 100 CHF/kWh in micro-CHF: beyond is garbage

enum Provider : uint8_t { P_OFF = 0, P_BKW = 1, P_CKW = 2, P_STH = 3 };

inline const char *provider_id(Provider p) {
  return p == P_BKW ? "bkw" : p == P_CKW ? "ckw" : p == P_STH ? "sth" : "off";
}
inline bool provider_from_id(const char *s, Provider &out) {
  if (!strcmp(s, "off")) out = P_OFF;
  else if (!strcmp(s, "bkw")) out = P_BKW;
  else if (!strcmp(s, "ckw")) out = P_CKW;
  else if (!strcmp(s, "sth")) out = P_STH;
  else return false;
  return true;
}

// Swiss Tariff Hub settings: the tariff to fetch and the account's key. Both end up in a URL or a
// header, so only characters that need no escaping there are accepted.
static constexpr size_t STH_ID_MAX = 48, STH_KEY_MAX = 64;
inline bool token_ok(const char *s, size_t max) {
  size_t n = 0;
  for (; s[n]; n++) {
    char c = s[n];
    if (n >= max || !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
      return false;
  }
  return n > 0;
}

// One string field of a flat JSON body (POST /api/config/tariff), copied into out[cap]. Returns
// 1 found, 0 absent, -1 malformed or too long. A few lines instead of ArduinoJson: tariff.cpp is a
// translation unit of its own, and deserializeJson() there instantiated the whole parser a second
// time (1.1 kB of image, measured 2026-11-13). The fields read here never contain escapes.
inline int json_str_field(const char *b, const char *name, char *out, size_t cap) {
  char pat[24];
  size_t nl = strlen(name);
  if (nl + 3 > sizeof pat) return -1;
  pat[0] = '"'; memcpy(pat + 1, name, nl); pat[nl + 1] = '"'; pat[nl + 2] = 0;
  const char *k = strstr(b, pat);
  if (!k) return 0;
  const char *p = k + nl + 2;
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
  if (*p++ != ':') return -1;
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
  if (*p++ != '"') return -1;
  size_t n = 0;
  while (*p && *p != '"' && *p != '\\') { if (n >= cap - 1) return -1; out[n++] = *p++; }
  if (*p != '"') return -1;
  out[n] = 0;
  return 1;
}

inline bool provider_from_body(const char *b, Provider &out) {
  char id[8];
  return json_str_field(b, "provider", id, sizeof id) == 1 && provider_from_id(id, out);
}

// ---------------------------------------------------------------- calendar

// Howard Hinnant's days_from_civil / civil_from_days: proleptic Gregorian, day 0 = 1970-01-01.
inline int64_t days_from_civil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned) (y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int64_t) doe - 719468;
}

inline void civil_from_days(int64_t z, int &y, unsigned &m, unsigned &d) {
  z += 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = (unsigned) (z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1;
  m = mp < 10 ? mp + 3 : mp - 9;
  y = (int) (yoe + era * 400) + (m <= 2);
}

inline int64_t floor_div(int64_t a, int64_t b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0))); }

// 01:00 UTC on the last Sunday of the month (EU summer-time switch).
inline int64_t last_sunday_0100z(int y, unsigned m) {
  int64_t last = days_from_civil(y, m, 31);          // March and October both have 31 days
  int64_t wd = ((last % 7) + 7 + 4) % 7;             // 1970-01-01 was a Thursday; 0 = Sunday
  return (last - wd) * 86400 + 3600;
}

// Europe/Zurich offset from UTC at the instant `utc`, in seconds.
inline int32_t zurich_offset(int64_t utc) {
  int y; unsigned m, d;
  civil_from_days(floor_div(utc, 86400), y, m, d);
  return (utc >= last_sunday_0100z(y, 3) && utc < last_sunday_0100z(y, 10)) ? 7200 : 3600;
}

// UTC instant of local midnight starting the local day that contains `utc`.
inline int64_t local_midnight(int64_t utc) {
  int64_t local = utc + zurich_offset(utc);
  int64_t day0 = floor_div(local, 86400) * 86400;    // local 00:00 as if it were UTC
  // The offset at midnight itself, not at `utc`: the two differ on a switch day after 01:00 UTC.
  int64_t guess = day0 - zurich_offset(day0 - 3600);
  return guess;
}

// Local midnight `n` days after (or before) the local midnight `midnight`. A day is 23, 24 or 25 h
// long, so step a whole day plus 3 h into the target day and snap back to its midnight.
inline int64_t add_local_days(int64_t midnight, int n) { return local_midnight(midnight + (int64_t) n * 86400 + 3 * 3600); }

// The local calendar date of a UTC instant, for CKW's ?start=YYYY-MM-DD.
inline void local_date(int64_t utc, int &y, unsigned &m, unsigned &d) {
  civil_from_days(floor_div(utc + zurich_offset(utc), 86400), y, m, d);
}

// Wall time in Europe/Zurich -> UTC. Of the readings a wall time can have (none in the spring gap,
// two in the repeated autumn hour) the earliest one at or after `not_before` is taken; when every
// reading is earlier, the latest, which the caller then sees as out of order.
inline bool local_to_utc(int y, unsigned mo, unsigned d, unsigned hh, unsigned mi, unsigned ss,
                         int64_t not_before, int64_t &out) {
  int64_t wall = days_from_civil(y, mo, d) * 86400 + hh * 3600 + mi * 60 + ss;
  int64_t cand[2] = {wall - 7200, wall - 3600};     // summer reading first: it is the earlier instant
  bool ok[2] = {zurich_offset(cand[0]) == 7200, zurich_offset(cand[1]) == 3600};
  int last = -1;
  for (int i = 0; i < 2; i++) {
    if (!ok[i]) continue;
    if (cand[i] >= not_before) { out = cand[i]; return true; }
    last = i;
  }
  if (last < 0) return false;
  out = cand[last];
  return true;
}

inline uint32_t qh_of(int64_t utc) { return utc > (int64_t) QH_EPOCH ? (uint32_t) ((utc - QH_EPOCH) / QH_S) : 0; }
inline int64_t utc_of_qh(uint32_t qh) { return (int64_t) QH_EPOCH + (int64_t) qh * QH_S; }

// ---------------------------------------------------------------- scalars

inline bool digits_(const char *&p, int n, unsigned &out) {
  out = 0;
  for (int i = 0; i < n; i++, p++) {
    if (*p < '0' || *p > '9') return false;
    out = out * 10 + (unsigned) (*p - '0');
  }
  return true;
}

// "2026-11-12T23:00:00Z" (BKW) or "…+00:00" (Swiss Tariff Hub). Only UTC is accepted: any other
// offset would mean the provider changed format, and reading it as UTC would shift every price.
inline bool parse_iso_utc(const char *s, int64_t &out) {
  unsigned y, mo, d, hh, mi, ss;
  const char *p = s;
  if (!digits_(p, 4, y) || *p++ != '-' || !digits_(p, 2, mo) || *p++ != '-' || !digits_(p, 2, d) ||
      *p++ != 'T' || !digits_(p, 2, hh) || *p++ != ':' || !digits_(p, 2, mi) || *p++ != ':' ||
      !digits_(p, 2, ss))
    return false;
  if (*p == '.') { p++; while (*p >= '0' && *p <= '9') p++; }
  if (*p == 'Z') p++;
  else if (!strcmp(p, "+00:00")) p += 6;
  else return false;
  if (*p) return false;
  if (mo < 1 || mo > 12 || d < 1 || d > 31 || hh > 23 || mi > 59 || ss > 59) return false;
  out = days_from_civil((int) y, mo, d) * 86400 + hh * 3600 + mi * 60 + ss;
  return true;
}

// "13.11.2026 00:00:00" (CKW), Europe/Zurich wall time.
inline bool parse_ckw_local(const char *s, int64_t not_before, int64_t &out) {
  unsigned y, mo, d, hh, mi, ss;
  const char *p = s;
  if (!digits_(p, 2, d) || *p++ != '.' || !digits_(p, 2, mo) || *p++ != '.' || !digits_(p, 4, y) ||
      *p++ != ' ' || !digits_(p, 2, hh) || *p++ != ':' || !digits_(p, 2, mi) || *p++ != ':' ||
      !digits_(p, 2, ss) || *p)
    return false;
  if (mo < 1 || mo > 12 || d < 1 || d > 31 || hh > 23 || mi > 59 || ss > 59) return false;
  return local_to_utc((int) y, mo, d, hh, mi, ss, not_before, out);
}

// Decimal (optionally with exponent, as .NET may print one) -> micro-units, rounded half away
// from zero. "0.0347" -> 34700, "-0.012" -> -12000, "1E-05" -> 10.
inline bool parse_micro(const char *s, int32_t &out) {
  const char *p = s;
  bool neg = false;
  if (*p == '-') { neg = true; p++; }
  int64_t mant = 0;
  int exp = 0, sig = 0;
  bool any = false, dot = false;
  for (;; p++) {
    if (*p >= '0' && *p <= '9') {
      any = true;
      if (sig < 15) { mant = mant * 10 + (*p - '0'); if (mant) sig++; if (dot) exp--; }
      else if (!dot) exp++;   // digits beyond 15 significant ones before the point still scale
    } else if (*p == '.' && !dot) {
      dot = true;
    } else {
      break;
    }
  }
  if (!any) return false;
  if (*p == 'e' || *p == 'E') {
    p++;
    bool eneg = false;
    if (*p == '+' || *p == '-') eneg = *p++ == '-';
    if (*p < '0' || *p > '9') return false;
    int e = 0;
    while (*p >= '0' && *p <= '9') { e = e * 10 + (*p++ - '0'); if (e > 400) return false; }
    exp += eneg ? -e : e;
  }
  if (*p) return false;
  exp += 6;   // to micro-units
  if (mant == 0) { out = 0; return true; }
  if (exp > 0) {
    for (int i = 0; i < exp; i++) { mant *= 10; if (mant > PRICE_LIMIT) return false; }
  } else if (exp < 0) {
    if (exp < -18) { mant = 0; }
    else {
      int64_t div = 1;
      for (int i = 0; i < -exp; i++) div *= 10;
      mant = (mant + div / 2) / div;
    }
  }
  if (mant > PRICE_LIMIT) return false;
  out = (int32_t) (neg ? -mant : mant);
  return true;
}

// ---------------------------------------------------------------- streaming parser

struct Slot { uint32_t qh; int32_t price; };

// One response's slots, collected before they replace anything in the table, so a response cut
// off half way leaves the prices the device already had. 128 covers a CKW day (96) and BKW's
// today + tomorrow (<= 200 is not reached: it sends one day, see above); more is an error.
static constexpr size_t PARSE_MAX = 128;

class TariffParser {
 public:
  enum Err : uint8_t { OK = 0, E_EMPTY, E_UNIT, E_OVERFLOW, E_SYNTAX };

  explicit TariffParser(Provider p) : prov_(p) {}

  void feed(const char *data, size_t n) {
    for (size_t i = 0; i < n; i++) this->feed_(data[i]);
  }
  // End of body: a literal still being read (a value at the very end) is complete now. For the
  // hub, a quarter hour that did not get all three components is dropped here.
  void finish() {
    if (st_ == S_LIT) { this->scalar_(false); st_ = S_IDLE; }
    if (prov_ != P_STH) return;
    size_t k = 0;
    for (size_t i = 0; i < n_; i++) {
      if (comps_[i] == 7) slots_[k++] = slots_[i];
      else bad_++;
    }
    n_ = k;
  }

  // The verdict on the whole response.
  Err result() const {
    if (unit_bad_) return E_UNIT;
    if (overflow_) return E_OVERFLOW;
    if (n_ == 0) return bad_ ? E_SYNTAX : E_EMPTY;
    return OK;
  }

  const Slot *slots() const { return slots_; }
  size_t count() const { return n_; }
  uint16_t bad() const { return bad_; }               // slots without a usable time or price
  uint16_t dropped() const { return dropped_; }       // out of order: CKW's repeated spring hour
  const char *publication() const { return pub_; }

 private:
  enum State : uint8_t { S_IDLE, S_STR, S_ESC, S_LIT };

  void feed_(char c) {
    switch (st_) {
      case S_STR:
        if (c == '\\') { st_ = S_ESC; return; }
        if (c == '"') { st_ = S_IDLE; str_done_ = true; return; }
        this->put_(c);
        return;
      case S_ESC:
        this->put_(c);   // the escaped character itself; no field we read contains one
        st_ = S_STR;
        return;
      case S_LIT:
        if (c == ',' || c == '}' || c == ']' || c == ' ' || c == '\n' || c == '\r' || c == '\t') {
          this->scalar_(false);
          st_ = S_IDLE;
          break;   // the delimiter still needs handling below
        }
        this->put_(c);
        return;
      case S_IDLE:
        break;
    }
    if (c == ' ' || c == '\n' || c == '\r' || c == '\t') return;
    if (str_done_) {
      // A finished string is a key when a colon follows, otherwise a value.
      str_done_ = false;
      if (c == ':') { this->set_key_(); return; }
      this->scalar_(true);
    }
    if (c == '"') { st_ = S_STR; len_ = 0; trunc_ = false; return; }
    if (c == '-' || (c >= '0' && c <= '9') || c == 't' || c == 'f' || c == 'n') {
      st_ = S_LIT; len_ = 0; trunc_ = false;
      this->put_(c);
    }
  }

  void put_(char c) {
    if (len_ < sizeof(buf_) - 1) buf_[len_++] = c; else trunc_ = true;
  }

  void set_key_() {
    buf_[len_] = 0;
    size_t k = len_ < sizeof(key_) - 1 ? len_ : sizeof(key_) - 1;
    memcpy(key_, buf_, k);
    key_[k] = 0;
    if (prov_ == P_STH && strstr(key_, "_price_utc")) {
      // The array the next timestamps belong to; one the parser does not know is skipped.
      comp_ = !strcmp(key_, "energy_price_utc") ? 0 : !strcmp(key_, "grid_price_utc") ? 1
            : !strcmp(key_, "residual_price_utc") ? 2 : -1;
    }
  }

  // Hub: key = timestamp, value = one component's price. The first array seen creates the slots,
  // the others add to them.
  void sth_value_() {
    int64_t t;
    int32_t v;
    if (comp_ < 0 || !parse_iso_utc(key_, t) || (t % QH_S) != 0 || t <= (int64_t) QH_EPOCH || trunc_ || !parse_micro(buf_, v)) {
      bad_++;
      return;
    }
    uint32_t qh = qh_of(t);
    for (size_t i = 0; i < n_; i++) {
      if (slots_[i].qh != qh) continue;
      if (comps_[i] & (1 << comp_)) { dropped_++; return; }
      int64_t sum = (int64_t) slots_[i].price + v;
      if (sum > PRICE_LIMIT || sum < -PRICE_LIMIT) { bad_++; return; }
      slots_[i].price = (int32_t) sum;
      comps_[i] |= (uint8_t) (1 << comp_);
      return;
    }
    if (n_ >= PARSE_MAX) { overflow_ = true; return; }
    comps_[n_] = (uint8_t) (1 << comp_);
    slots_[n_++] = {qh, v};
  }

  bool key_is_(const char *k) const { return !strcmp(key_, k); }

  void scalar_(bool quoted) {
    buf_[len_] = 0;
    if (prov_ == P_STH) {
      if (key_[0] >= '0' && key_[0] <= '9') this->sth_value_();
      return;
    }
    const bool bkw = prov_ == P_BKW;
    if (key_is_(bkw ? "publication_timestamp" : "publication")) {
      if (!pub_[0] && !trunc_) { strncpy(pub_, buf_, sizeof(pub_) - 1); pub_[sizeof(pub_) - 1] = 0; }
    } else if (key_is_(bkw ? "start_timestamp" : "start")) {
      int64_t t;
      int64_t not_before = have_last_ ? utc_of_qh(last_qh_) + QH_S : INT64_MIN;
      bool ok = quoted && !trunc_ && (bkw ? parse_iso_utc(buf_, t) : parse_ckw_local(buf_, not_before, t));
      // An unusable start is counted once, when its value arrives without one.
      have_start_ = ok && (t % QH_S) == 0 && t > (int64_t) QH_EPOCH;
      if (have_start_) start_ = t;
      unit_ok_ = false;
    } else if (key_is_("unit")) {
      unit_ok_ = quoted && !strcmp(buf_, "CHF_kWh");
      if (!unit_ok_) unit_bad_ = true;
    } else if (key_is_("value")) {
      int32_t v;
      // BKW sends a number, CKW a string; either is read the same way.
      if (!have_start_ || !unit_ok_ || trunc_ || !parse_micro(buf_, v)) { bad_++; have_start_ = false; return; }
      this->emit_(qh_of(start_), v);
      have_start_ = false;
      unit_ok_ = false;
    }
  }

  void emit_(uint32_t qh, int32_t v) {
    if (have_last_ && qh <= last_qh_) { dropped_++; return; }
    if (n_ >= PARSE_MAX) { overflow_ = true; return; }
    slots_[n_++] = {qh, v};
    last_qh_ = qh;
    have_last_ = true;
  }

  Provider prov_;
  State st_{S_IDLE};
  bool str_done_{false};
  char buf_[40];
  size_t len_{0};
  bool trunc_{false};
  char key_[32]{};   // the hub's timestamp keys are 25 characters
  int8_t comp_{-1};
  int64_t start_{0};
  bool have_start_{false};
  bool unit_ok_{false};
  bool unit_bad_{false};
  bool overflow_{false};
  uint32_t last_qh_{0};
  bool have_last_{false};
  uint16_t bad_{0}, dropped_{0};
  char pub_[28]{};
  Slot slots_[PARSE_MAX];
  uint8_t comps_[PARSE_MAX]{};   // hub: which of the three components each slot has, bits 0..2
  size_t n_{0};
};

// ---------------------------------------------------------------- the table the device keeps

// A sliding window of quarter hours starting at base_qh. Moving the base forward drops what fell
// out and keeps the overlap, so prices outlive the day they were fetched for until the window
// passes them. RAM only: prices change once a day and are fetched again after a restart, so they
// cost the flash nothing.
template<size_t N> class TariffTable {
 public:
  TariffTable() { clear(); }
  void clear() { for (auto &x : v_) x = PRICE_NONE; }
  uint32_t base() const { return base_; }
  static constexpr size_t size() { return N; }

  void rebase(uint32_t nb) {
    if (nb == base_) return;
    if (nb > base_ && nb - base_ < N) {
      size_t sh = nb - base_;
      memmove(v_, v_ + sh, (N - sh) * sizeof(int32_t));
      for (size_t i = N - sh; i < N; i++) v_[i] = PRICE_NONE;
    } else if (nb < base_ && base_ - nb < N) {
      size_t sh = base_ - nb;
      memmove(v_ + sh, v_, (N - sh) * sizeof(int32_t));
      for (size_t i = 0; i < sh; i++) v_[i] = PRICE_NONE;
    } else {
      clear();
    }
    base_ = nb;
  }
  bool put(uint32_t qh, int32_t p) {
    if (qh < base_ || qh - base_ >= N) return false;
    v_[qh - base_] = p;
    return true;
  }
  int32_t get(uint32_t qh) const { return (qh < base_ || qh - base_ >= N) ? PRICE_NONE : v_[qh - base_]; }
  size_t count(uint32_t from, uint32_t to) const {
    size_t c = 0;
    for (uint32_t q = from; q < to; q++) c += get(q) != PRICE_NONE;
    return c;
  }
  // First and one-past-last slot holding a price; false when the table is empty.
  bool span(uint32_t &first, uint32_t &end) const {
    size_t a = 0, b = N;
    while (a < N && v_[a] == PRICE_NONE) a++;
    if (a == N) return false;
    while (b > a && v_[b - 1] == PRICE_NONE) b--;
    first = base_ + (uint32_t) a;
    end = base_ + (uint32_t) b;
    return true;
  }

 private:
  uint32_t base_{0};
  int32_t v_[N];
};

}  // namespace gplug_tariff
