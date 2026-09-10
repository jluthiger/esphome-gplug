// AES-128-GCM for DLMS "general-glo-ciphering".
//
// On the real firmware (ESP_PLATFORM defined by ESP-IDF) this is a thin wrapper around
// ESP-IDF's bundled mbedtls_gcm_* -- the same proven, audited library used by the real
// github.com/jluthiger/esphome-gplugk component, not a hand-rolled implementation.
//
// On host (no ESP_PLATFORM, e.g. running firmware/test/*.cpp on a dev machine) this falls back
// to a small from-scratch AES-128-GCM so the DLMS decoder stays host-testable without needing
// ESP-IDF's mbedtls build (which pulls in PSA crypto and is impractical to link standalone).
// This fallback is verified against the NIST GCM test vectors (test_aes.cpp) and against real
// gPlugK captures decrypted with the real device key (test_raw.cpp) -- it is not a guess, but
// it is still not what ships; ESP_PLATFORM is what runs on the device.
#pragma once
#include <cstdint>
#include <cstring>

#ifdef ESP_PLATFORM
#include <mbedtls/gcm.h>

namespace gplug_aes {

// Same interface as the host fallback below: construct with the key, call run() to
// decrypt/encrypt in place and get the tag. Mirrors esphome-gplugk's gplugk.cpp exactly:
// mbedtls_gcm_crypt_and_tag() in DECRYPT mode does not itself reject a bad tag (see mbedtls's
// own doc comment on that function) -- the caller compares `tag` against the received tag only
// when it actually wants authentication (see DlmsDecoder::on_apdu_).
class Gcm {
 public:
  explicit Gcm(const uint8_t key[16]) {
    mbedtls_gcm_init(&ctx_);
    mbedtls_gcm_setkey(&ctx_, MBEDTLS_CIPHER_ID_AES, key, 128);
  }
  ~Gcm() { mbedtls_gcm_free(&ctx_); }
  Gcm(const Gcm &) = delete;

  void run(bool decrypt, const uint8_t iv[12], const uint8_t *aad, size_t aad_len, uint8_t *data, size_t len,
           uint8_t tag[16]) {
    mbedtls_gcm_crypt_and_tag(&ctx_, decrypt ? MBEDTLS_GCM_DECRYPT : MBEDTLS_GCM_ENCRYPT, len, iv, 12, aad, aad_len,
                               data, data, 16, tag);
  }

 private:
  mbedtls_gcm_context ctx_;
};

}  // namespace gplug_aes

#else  // host fallback

namespace gplug_aes {

class Aes128 {
 public:
  explicit Aes128(const uint8_t key[16]) { expand_(key); }

  void encrypt_block(const uint8_t in[16], uint8_t out[16]) const {
    uint8_t s[16];
    memcpy(s, in, 16);
    add_round_key_(s, 0);
    for (int r = 1; r < 10; r++) { sub_bytes_(s); shift_rows_(s); mix_columns_(s); add_round_key_(s, r); }
    sub_bytes_(s); shift_rows_(s); add_round_key_(s, 10);
    memcpy(out, s, 16);
  }

 private:
  uint8_t rk_[176];

  static uint8_t sbox_(uint8_t x) {
    static const uint8_t S[256] = {
      0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
      0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
      0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
      0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
      0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
      0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
      0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
      0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16};
    return S[x];
  }
  static uint8_t xtime_(uint8_t x) { return (uint8_t) ((x << 1) ^ ((x & 0x80) ? 0x1b : 0)); }

