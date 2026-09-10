// Synthetic end-to-end test: build a DLMS data-notification, encrypt it (AES-GCM, DLMS IV/AAD layout),
// wrap in HDLC (LLC + FCS/HCS), feed byte-wise, decode values Tasmota-style.
#include "dlms_decoder.h"
#include <cstdio>
#include <string>
using namespace gplug_dlms;

int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static void push_obis(std::vector<uint8_t> &v, uint8_t c, uint8_t d, uint8_t e) {
  uint8_t o[] = {0x09, 0x06, 0x01, 0x00, c, d, e, 0xFF}; v.insert(v.end(), o, o + 8);
}
static void push_u32(std::vector<uint8_t> &v, uint32_t x) { v.push_back(0x06); for (int i = 3; i >= 0; i--) v.push_back(x >> (8 * i)); }
static void push_u16(std::vector<uint8_t> &v, uint16_t x) { v.push_back(0x12); v.push_back(x >> 8); v.push_back(x); }
static void push_i16(std::vector<uint8_t> &v, int16_t x) { v.push_back(0x10); v.push_back((uint16_t) x >> 8); v.push_back((uint8_t) x); }
static void push_scaler_unit(std::vector<uint8_t> &v, int8_t s, uint8_t u) { uint8_t o[] = {0x02, 0x02, 0x0F, (uint8_t) s, 0x16, u}; v.insert(v.end(), o, o + 6); }

static std::vector<uint8_t> plaintext() {
  std::vector<uint8_t> p = {0x0F, 0x00, 0x00, 0x00, 0x01, 0x0C, 0x07, 0xE6, 0x09, 0x09, 0x03, 0x10, 0x00, 0x00, 0x00, 0xFF, 0x80, 0x00, 0x00};
  p.push_back(0x02); p.push_back(4);                      // structure of 4 entries
  // SM-ID: 0-0:96.1.0 as octet string
  { uint8_t o[] = {0x09, 0x06, 0x00, 0x00, 0x60, 0x01, 0x00, 0xFF}; p.insert(p.end(), o, o + 8); }
  const char *id = "55771146"; p.push_back(0x09); p.push_back(8); p.insert(p.end(), id, id + 8);
  push_obis(p, 1, 7, 0); push_u32(p, 1110);   push_scaler_unit(p, 0, 27);   // 1.7.0 = 1110 W
  push_obis(p, 2, 7, 0); push_u32(p, 0);
  push_obis(p, 32, 7, 0); push_u16(p, 2301);  push_scaler_unit(p, -1, 35);  // 230.1 V
  push_obis(p, 31, 7, 0); push_i16(p, -123);
  push_obis(p, 1, 8, 0); push_u32(p, 19087123);                              // 19087.123 kWh as Wh
  return p;
}

static std::vector<uint8_t> hdlc(const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> f;
  std::vector<uint8_t> body = {0x00, 0x00, 0x02, 0x21 /*dest*/, 0x03 /*src*/, 0x13 /*ctrl*/};
  size_t total = 2 + body.size() + 2 + 3 + payload.size() + 2;   // format+len, addr/ctrl, hcs, llc, payload, fcs
  body.insert(body.begin(), {(uint8_t) (0xA0 | ((total >> 8) & 7)), (uint8_t) total});
  uint16_t hcs = crc16_x25(body.data(), body.size());
  body.push_back(hcs); body.push_back(hcs >> 8);
  body.insert(body.end(), {0xE6, 0xE7, 0x00});
  body.insert(body.end(), payload.begin(), payload.end());
  uint16_t fcs = crc16_x25(body.data(), body.size());
  body.push_back(fcs); body.push_back(fcs >> 8);
  f.push_back(0x7E); f.insert(f.end(), body.begin(), body.end()); f.push_back(0x7E);
  return f;
}

static std::vector<uint8_t> glo_cipher(const uint8_t key[16], const uint8_t ak[16], std::vector<uint8_t> pt, uint32_t fc, bool auth) {
  uint8_t st[8] = {'K', 'A', 'M', 0x12, 0x34, 0x56, 0x78, 0x9A};
  uint8_t iv[12]; memcpy(iv, st, 8); iv[8] = fc >> 24; iv[9] = fc >> 16; iv[10] = fc >> 8; iv[11] = fc;
  uint8_t sec = auth ? 0x30 : 0x20;
  uint8_t aad[17] = {sec}; memcpy(aad + 1, ak, 16);
  uint8_t tag[16];
  gplug_aes::Gcm(key).run(false, iv, aad, auth ? 17 : 0, pt.data(), pt.size(), tag);
  std::vector<uint8_t> a = {0xDB, 0x08}; a.insert(a.end(), st, st + 8);
  size_t len = 5 + pt.size() + (auth ? 12 : 0);
  if (len > 255) { a.push_back(0x82); a.push_back(len >> 8); a.push_back(len); } else if (len > 127) { a.push_back(0x81); a.push_back(len); } else a.push_back(len);
  a.push_back(sec); a.insert(a.end(), iv + 8, iv + 12);
  a.insert(a.end(), pt.begin(), pt.end());
  if (auth) a.insert(a.end(), tag, tag + 12);
  return a;
}

