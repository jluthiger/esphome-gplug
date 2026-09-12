// CSV view of the 15-min history ("Lastgang"): one row per stored record at native resolution,
// nothing bucketed, nothing dropped. What a grid operator's bill is checked against and what a
// ZEV/LEG settlement is computed from, so the rules here err on the side of leaving a cell empty
// over printing a number that looks right but isn't.
//
// Header-only and free of ESPHome/IDF like history_store.h, so test/test_csv.cpp pins the exact
// text on the host. The caller supplies local-time formatting (the device uses ESPHome's timezone,
// the test uses UTC).
//
// Format (one header row, `;` separated, CRLF, integers only so no locale can misread a number):
//   von;bis                     local time "YYYY-MM-DD HH:MM", interval start / end; empty when
//                               the record was written before the clock ever synced
//   bezug_zaehler_wh            meter counter 1.8.0 at the end of the interval, Wh
//   einspeisung_zaehler_wh      meter counter 2.8.0 at the end of the interval, Wh
//   bezug_wh, einspeisung_wh    energy in this interval = counter minus the previous record's, only
//                               when that record is the directly preceding interval and the chain
//                               is intact; empty otherwise (never a multi-interval delta squeezed
//                               into one row)
//   p_avg_w, p_min_w, p_max_w   net power over the interval (import positive), from the 10 s samples
//   hinweise                    comma-separated quality tokens, empty = clean interval
//
// Second shape, CSV_CKW: the grid operator CKW's own customer-portal export, mirrored column for
// column so the two files diff line by line: tab separated, "Zeitraum" = interval start as
// "DD.MM.YY HH:MM", one energy column in kWh with three decimals, Bezug or Einspeisung per file.
// Undated records are left out (no Zeitraum to align on); an interval whose energy cannot be
// attributed (see above) keeps its row with an empty value, so the alignment holds.
#pragma once
#include "history_store.h"
#include <cstdio>
#include <string>

namespace gplug_hist {

enum CsvFormat : uint8_t {
  CSV_FULL,              // every column, above
  CSV_CKW_BEZUG,         // CKW portal shape, import energy
  CSV_CKW_EINSPEISUNG,   // CKW portal shape, export energy
};

inline const char *csv_header(CsvFormat f) {
  switch (f) {
    case CSV_CKW_BEZUG: return "Zeitraum\tEnergieverbrauch (kWh)\r\n";
    case CSV_CKW_EINSPEISUNG: return "Zeitraum\tEnergieeinspeisung (kWh)\r\n";
    default: return "von;bis;bezug_zaehler_wh;einspeisung_zaehler_wh;bezug_wh;einspeisung_wh;p_avg_w;p_min_w;p_max_w;hinweise\r\n";
  }
}
static constexpr uint32_t CSV_MAX_DELTA_WH = 7500;   // > 30 kW average over a quarter hour: not real
static constexpr size_t CSV_ROW_MAX = 180;           // upper bound of one row (every token set), for chunk sizing

// Carried from row to row: the previous record's counters and interval, for the deltas.
struct CsvState {
  uint32_t prev_ei{WH_ABSENT}, prev_eo{WH_ABSENT};
  uint32_t prev_qh{0};
  bool have_prev{false};      // a previous record exists at all
  bool have_prev_qh{false};   // ... and it carried a timestamp
};

// Formats one record, or -- with `out` null -- only advances the state (records before an export
// window still carry the counter the first row's delta is measured against). `local_time` is
// called as local_time(uint32_t epoch, char *out, size_t out_len) and writes "YYYY-MM-DD HH:MM".
// "YYYY-MM-DD HH:MM" -> "DD.MM.YY HH:MM" (CKW's Zeitraum), in place of the same buffer.
inline void csv_ckw_time(const char *iso, char *out, size_t n) {
  snprintf(out, n, "%.2s.%.2s.%.2s %.5s", iso + 8, iso + 5, iso + 2, iso + 11);
}

template<class Fn>
inline void csv_row(std::string *out, const HistRecord &r, uint32_t qh, bool have_qh, bool estimated, CsvState &st,
                    Fn &&local_time, CsvFormat fmt = CSV_FULL) {
  // Is the previous record the directly preceding interval? With timestamps that is a qh check.
  // Without (pre-sync records of one boot) they were closed back to back on the millis() cadence,
  // unless a reboot sits in between.
  bool contiguous = st.have_prev &&
                    ((have_qh && st.have_prev_qh && qh == st.prev_qh + 1) ||
                     (!have_qh && !st.have_prev_qh && !(r.flags & HF_BOOT_BEFORE)));
  bool gap = st.have_prev && !contiguous;

  if (out && fmt != CSV_FULL) {
    if (have_qh) {
      std::string &s = *out;
      char iso[20], t[20];
      local_time(HIST_EPOCH + qh * HIST_INTERVAL_S, iso, sizeof iso);
      csv_ckw_time(iso, t, sizeof t);
      s += t;
      s += '\t';
      bool chain = contiguous && !(r.flags & HF_CONFIG_CHANGE);
      uint32_t d;
      bool is_ei = fmt == CSV_CKW_BEZUG;
      if (chain && HistBucket::delta(is_ei ? st.prev_ei : st.prev_eo, is_ei ? r.ei_wh : r.eo_wh, CSV_MAX_DELTA_WH, &d)) {
        snprintf(t, sizeof t, "%u.%03u", (unsigned) (d / 1000), (unsigned) (d % 1000));
        s += t;
      }
      s += "\r\n";
    }
  } else if (out) {
    std::string &s = *out;
    char t[20];
    if (have_qh) {
      local_time(HIST_EPOCH + qh * HIST_INTERVAL_S, t, sizeof t);
      s += t;
      s += ';';
      local_time(HIST_EPOCH + (qh + 1) * HIST_INTERVAL_S, t, sizeof t);
      s += t;
    } else {
      s += ';';
    }
    s += ';';
    if (r.ei_wh != WH_ABSENT) s += std::to_string(r.ei_wh);
    s += ';';
    if (r.eo_wh != WH_ABSENT) s += std::to_string(r.eo_wh);
    s += ';';
    bool chain = contiguous && !(r.flags & HF_CONFIG_CHANGE);
    uint32_t d;
    if (chain && HistBucket::delta(st.prev_ei, r.ei_wh, CSV_MAX_DELTA_WH, &d)) s += std::to_string(d);
    s += ';';
    if (chain && HistBucket::delta(st.prev_eo, r.eo_wh, CSV_MAX_DELTA_WH, &d)) s += std::to_string(d);
    s += ';';
    s += std::to_string(r.p_avg);
    s += ';';
    s += std::to_string(r.p_min);
    s += ';';
    s += std::to_string(r.p_max);
    s += ';';
    const char *sep = "";
    auto note = [&](const char *tok) { s += sep; s += tok; sep = ","; };
    if (!have_qh) note("zeit_unbekannt");
    else if (estimated) note("zeit_geschaetzt");
    if (gap) note("luecke_davor");
    if (r.flags & HF_BOOT_BEFORE) note("neustart");
    if (r.flags & HF_CONFIG_CHANGE) note("konfig_geaendert");
    if (r.flags & HF_PARTIAL) note("teilintervall");
    if (r.flags & HF_NO_DATA) note("keine_daten");
    if (r.flags & HF_LOCAL_ENERGY) note("integriert");
    s += "\r\n";
  }

  st.prev_ei = r.ei_wh;
  st.prev_eo = r.eo_wh;
  st.prev_qh = qh;
  st.have_prev = true;
  st.have_prev_qh = have_qh;
}

}  // namespace gplug_hist
