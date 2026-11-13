// tariff_parser.h: the dynamic-tariff responses as BKW and CKW actually send them (saved
// 2026-11-13 in tariff/, including CKW's two DST days), fed in every chunking a socket could
// produce, plus the calendar arithmetic that places a slot. A wrong answer shifts every price by an
// hour against the measured energy, which no one would notice on a chart.
#include "tariff_parser.h"
#include <cstdio>
#include <string>
#include <vector>
using namespace gplug_tariff;

int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static std::string slurp(const char *path) {
  std::string s;
  FILE *f = fopen(path, "rb");
  if (!f) { printf("FAIL cannot open %s\n", path); fails++; return s; }
  char b[4096];
  size_t n;
  while ((n = fread(b, 1, sizeof b, f)) > 0) s.append(b, n);
  fclose(f);
  return s;
}

static int64_t utc(int y, unsigned m, unsigned d, unsigned hh, unsigned mi) {
  return days_from_civil(y, m, d) * 86400 + hh * 3600 + mi * 60;
}

// Parses `body` in chunks of `chunk` bytes (0 = all at once).
static TariffParser *parse(Provider p, const std::string &body, size_t chunk) {
  auto *tp = new TariffParser(p);
  if (!chunk) chunk = body.size();
  for (size_t i = 0; i < body.size(); i += chunk)
    tp->feed(body.data() + i, body.size() - i < chunk ? body.size() - i : chunk);
  tp->finish();
  return tp;
}

static bool same(const TariffParser &a, const TariffParser &b) {
  if (a.count() != b.count() || a.bad() != b.bad() || a.dropped() != b.dropped()) return false;
  if (strcmp(a.publication(), b.publication())) return false;
  for (size_t i = 0; i < a.count(); i++)
    if (a.slots()[i].qh != b.slots()[i].qh || a.slots()[i].price != b.slots()[i].price) return false;
  return true;
}