static Value lookup(const DlmsDecoder &d, const char *obis) {
  uint8_t pat[8]; size_t n = DlmsDecoder::obis_pattern(obis, pat);
  return DlmsDecoder::find(d.plaintext(), d.plaintext_len(), pat, n);
}

int main() {
  uint8_t key[16] = {0xDE,0xAB,0xCD,0x00,0x20,0xA0,0xCF,0xDE,0xDE,0xAB,0xCD,0x00,0x20,0xA0,0xCF,0xDE};
  uint8_t ak[16] = {0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF};

  // pattern builder
  { uint8_t p[8]; CHECK(DlmsDecoder::obis_pattern("1.7.0", p) == 4 && p[0] == 1 && p[1] == 7 && p[2] == 0 && p[3] == 0xFF);
    CHECK(DlmsDecoder::obis_pattern("96.1.0", p) == 4 && p[0] == 96); CHECK(DlmsDecoder::obis_pattern("1-0:1.7.0", p) == 0); }

  // 1. authenticated, single frame, with AK → tag verified
  { DlmsDecoder d; d.set_key(key); d.set_auth_key(ak);
    auto f = hdlc(glo_cipher(key, ak, plaintext(), 42, true));
    bool ok = false; for (uint8_t c : f) ok |= d.feed(c);
    CHECK(ok); CHECK(d.stats.frames == 1 && d.stats.fcs_errors == 0 && d.stats.hcs_errors == 0 && d.stats.auth_failed == 0);
    CHECK(!d.key_invalid()); CHECK(d.frame_counter() == 42);
    Value v = lookup(d, "1.7.0"); CHECK(v.found && !v.is_string && v.num == 1110);
    v = lookup(d, "32.7.0"); CHECK(v.found && v.num == 2301);
    v = lookup(d, "31.7.0"); CHECK(v.found && v.num == -123);
    v = lookup(d, "1.8.0"); CHECK(v.found && v.num == 19087123);
    v = lookup(d, "96.1.0"); CHECK(v.found && v.is_string && v.str_len == 8 && !memcmp(v.str, "55771146", 8));
    v = lookup(d, "99.9.9"); CHECK(!v.found);
  }
  // 2. wrong key, no AK → plaintext sanity check flags key_invalid
  { DlmsDecoder d; uint8_t bad[16] = {1}; d.set_key(bad);
    auto f = hdlc(glo_cipher(key, ak, plaintext(), 1, false));
    bool ok = false; for (uint8_t c : f) ok |= d.feed(c);
    CHECK(!ok && d.key_invalid()); }
  // 3. wrong AK → auth failure
  { DlmsDecoder d; d.set_key(key); uint8_t bad[16] = {9}; d.set_auth_key(bad);
    auto f = hdlc(glo_cipher(key, ak, plaintext(), 1, true));
    bool ok = false; for (uint8_t c : f) ok |= d.feed(c);
    CHECK(!ok && d.stats.auth_failed == 1 && d.key_invalid()); }
  // 4. corrupted FCS
  { DlmsDecoder d; d.set_key(key);
    auto f = hdlc(glo_cipher(key, ak, plaintext(), 1, false)); f[f.size() - 2] ^= 0xFF;
    bool ok = false; for (uint8_t c : f) ok |= d.feed(c);
    CHECK(!ok && d.stats.fcs_errors == 1); }
  // 5. APDU split over two HDLC frames (L+G E450 style), long-form length 0x82
  { DlmsDecoder d; d.set_key(key);
    auto pt = plaintext(); for (int i = 0; i < 40; i++) { push_obis(pt, 40 + i, 7, 0); push_u32(pt, i); }   // > 255 bytes
    auto apdu = glo_cipher(key, ak, pt, 7, false);
    CHECK(apdu[10] == 0x82);
    size_t half = apdu.size() / 2;
    std::vector<uint8_t> a1(apdu.begin(), apdu.begin() + half), a2(apdu.begin() + half, apdu.end());
    auto f1 = hdlc(a1), f2 = hdlc(a2);
    bool ok = false; for (uint8_t c : f1) ok |= d.feed(c); CHECK(!ok);
    for (uint8_t c : f2) ok |= d.feed(c); CHECK(ok && d.stats.frames == 2 && d.stats.apdus == 1);
    Value v = lookup(d, "60.7.0"); CHECK(v.found && v.num == 20); }
  // 6. shared flag between frames (…7E7E… collapsed to one 7E) and leading garbage
  { DlmsDecoder d; d.set_key(key);
    auto f = hdlc(glo_cipher(key, ak, plaintext(), 3, false));
    std::vector<uint8_t> s = {0x00, 0xFF, 0x13}; s.insert(s.end(), f.begin(), f.end()); s.insert(s.end(), f.begin() + 1, f.end());
    int n = 0; for (uint8_t c : s) n += d.feed(c);
    CHECK(n == 2 && d.stats.frames == 2); }
  // 7. no key configured but encrypted frame arrives → key_invalid, encrypted_seen
  { DlmsDecoder d; auto f = hdlc(glo_cipher(key, ak, plaintext(), 3, false));
    bool ok = false; for (uint8_t c : f) ok |= d.feed(c);
    CHECK(!ok && d.key_invalid() && d.encrypted_seen()); }

  printf(fails ? "%d FAILED\n" : "all ok\n", fails);
  return fails ? 1 : 0;
}
