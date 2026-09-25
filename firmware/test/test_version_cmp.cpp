// version_cmp.h: the precedence that decides whether the firmware card offers an install. A wrong
// answer either hides a release or offers a downgrade, so the release sequence we actually tag
// (rc, final, next -dev) is pinned here, plus the malformed inputs a manifest could carry.
#include "version_cmp.h"
#include <cstdio>
using namespace gplug_ver;

int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static int cmp(const char *a, const char *b) {
  int c = 99;
  return compare(a, b, c) ? c : 99;
}

int main() {
  // --- 1. core numbers compare numerically, not as text ---
  CHECK(cmp("0.6.0", "0.6.0") == 0);
  CHECK(cmp("0.6.1", "0.6.0") == 1);
  CHECK(cmp("0.10.0", "0.9.9") == 1);
  CHECK(cmp("1.0.0", "0.99.99") == 1);
  CHECK(cmp("0.6.0", "0.6.10") == -1);

  // --- 2. our release sequence: rc < final < next -dev < next final ---
  const char *seq[] = {"0.6.0-rc.1", "0.6.0-rc.2", "0.6.0-rc.10", "0.6.0", "0.7.0-dev", "0.7.0-rc.1", "0.7.0"};
  const size_t n = sizeof(seq) / sizeof(seq[0]);
  for (size_t i = 0; i < n; i++)
    for (size_t j = 0; j < n; j++)
      CHECK(cmp(seq[i], seq[j]) == (i < j ? -1 : i > j ? 1 : 0));

  // --- 3. newer(): the only question the device asks ---
  CHECK(newer("0.7.0", "0.6.0"));
  CHECK(newer("0.6.0", "0.6.0-rc.1"));     // an rc on the device is offered its final release
  CHECK(!newer("0.6.0", "0.7.0-dev"));     // a dev build is never offered a downgrade
  CHECK(!newer("0.6.0", "0.6.0"));
  CHECK(!newer("0.5.1", "0.6.0"));

  // --- 4. SemVer details: numeric < alphanumeric, shorter prefix < longer, build ignored ---
  CHECK(cmp("1.0.0-1", "1.0.0-alpha") == -1);
  CHECK(cmp("1.0.0-alpha", "1.0.0-alpha.1") == -1);
  CHECK(cmp("1.0.0-alpha.beta", "1.0.0-beta") == -1);
  CHECK(cmp("1.0.0-rc.01", "1.0.0-rc.1") == 0);
  CHECK(cmp("0.6.0+abc", "0.6.0") == 0);
  CHECK(cmp("0.6.0-rc.1+abc", "0.6.0-rc.1") == 0);

  // --- 5. malformed input never counts as newer ---
  const char *bad[] = {"", "0.6", "0.6.", "v0.7.0", "0.7.0-", "0.7.0-rc..1", "0.7.0x", "a.b.c", "99999999999.0.0"};
  for (const char *b : bad) {
    CHECK(cmp(b, "0.6.0") == 99);
    CHECK(!newer(b, "0.6.0"));
    CHECK(!newer("0.7.0", b));
  }

  printf("%s\n", fails ? "FAILED" : "all version_cmp checks passed");
  return fails ? 1 : 0;
}