int main() {
  // --- 1. calendar: the switch instants and local midnights of 2026 ---
  CHECK(last_sunday_0100z(2026, 3) == utc(2026, 3, 29, 1, 0));
  CHECK(last_sunday_0100z(2026, 10) == utc(2026, 10, 25, 1, 0));
  CHECK(last_sunday_0100z(2027, 3) == utc(2027, 3, 28, 1, 0));
  CHECK(zurich_offset(utc(2026, 3, 29, 0, 59) + 59) == 3600);
  CHECK(zurich_offset(utc(2026, 3, 29, 1, 0)) == 7200);
  CHECK(zurich_offset(utc(2026, 10, 25, 0, 59)) == 7200);
  CHECK(zurich_offset(utc(2026, 10, 25, 1, 0)) == 3600);
  CHECK(zurich_offset(utc(2026, 7, 1, 12, 0)) == 7200);
  CHECK(zurich_offset(utc(2026, 1, 1, 12, 0)) == 3600);
  CHECK(local_midnight(utc(2026, 11, 13, 14, 0)) == utc(2026, 11, 12, 23, 0));
  CHECK(local_midnight(utc(2026, 11, 12, 23, 0)) == utc(2026, 11, 12, 23, 0));
  CHECK(local_midnight(utc(2026, 11, 12, 22, 59)) == utc(2026, 11, 11, 23, 0));
  CHECK(local_midnight(utc(2026, 7, 1, 21, 59)) == utc(2026, 6, 30, 22, 0));
  CHECK(local_midnight(utc(2026, 7, 1, 22, 0)) == utc(2026, 7, 1, 22, 0));
  // Switch days: 23 and 25 hours long.
  int64_t sp = local_midnight(utc(2026, 3, 29, 12, 0)), sp1 = add_local_days(sp, 1);
  CHECK(sp == utc(2026, 3, 28, 23, 0));
  CHECK(sp1 - sp == 23 * 3600);
  int64_t au = local_midnight(utc(2026, 10, 25, 12, 0)), au1 = add_local_days(au, 1);
  CHECK(au == utc(2026, 10, 24, 22, 0));
  CHECK(au1 - au == 25 * 3600);
  CHECK(add_local_days(au1, -1) == au);
  CHECK(add_local_days(sp, -3) == utc(2026, 3, 25, 23, 0));
  int y; unsigned m, d;
  local_date(utc(2026, 11, 12, 23, 30), y, m, d);
  CHECK(y == 2026 && m == 11 && d == 13);
  // Wall time -> UTC: the gap has no reading, the repeated hour two.
  int64_t t;
  CHECK(!local_to_utc(2026, 3, 29, 2, 30, 0, INT64_MIN, t));
  CHECK(local_to_utc(2026, 10, 25, 2, 30, 0, INT64_MIN, t) && t == utc(2026, 10, 25, 0, 30));
  CHECK(local_to_utc(2026, 10, 25, 2, 30, 0, utc(2026, 10, 25, 0, 45), t) && t == utc(2026, 10, 25, 1, 30));
  CHECK(qh_of(utc(2020, 1, 1, 0, 15)) == 1);
  CHECK(utc_of_qh(qh_of(utc(2026, 11, 13, 10, 45))) == utc(2026, 11, 13, 10, 45));

  // --- 2. scalars ---
  int64_t u;
  CHECK(parse_iso_utc("2026-11-12T23:00:00Z", u) && u == utc(2026, 11, 12, 23, 0));
  CHECK(parse_iso_utc("2026-11-12T23:00:00.000Z", u) && u == utc(2026, 11, 12, 23, 0));
  CHECK(!parse_iso_utc("2026-11-12T23:00:00+01:00", u));
  CHECK(!parse_iso_utc("2026-11-12 23:00:00Z", u));
  CHECK(!parse_iso_utc("2026-13-12T23:00:00Z", u));
  CHECK(parse_ckw_local("13.11.2026 00:00:00", INT64_MIN, u) && u == utc(2026, 11, 12, 23, 0));
  CHECK(!parse_ckw_local("13.11.2026 00:00", INT64_MIN, u));
  int32_t v;
  CHECK(parse_micro("0.0347", v) && v == 34700);
  CHECK(parse_micro("0.2", v) && v == 200000);
  CHECK(parse_micro("-0.012", v) && v == -12000);
  CHECK(parse_micro("0", v) && v == 0);
  CHECK(parse_micro("1E-05", v) && v == 10);
  CHECK(parse_micro("2.5e-1", v) && v == 250000);
  CHECK(parse_micro("0.12345678", v) && v == 123457);     // rounded, half away from zero
  CHECK(parse_micro("-0.0000005", v) && v == -1);
  CHECK(parse_micro("12", v) && v == 12000000);
  CHECK(!parse_micro("", v));
  CHECK(!parse_micro("-", v));
  CHECK(!parse_micro("0.1x", v));
  CHECK(!parse_micro("null", v));
  CHECK(!parse_micro("1000", v));                         // 1000 CHF/kWh: garbage, not a price

  // --- 3. BKW, 2026-11-13: 96 feed-in slots from local midnight, in every chunking ---
  std::string bkw = slurp("tariff/bkw_2026-11-13.json");
  TariffParser *whole = parse(P_BKW, bkw, 0);
  CHECK(whole->result() == TariffParser::OK);
  CHECK(whole->count() == 96);
  CHECK(whole->bad() == 0 && whole->dropped() == 0);
  CHECK(!strcmp(whole->publication(), "2026-11-13T14:50:00Z"));
  CHECK(whole->slots()[0].qh == qh_of(utc(2026, 11, 12, 23, 0)));
  CHECK(whole->slots()[0].price == 200000);
  CHECK(whole->slots()[1].price == 192000);
  CHECK(whole->slots()[95].qh == qh_of(utc(2026, 11, 13, 22, 45)));
  CHECK(whole->slots()[95].price == 176000);
  for (size_t i = 1; i < whole->count(); i++) CHECK(whole->slots()[i].qh == whole->slots()[i - 1].qh + 1);
  for (size_t c : {1, 2, 3, 7, 64, 511, 512, 1024}) {
    TariffParser *p = parse(P_BKW, bkw, c);
    CHECK(same(*whole, *p));
    delete p;
  }
  delete whole;

  // --- 4. CKW, 2026-11-13: string values, local wall time ---
  std::string ckw = slurp("tariff/ckw_2026-11-13.json");
  whole = parse(P_CKW, ckw, 0);
  CHECK(whole->result() == TariffParser::OK);
  CHECK(whole->count() == 96);
  CHECK(whole->bad() == 0 && whole->dropped() == 0);
  CHECK(!strcmp(whole->publication(), "12.11.2026 10:19:27"));
  CHECK(whole->slots()[0].qh == qh_of(utc(2026, 11, 12, 23, 0)));
  CHECK(whole->slots()[0].price == 34700);
  CHECK(whole->slots()[95].price == 38000);
  for (size_t c : {1, 5, 100, 512}) {
    TariffParser *p = parse(P_CKW, ckw, c);
    CHECK(same(*whole, *p));
    delete p;
  }
  // The wrong parser for the body finds no slot rather than a wrong one.
  TariffParser *wrong = parse(P_BKW, ckw, 0);
  CHECK(wrong->count() == 0 && wrong->result() != TariffParser::OK);
  delete wrong;
  delete whole;

  // --- 5. CKW's switch days: 96 slots on a naive grid ---
  // Spring (23 h): 01:45 then 03:00..03:45 twice. The first block is kept, the repeat dropped.
  TariffParser *spr = parse(P_CKW, slurp("tariff/ckw_2026-03-29.json"), 0);
  CHECK(spr->result() == TariffParser::OK);
  CHECK(spr->count() == 92);
  CHECK(spr->dropped() == 4);
  CHECK(spr->slots()[7].qh == qh_of(utc(2026, 3, 29, 0, 45)));    // 01:45 CET
  CHECK(spr->slots()[8].qh == qh_of(utc(2026, 3, 29, 1, 0)));     // 03:00 CEST
  CHECK(spr->slots()[8].price == 115500);
  CHECK(spr->slots()[12].qh == qh_of(utc(2026, 3, 29, 2, 0)));    // 04:00 CEST
  for (size_t i = 1; i < spr->count(); i++) CHECK(spr->slots()[i].qh == spr->slots()[i - 1].qh + 1);
  CHECK(spr->slots()[91].qh + 1 == qh_of(utc(2026, 3, 29, 22, 0)));   // ends at local midnight
  delete spr;
  // Autumn (25 h): 02:xx comes once and is read as summer time; the winter 02:xx hour has no price.
  TariffParser *aut = parse(P_CKW, slurp("tariff/ckw_2026-10-25.json"), 0);
  CHECK(aut->result() == TariffParser::OK);
  CHECK(aut->count() == 96);
  CHECK(aut->dropped() == 0);
  CHECK(aut->slots()[8].qh == qh_of(utc(2026, 10, 25, 0, 0)));     // 02:00 CEST
  CHECK(aut->slots()[11].qh == qh_of(utc(2026, 10, 25, 0, 45)));
  CHECK(aut->slots()[12].qh == qh_of(utc(2026, 10, 25, 2, 0)));    // 03:00 CET: 01:00-01:45Z missing
  CHECK(aut->slots()[95].qh + 1 == qh_of(utc(2026, 10, 25, 23, 0)));
  delete aut;

  // --- 5b. Swiss Tariff Hub, CKW Home dynamic, 2026-11-13: three components summed ---
  std::string sth = slurp("tariff/sth_2026-11-13.json");
  whole = parse(P_STH, sth, 0);
  CHECK(whole->result() == TariffParser::OK);
  CHECK(whole->count() == 96);
  CHECK(whole->bad() == 0 && whole->dropped() == 0);
  CHECK(whole->slots()[0].qh == qh_of(utc(2026, 11, 12, 23, 0)));
  CHECK(whole->slots()[0].price == 120000 + 65000 + 34700);    // energy + grid + residual
  CHECK(whole->slots()[95].price == 120000 + 68300 + 38000);
  for (size_t i = 1; i < whole->count(); i++) CHECK(whole->slots()[i].qh == whole->slots()[i - 1].qh + 1);
  for (size_t c : {1, 3, 64, 512}) {
    TariffParser *p = parse(P_STH, sth, c);
    CHECK(same(*whole, *p));
    delete p;
  }
  delete whole;
  // Same 96 quarter hours as CKW's own API, and above it by at least the 0.12 CHF energy price.
  {
    TariffParser *c = parse(P_CKW, ckw, 0);
    TariffParser *h = parse(P_STH, sth, 0);
    CHECK(c->count() == h->count());
    for (size_t i = 0; i < c->count() && i < h->count(); i++)
      CHECK(h->slots()[i].qh == c->slots()[i].qh && h->slots()[i].price - c->slots()[i].price > 120000);
    delete c;
    delete h;
  }
  // A quarter hour missing one component is left out, not shown too cheap; unknown arrays are skipped.
  {
    TariffParser *m = parse(P_STH, "{\"energy_price_utc\":[{\"2026-11-12T23:00:00+00:00\":0.1},{\"2026-11-12T23:15:00+00:00\":0.1}],"
                                   "\"grid_price_utc\":[{\"2026-11-12T23:00:00+00:00\":0.05},{\"2026-11-12T23:15:00+00:00\":0.05}],"
                                   "\"other_price_utc\":[{\"2026-11-12T23:00:00+00:00\":9}],"
                                   "\"residual_price_utc\":[{\"2026-11-12T23:00:00+00:00\":-0.01}]}", 0);
    CHECK(m->result() == TariffParser::OK);
    CHECK(m->count() == 1 && m->bad() == 2 && m->slots()[0].price == 140000);
    delete m;
    m = parse(P_STH, "{\"detail\":{\"error\":\"history_pro_required\",\"earliest_date\":\"2026-11-07\"}}", 0);
    CHECK(m->result() == TariffParser::E_EMPTY);
    delete m;
  }

  // --- 6. what a provider might send instead ---
  TariffParser *e = parse(P_CKW, "{\"data\":[]}", 0);
  CHECK(e->result() == TariffParser::E_EMPTY);
  delete e;
  e = parse(P_BKW, "{\"title\":\"Unexpected query parameters\",\"status\":400}", 0);
  CHECK(e->result() == TariffParser::E_EMPTY);
  delete e;
  e = parse(P_CKW, "{\"data\":[{\"start\":\"13.11.2026 00:00:00\",\"unit\":\"EUR_kWh\",\"value\":\"0.1\"}]}", 0);
  CHECK(e->result() == TariffParser::E_UNIT && e->count() == 0);
  delete e;
  // A value without a start time, a start not on a quarter hour, a value that is no number.
  e = parse(P_CKW, "{\"data\":[{\"unit\":\"CHF_kWh\",\"value\":\"0.1\"},"
                   "{\"start\":\"13.11.2026 00:07:00\",\"unit\":\"CHF_kWh\",\"value\":\"0.1\"},"
                   "{\"start\":\"13.11.2026 00:15:00\",\"unit\":\"CHF_kWh\",\"value\":\"n/a\"},"
                   "{\"start\":\"13.11.2026 00:30:00\",\"unit\":\"CHF_kWh\",\"value\":\"-0.05\"}]}", 0);
  CHECK(e->result() == TariffParser::OK);
  CHECK(e->count() == 1 && e->bad() == 3);
  CHECK(e->slots()[0].price == -50000 && e->slots()[0].qh == qh_of(utc(2026, 11, 12, 23, 30)));
  delete e;
  // A number at the very end of the body (no delimiter after it) still counts.
  e = parse(P_BKW, "{\"prices\":[{\"start_timestamp\":\"2026-11-12T23:00:00Z\",\"unit\":\"CHF_kWh\",\"value\":-0.01", 0);
  CHECK(e->count() == 1 && e->slots()[0].price == -10000);
  delete e;
  // Escapes and long strings do not derail the scanner.
  e = parse(P_BKW, "{\"note\":\"a \\\"quoted\\\" , : } text that is much longer than the scratch buffer\","
                   "\"prices\":[{\"start_timestamp\":\"2026-11-12T23:00:00Z\",\"feed_in\":[{\"unit\":\"CHF_kWh\",\"value\":0.1}]}]}", 0);
  CHECK(e->count() == 1 && e->slots()[0].price == 100000);
  delete e;
  // More slots than PARSE_MAX: an error, not a silently truncated day.
  std::string big = "{\"prices\":[";
  for (size_t i = 0; i < PARSE_MAX + 1; i++) {
    char b[160];
    int64_t s = utc(2026, 11, 1, 0, 0) + (int64_t) i * 900;
    int yy; unsigned mm, dd;
    civil_from_days(s / 86400, yy, mm, dd);
    snprintf(b, sizeof b, "%s{\"start_timestamp\":\"%04d-%02u-%02uT%02d:%02d:00Z\",\"feed_in\":[{\"unit\":\"CHF_kWh\",\"value\":0.1}]}",
             i ? "," : "", yy, mm, dd, (int) (s % 86400 / 3600), (int) (s % 3600 / 60));
    big += b;
  }
  big += "]}";
  e = parse(P_BKW, big, 0);
  CHECK(e->result() == TariffParser::E_OVERFLOW);
  delete e;

  // --- 7. the table ---
  TariffTable<10> tb;
  uint32_t f, en;
  CHECK(!tb.span(f, en));
  tb.rebase(100);
  CHECK(tb.put(100, 1) && tb.put(109, 2) && !tb.put(110, 3) && !tb.put(99, 4));
  CHECK(tb.count(100, 110) == 2);
  CHECK(tb.span(f, en) && f == 100 && en == 110);
  tb.rebase(105);   // forward: the overlap stays
  CHECK(tb.get(100) == PRICE_NONE && tb.get(109) == 2 && tb.get(114) == PRICE_NONE);
  CHECK(tb.span(f, en) && f == 109 && en == 110);
  tb.rebase(103);   // backward: still the overlap
  CHECK(tb.get(109) == 2 && tb.get(103) == PRICE_NONE);
  tb.rebase(500);   // no overlap
  CHECK(!tb.span(f, en));

  // --- 8. provider ids round-trip ---
  Provider pv;
  CHECK(provider_from_id("ckw", pv) && pv == P_CKW && !strcmp(provider_id(pv), "ckw"));
  CHECK(provider_from_id("off", pv) && pv == P_OFF);
  CHECK(!provider_from_id("ewz", pv));
  CHECK(provider_from_body("{\"provider\":\"bkw\"}", pv) && pv == P_BKW);
  CHECK(provider_from_body("{ \"provider\" : \"off\" }", pv) && pv == P_OFF);
  CHECK(!provider_from_body("{\"provider\":\"bkwbkwbkw\"}", pv));
  CHECK(!provider_from_body("{\"provider\":\"ckw", pv));
  CHECK(!provider_from_body("{\"provider\":3}", pv));
  CHECK(!provider_from_body("{}", pv));
  CHECK(provider_from_body("{\"provider\":\"sth\",\"key\":\"x\"}", pv) && pv == P_STH);
  char fb[16];
  CHECK(json_str_field("{\"provider\":\"sth\",\"tariff_id\":\"ckw_home\"}", "tariff_id", fb, sizeof fb) == 1 && !strcmp(fb, "ckw_home"));
  CHECK(json_str_field("{\"provider\":\"sth\"}", "key", fb, sizeof fb) == 0);
  CHECK(json_str_field("{\"key\":\"0123456789abcdefXYZ\"}", "key", fb, sizeof fb) == -1);
  CHECK(json_str_field("{\"key\":\"a\\\"b\"}", "key", fb, sizeof fb) == -1);
  CHECK(token_ok("stk_Ab3-x_9", 64) && !token_ok("a b", 64) && !token_ok("", 64) && !token_ok("a&b", 64) && !token_ok("abc", 2));
  int64_t tz;
  CHECK(parse_iso_utc("2026-11-12T23:00:00+00:00", tz) && tz == utc(2026, 11, 12, 23, 0));
  CHECK(!parse_iso_utc("2026-11-12T23:00:00+01:00", tz));

  if (fails) { printf("%d failure(s)\n", fails); return 1; }
  printf("all tariff checks passed\n");
  return 0;
}
