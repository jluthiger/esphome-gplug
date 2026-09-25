// Release version ordering for the update check (GET /api/update, field `newer`).
//
// ESPHome's http_request update platform calls an update "available" whenever the manifest's
// version string differs from ESPHOME_PROJECT_VERSION. That is wrong in both directions for us: a
// development build (0.7.0-dev) would be offered the older 0.6.0 as an "update", and so would a
// device someone flashed with a newer rc by hand. The firmware card offers an install only when the
// manifest's version is strictly newer by SemVer 2.0 precedence, which is what our tags follow
// (README.md "Versions and branches"): 0.6.0-rc.1 < 0.6.0 < 0.7.0-dev < 0.7.0.
//
// Header-only, no ESPHome or IDF dependency, tested in test/test_version_cmp.cpp.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace gplug_ver {

// Reads a decimal number without leading sign; false if there is none or it overflows 32 bits.
inline bool read_num_(const char *&p, const char *end, uint32_t &out) {
  if (p >= end || *p < '0' || *p > '9') return false;
  uint64_t v = 0;
  while (p < end && *p >= '0' && *p <= '9') {
    v = v * 10 + (uint32_t) (*p++ - '0');
    if (v > 0xFFFFFFFFu) return false;
  }
  out = (uint32_t) v;
  return true;
}

inline bool all_digits_(const char *a, size_t n) {
  if (n == 0) return false;
  for (size_t i = 0; i < n; i++)
    if (a[i] < '0' || a[i] > '9') return false;
  return true;
}

// One dot-separated pre-release identifier against another (SemVer 11.4.1-3): numeric ones compare
// as numbers and rank below alphanumeric ones, alphanumeric ones compare as ASCII.
inline int cmp_ident_(const char *a, size_t an, const char *b, size_t bn) {
  bool ad = all_digits_(a, an), bd = all_digits_(b, bn);
  if (ad && bd) {
    // Without leading zeros the longer number is the bigger one; equal lengths compare as text.
    while (an > 1 && *a == '0') { a++; an--; }
    while (bn > 1 && *b == '0') { b++; bn--; }
    if (an != bn) return an < bn ? -1 : 1;
  } else if (ad != bd) {
    return ad ? -1 : 1;
  }
  int c = memcmp(a, b, an < bn ? an : bn);
  if (c != 0) return c < 0 ? -1 : 1;
  return an == bn ? 0 : (an < bn ? -1 : 1);
}

struct Parsed {
  uint32_t core[3];
  const char *pre;   // first pre-release character, or nullptr
  const char *end;   // end of the pre-release (build metadata cut off)
};

// Parses MAJOR.MINOR.PATCH[-PRE][+BUILD] completely, so that trailing garbage is rejected even when
// the numbers alone would already decide the comparison.
inline bool parse_(const char *s, Parsed &v) {
  const char *p = s, *e = s + strlen(s);
  // Build metadata never affects precedence (SemVer 10).
  if (const char *b = (const char *) memchr(s, '+', (size_t) (e - s))) e = b;
  for (int i = 0; i < 3; i++) {
    if (!read_num_(p, e, v.core[i])) return false;
    if (i < 2 && (p >= e || *p++ != '.')) return false;
  }
  v.pre = nullptr;
  v.end = e;
  if (p == e) return true;
  if (*p++ != '-' || p == e) return false;   // "0.7.0x", a bare "0.7.0-"
  v.pre = p;
  for (const char *q = p; q <= e; q++) {
    if (q == e || *q == '.') {
      if (q == p) return false;   // empty identifier: "rc..1", "rc."
      p = q + 1;
    } else if (!((*q >= '0' && *q <= '9') || (*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z') || *q == '-')) {
      return false;
    }
  }
  return true;
}

// Compares two versions of the form MAJOR.MINOR.PATCH[-PRE][+BUILD]. Returns -1, 0 or 1 in `out`;
// false if either string is not such a version, in which case the caller must not offer anything.
inline bool compare(const char *a, const char *b, int &out) {
  Parsed x, y;
  if (!parse_(a, x) || !parse_(b, y)) return false;
  for (int i = 0; i < 3; i++)
    if (x.core[i] != y.core[i]) { out = x.core[i] < y.core[i] ? -1 : 1; return true; }
  // A version without a pre-release ranks above the same version with one: 0.6.0-rc.1 < 0.6.0.
  if (!x.pre || !y.pre) { out = !x.pre == !y.pre ? 0 : (x.pre ? -1 : 1); return true; }
  const char *pa = x.pre, *pb = y.pre;
  while (true) {
    const char *da = (const char *) memchr(pa, '.', (size_t) (x.end - pa));
    const char *db = (const char *) memchr(pb, '.', (size_t) (y.end - pb));
    size_t na = (size_t) ((da ? da : x.end) - pa), nb = (size_t) ((db ? db : y.end) - pb);
    int c = cmp_ident_(pa, na, pb, nb);
    if (c != 0) { out = c; return true; }
    if (!da || !db) { out = (!da && !db) ? 0 : (!da ? -1 : 1); return true; }
    pa = da + 1;
    pb = db + 1;
  }
}

// True only if `latest` parses and is strictly newer than `current`.
inline bool newer(const char *latest, const char *current) {
  int c;
  return compare(latest, current, c) && c > 0;
}

}  // namespace gplug_ver
