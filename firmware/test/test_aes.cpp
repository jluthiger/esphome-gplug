// NIST GCM test vectors (McGrew & Viega, "The Galois/Counter Mode of Operation", test cases 1-4)
#include "aes_gcm.h"
#include <cstdio>
#include <string>
using namespace gplug_aes;
static void hex2(const char *h, uint8_t *o) { for (size_t i = 0; h[2*i]; i++) { unsigned v; sscanf(h + 2*i, "%2x", &v); o[i] = v; } }
static std::string hexs(const uint8_t *p, size_t n) { char b[3]; std::string s; for (size_t i = 0; i < n; i++) { snprintf(b, 3, "%02x", p[i]); s += b; } return s; }
int fails = 0;
static void tc(const char *name, const char *key, const char *iv, const char *pt, const char *aad, const char *ct, const char *tag) {
  uint8_t k[16], v[12], d[128], a[32], t[16];
  hex2(key, k); hex2(iv, v); size_t n = strlen(pt) / 2, an = strlen(aad) / 2; hex2(pt, d); hex2(aad, a);
  Gcm g(k);
  g.run(false, v, a, an, d, n, t);
  bool ok = hexs(d, n) == ct && hexs(t, 16) == tag;
  g.run(true, v, a, an, d, n, t);
  ok = ok && hexs(d, n) == pt && hexs(t, 16) == tag;
  printf("%s %s\n", ok ? "ok  " : "FAIL", name); if (!ok) fails++;
}
int main() {
  tc("TC1 empty", "00000000000000000000000000000000", "000000000000000000000000", "", "", "", "58e2fccefa7e3061367f1d57a4e7455a");
  tc("TC2 1 block", "00000000000000000000000000000000", "000000000000000000000000", "00000000000000000000000000000000", "",
     "0388dace60b6a392f328c2b971b2fe78", "ab6e47d42cec13bdf53a67b21257bddf");
  tc("TC3 4 blocks", "feffe9928665731c6d6a8f9467308308", "cafebabefacedbaddecaf888",
     "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b391aafd255", "",
     "42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b25466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091473f5985",
     "4d5c2af327cd64a62cf35abd2ba6fab4");
  tc("TC4 aad + partial", "feffe9928665731c6d6a8f9467308308", "cafebabefacedbaddecaf888",
     "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39",
     "feedfacedeadbeeffeedfacedeadbeefabaddad2",
     "42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b25466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091",
     "5bc94fbc3221a5db94fae95ae7121a47");
  printf(fails ? "%d FAILED\n" : "all ok\n", fails); return fails;
}