  void expand_(const uint8_t key[16]) {
    memcpy(rk_, key, 16);
    uint8_t rcon = 1;
    for (int i = 16; i < 176; i += 4) {
      uint8_t t[4] = {rk_[i - 4], rk_[i - 3], rk_[i - 2], rk_[i - 1]};
      if (i % 16 == 0) {
        uint8_t u = t[0]; t[0] = sbox_(t[1]) ^ rcon; t[1] = sbox_(t[2]); t[2] = sbox_(t[3]); t[3] = sbox_(u);
        rcon = xtime_(rcon);
      }
      for (int j = 0; j < 4; j++) rk_[i + j] = rk_[i - 16 + j] ^ t[j];
    }
  }
  void add_round_key_(uint8_t s[16], int r) const { for (int i = 0; i < 16; i++) s[i] ^= rk_[r * 16 + i]; }
  static void sub_bytes_(uint8_t s[16]) { for (int i = 0; i < 16; i++) s[i] = sbox_(s[i]); }
  static void shift_rows_(uint8_t s[16]) {
    uint8_t t[16];
    for (int c = 0; c < 4; c++) for (int r = 0; r < 4; r++) t[c * 4 + r] = s[((c + r) % 4) * 4 + r];
    memcpy(s, t, 16);
  }
  static void mix_columns_(uint8_t s[16]) {
    for (int c = 0; c < 4; c++) {
      uint8_t *p = s + c * 4, a0 = p[0], a1 = p[1], a2 = p[2], a3 = p[3];
      p[0] = xtime_(a0) ^ (xtime_(a1) ^ a1) ^ a2 ^ a3;
      p[1] = a0 ^ xtime_(a1) ^ (xtime_(a2) ^ a2) ^ a3;
      p[2] = a0 ^ a1 ^ xtime_(a2) ^ (xtime_(a3) ^ a3);
      p[3] = (xtime_(a0) ^ a0) ^ a1 ^ a2 ^ xtime_(a3);
    }
  }
};

class Gcm {
 public:
  explicit Gcm(const uint8_t key[16]) : aes_(key) {
    uint8_t z[16] = {0};
    aes_.encrypt_block(z, h_);
  }

  // GCM with 96-bit IV. Works in place. Computes the 16-byte tag over aad + ciphertext.
  // decrypt: data is ciphertext on entry, plaintext on exit.  encrypt: the reverse.
  void run(bool decrypt, const uint8_t iv[12], const uint8_t *aad, size_t aad_len, uint8_t *data, size_t len,
           uint8_t tag[16]) {
    uint8_t j0[16], ctr[16], ks[16], y[16] = {0};
    memcpy(j0, iv, 12); j0[12] = j0[13] = j0[14] = 0; j0[15] = 1;
    memcpy(ctr, j0, 16);
    ghash_(y, aad, aad_len);
    if (decrypt) ghash_(y, data, len);
    for (size_t off = 0; off < len; off += 16) {
      inc32_(ctr);
      aes_.encrypt_block(ctr, ks);
      size_t n = len - off < 16 ? len - off : 16;
      for (size_t i = 0; i < n; i++) data[off + i] ^= ks[i];
    }
    if (!decrypt) ghash_(y, data, len);
    uint8_t lens[16];
    put64_(lens, (uint64_t) aad_len * 8); put64_(lens + 8, (uint64_t) len * 8);
    gmul_(y, lens);
    aes_.encrypt_block(j0, ks);
    for (int i = 0; i < 16; i++) tag[i] = y[i] ^ ks[i];
  }

 private:
  Aes128 aes_;
  uint8_t h_[16];

  static void put64_(uint8_t *p, uint64_t v) { for (int i = 7; i >= 0; i--) { p[i] = (uint8_t) v; v >>= 8; } }
  static void inc32_(uint8_t c[16]) { for (int i = 15; i >= 12; i--) if (++c[i]) break; }

  void gmul_(uint8_t y[16], const uint8_t x[16]) const {   // y = (y ^ x) * H in GF(2^128)
    uint8_t v[16], z[16] = {0};
    for (int i = 0; i < 16; i++) v[i] = y[i] ^ x[i];
    uint8_t hh[16]; memcpy(hh, h_, 16);
    for (int i = 0; i < 128; i++) {
      if (v[i / 8] & (0x80 >> (i % 8))) for (int k = 0; k < 16; k++) z[k] ^= hh[k];
      bool lsb = hh[15] & 1;
      for (int k = 15; k > 0; k--) hh[k] = (uint8_t) ((hh[k] >> 1) | (hh[k - 1] << 7));
      hh[0] >>= 1;
      if (lsb) hh[0] ^= 0xe1;
    }
    memcpy(y, z, 16);
  }
  void ghash_(uint8_t y[16], const uint8_t *p, size_t n) const {
    for (size_t off = 0; off < n; off += 16) {
      uint8_t blk[16] = {0};
      size_t m = n - off < 16 ? n - off : 16;
      memcpy(blk, p + off, m);
      gmul_(y, blk);
    }
  }
};

}  // namespace gplug_aes

#endif  // ESP_PLATFORM
