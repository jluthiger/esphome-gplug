// history_csv.h: the exact text of the Lastgang export, row by row. This is the file a grid
// operator's bill is checked against, so every rule (which cells stay empty, which tokens appear)
// is pinned here rather than eyeballed on a device.
#include "history_csv.h"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
using namespace gplug_hist;

int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
#define CHECK_EQ(a, b) do { std::string _a = (a), _b = (b); if (_a != _b) { printf("FAIL %s:%d\n  got  %s\n  want %s\n", __FILE__, __LINE__, _a.c_str(), _b.c_str()); fails++; } } while (0)

// UTC stands in for the device's local timezone.
static void utc_time(uint32_t epoch, char *out, size_t n) {
  time_t t = epoch;
  struct tm tm;
  gmtime_r(&t, &tm);
  strftime(out, n, "%Y-%m-%d %H:%M", &tm);
}

static HistRecord rec(uint32_t ei, uint32_t eo, int16_t pavg, uint8_t flags = 0) {
  HistRecord r;
  r.ei_wh = ei; r.eo_wh = eo;
  r.p_min = (int16_t) (pavg - 100); r.p_max = (int16_t) (pavg + 100); r.p_avg = pavg;
  r.flags = flags;
  record_finalize(r);
  return r;
}

static std::string row(const HistRecord &r, uint32_t qh, bool have_qh, bool est, CsvState &st) {
  std::string s;
  csv_row(&s, r, qh, have_qh, est, st, utc_time);
  return s;
}

int main() {
  // 2026-09-11 14:00 UTC as a quarter-hour index
  const uint32_t QH = (1789135200u - HIST_EPOCH) / 900;   // 2026-09-11T14:00:00Z
  CHECK(HIST_EPOCH + QH * 900 == 1789135200u);

  // --- 1. header, first row (no previous record: counters yes, deltas no) ---
  {
    CHECK_EQ(std::string(csv_header()),
             "von;bis;bezug_zaehler_wh;einspeisung_zaehler_wh;bezug_wh;einspeisung_wh;p_avg_w;p_min_w;p_max_w;hinweise\r\n");
    CsvState st;
    CHECK_EQ(row(rec(1000000, 20000, 700), QH, true, false, st),
             "2026-09-11 14:00;2026-09-11 14:15;1000000;20000;;;700;600;800;\r\n");
    // --- 2. contiguous next interval: deltas from the counters ---
    CHECK_EQ(row(rec(1000180, 20000, 720), QH + 1, true, false, st),
             "2026-09-11 14:15;2026-09-11 14:30;1000180;20000;180;0;720;620;820;\r\n");
    // --- 3. a hole (device was off): counters printed, no delta, flagged ---
    CHECK_EQ(row(rec(1000900, 20050, 650, HF_BOOT_BEFORE), QH + 4, true, false, st),
             "2026-09-11 15:00;2026-09-11 15:15;1000900;20050;;;650;550;750;luecke_davor,neustart\r\n");
    // --- 4. config change breaks the chain even when contiguous ---
    CHECK_EQ(row(rec(1001000, 20050, 600, HF_CONFIG_CHANGE), QH + 5, true, false, st),
             "2026-09-11 15:15;2026-09-11 15:30;1001000;20050;;;600;500;700;konfig_geaendert\r\n");
    // --- 4b. a reboot alone (contiguous, no config change) keeps the delta: counters are absolute ---
    CHECK_EQ(row(rec(1001020, 20050, 610, HF_BOOT_BEFORE | HF_PARTIAL), QH + 6, true, false, st),
             "2026-09-11 15:30;2026-09-11 15:45;1001020;20050;20;0;610;510;710;neustart,teilintervall\r\n");
    // --- 5. partial + estimated time keep the delta, but say so ---
    CHECK_EQ(row(rec(1001050, 20050, 590, HF_PARTIAL), QH + 7, true, true, st),
             "2026-09-11 15:45;2026-09-11 16:00;1001050;20050;30;0;590;490;690;zeit_geschaetzt,teilintervall\r\n");
  }

  // --- 6. implausible jump (> 30 kW average): counter shown, delta withheld ---
  {
    CsvState st;
    row(rec(5000, 0, 100), QH, true, false, st);
    CHECK_EQ(row(rec(5000 + CSV_MAX_DELTA_WH + 1, 0, 100), QH + 1, true, false, st),
             "2026-09-11 14:15;2026-09-11 14:30;12501;0;;0;100;0;200;\r\n");
    // counter going backwards (meter swap): same
    CHECK_EQ(row(rec(4000, 0, 100), QH + 2, true, false, st),
             "2026-09-11 14:30;2026-09-11 14:45;4000;0;;0;100;0;200;\r\n");
  }

  // --- 7. absent counters (meter without 1.8.0/2.8.0, integrated locally) ---
  {
    CsvState st;
    CHECK_EQ(row(rec(WH_ABSENT, WH_ABSENT, 300, HF_LOCAL_ENERGY | HF_NO_DATA), QH, true, false, st),
             "2026-09-11 14:00;2026-09-11 14:15;;;;;300;200;400;keine_daten,integriert\r\n");
    CHECK_EQ(row(rec(WH_ABSENT, WH_ABSENT, 300, HF_LOCAL_ENERGY), QH + 1, true, false, st),
             "2026-09-11 14:15;2026-09-11 14:30;;;;;300;200;400;integriert\r\n");
  }

  // --- 8. pre-sync records: no time; back-to-back ones in one boot still get deltas ---
  {
    CsvState st;
    CHECK_EQ(row(rec(100, 0, 10), 0, false, false, st), ";;100;0;;;10;-90;110;zeit_unbekannt\r\n");
    CHECK_EQ(row(rec(130, 0, 10), 0, false, false, st), ";;130;0;30;0;10;-90;110;zeit_unbekannt\r\n");
    // a reboot in between: the number of missed intervals is unknowable
    CHECK_EQ(row(rec(200, 0, 10, HF_BOOT_BEFORE), 0, false, false, st),
             ";;200;0;;;10;-90;110;zeit_unbekannt,luecke_davor,neustart\r\n");
    // the first dated record after undated ones: no delta, gap
    CHECK_EQ(row(rec(230, 0, 10), QH, true, false, st),
             "2026-09-11 14:00;2026-09-11 14:15;230;0;;;10;-90;110;luecke_davor\r\n");
  }

  // --- 9. state-only step (record before the export window) feeds the first row's delta ---
  {
    CsvState st;
    csv_row(nullptr, rec(700, 50, 100), QH, true, false, st, utc_time);
    CHECK_EQ(row(rec(760, 50, 100), QH + 1, true, false, st),
             "2026-09-11 14:15;2026-09-11 14:30;760;50;60;0;100;0;200;\r\n");
  }

  // --- 10. every row stays under the chunking bound ---
  {
    CsvState st;
    std::string s = row(rec(4000000000u, 4000000000u, -32768, HF_BOOT_BEFORE | HF_CONFIG_CHANGE | HF_PARTIAL | HF_NO_DATA | HF_LOCAL_ENERGY),
                        QH_MAX - 1, true, true, st);
    CHECK(s.size() <= CSV_ROW_MAX);
  }

  printf(fails ? "%d FAILED\n" : "all ok\n", fails);
  return fails ? 1 : 0;
}
