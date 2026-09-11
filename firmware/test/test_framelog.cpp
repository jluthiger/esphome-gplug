// FrameLog<> ring behaviour, plus an end-to-end check that DlmsDecoder's capture hook
// (last_frame()/last_frame_ok()/frame_seq()) drives it exactly the way GplugSmi::loop() does --
// including the key assertion that a CRC-valid-but-wrong-key frame is captured as ok=true with no
// plaintext, distinct from a genuinely corrupted (FCS-fail) frame.
#include "frame_log.h"
#include "dlms_decoder.h"
#include <cstdio>
#include <cstring>
using namespace gplug_dlms;
using namespace gplug_framelog;

int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

// --- duplicated from test_dlms.cpp (no shared test-helper header exists in this tree) ---
static void push_obis(std::vector<uint8_t> &v, uint8_t c, uint8_t d, uint8_t e) {
  uint8_t o[] = {0x09, 0x06, 0x01, 0x00, c, d, e, 0xFF}; v.insert(v.end(), o, o + 8);
}
static void push_u32(std::vector<uint8_t> &v, uint32_t x) { v.push_back(0x06); for (int i = 3; i >= 0; i--) v.push_back(x >> (8 * i)); }

static std::vector<uint8_t> plaintext() {
  std::vector<uint8_t> p = {0x0F, 0x00, 0x00, 0x00, 0x01, 0x0C, 0x07, 0xE6, 0x09, 0x09, 0x03, 0x10, 0x00, 0x00, 0x00, 0xFF, 0x80, 0x00, 0x00};
  p.push_back(0x02); p.push_back(2);
  push_obis(p, 1, 7, 0); push_u32(p, 1110);
  push_obis(p, 1, 8, 0); push_u32(p, 19087123);
  return p;
}

static std::vector<uint8_t> hdlc(const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> f;
  std::vector<uint8_t> body = {0x00, 0x00, 0x02, 0x21, 0x03, 0x13};
  size_t total = 2 + body.size() + 2 + 3 + payload.size() + 2;
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

static std::vector<uint8_t> glo_cipher(const uint8_t key[16], std::vector<uint8_t> pt, uint32_t fc) {
  uint8_t st[8] = {'K', 'A', 'M', 0x12, 0x34, 0x56, 0x78, 0x9A};
  uint8_t iv[12]; memcpy(iv, st, 8); iv[8] = fc >> 24; iv[9] = fc >> 16; iv[10] = fc >> 8; iv[11] = fc;
  uint8_t sec = 0x20;   // unauthenticated ciphering: key_invalid_ derives from the plaintext sanity check
  uint8_t tag[16];
  gplug_aes::Gcm(key).run(false, iv, nullptr, 0, pt.data(), pt.size(), tag);
  std::vector<uint8_t> a = {0xDB, 0x08}; a.insert(a.end(), st, st + 8);
  size_t len = 5 + pt.size();
  a.push_back((uint8_t) len);
  a.push_back(sec); a.insert(a.end(), iv + 8, iv + 12);
  a.insert(a.end(), pt.begin(), pt.end());
  return a;
}

// Mirrors GplugSmi::loop()'s DLMS branch: feed one byte, and whenever frame_seq() bumps, push a
// capture into the log exactly as loop() does.
static void feed_and_capture(DlmsDecoder &d, FrameLog<5, 768, 768> &log, const std::vector<uint8_t> &bytes,
                              uint32_t &seq_seen, uint32_t ts) {
  for (uint8_t c : bytes) {
    bool full_ok = d.feed(c);
    if (d.frame_seq() != seq_seen) {
      seq_seen = d.frame_seq();
      const auto &raw = d.last_frame();
      log.push(ts, d.last_frame_ok(), raw.data(), raw.size(),
                full_ok ? d.plaintext() : nullptr, full_ok ? d.plaintext_len() : 0);
    }
  }
}

int main() {
  // --- 1. pure ring behaviour, no DLMS involved ---
  {
    FrameLog<5, 16, 16> log;
    uint8_t a[4] = {1, 2, 3, 4};
    log.push(100, true, a, 4, nullptr, 0);
    log.push(200, false, a, 4, nullptr, 0);
    log.push(300, true, a, 4, nullptr, 0);
    CHECK(log.count() == 3);
    CHECK(log.at(0)->ts_ms == 300);   // i=0 is most recent
    CHECK(log.at(1)->ts_ms == 200);
    CHECK(log.at(2)->ts_ms == 100);
    CHECK(log.at(3) == nullptr);

    for (int i = 0; i < 4; i++) log.push(400 + i, true, a, 4, nullptr, 0);   // 7 pushes total into N=5
    CHECK(log.count() == 5);
    CHECK(log.at(0)->ts_ms == 403);
    CHECK(log.at(4)->ts_ms == 300);   // oldest surviving push

    uint8_t big[20]; memset(big, 0xAB, sizeof big);
    log.push(500, true, big, sizeof big, nullptr, 0);
    CHECK(log.at(0)->raw_len == 16 && log.at(0)->raw_trunc);
  }

  uint8_t key[16] = {0xDE,0xAB,0xCD,0x00,0x20,0xA0,0xCF,0xDE,0xDE,0xAB,0xCD,0x00,0x20,0xA0,0xCF,0xDE};
  uint8_t bad_key[16] = {1};

  // --- 2. good authenticated-frame-free (unauthenticated) push, correct key ---
  {
    DlmsDecoder d; d.set_key(key);
    FrameLog<5, 768, 768> log; uint32_t seq = 0;
    feed_and_capture(d, log, hdlc(glo_cipher(key, plaintext(), 1)), seq, 1000);
    CHECK(log.count() == 1);
    CHECK(log.at(0)->ok);
    CHECK(log.at(0)->plain_len > 0);
  }

  // --- 3. corrupted FCS: capture still happens, but ok=false and no plaintext ---
  {
    DlmsDecoder d; d.set_key(key);
    auto f = hdlc(glo_cipher(key, plaintext(), 1)); f[f.size() - 2] ^= 0xFF;
    FrameLog<5, 768, 768> log; uint32_t seq = 0;
    feed_and_capture(d, log, f, seq, 2000);
    CHECK(log.count() == 1);
    CHECK(!log.at(0)->ok);
    CHECK(log.at(0)->plain_len == 0);
  }

  // --- 4. wrong key, correct FCS/HCS: HDLC frame is genuinely well-formed (ok=true) but the
  //        decrypt/plaintext-sanity check fails, so no plaintext is captured. This is the core
  //        distinction from on_frame_()'s CRC-ok flag vs. the overall decode outcome. ---
  {
    DlmsDecoder d; d.set_key(bad_key);
    FrameLog<5, 768, 768> log; uint32_t seq = 0;
    feed_and_capture(d, log, hdlc(glo_cipher(key, plaintext(), 1)), seq, 3000);
    CHECK(log.count() == 1);
    CHECK(log.at(0)->ok);            // FCS/HCS fine
    CHECK(log.at(0)->plain_len == 0);   // but never decrypted
  }

  // --- 5. two frames fed sequentially land in the right order ---
  {
    DlmsDecoder d; d.set_key(key);
    FrameLog<5, 768, 768> log; uint32_t seq = 0;
    feed_and_capture(d, log, hdlc(glo_cipher(key, plaintext(), 1)), seq, 4000);
    feed_and_capture(d, log, hdlc(glo_cipher(key, plaintext(), 2)), seq, 4001);
    CHECK(log.count() == 2);
    CHECK(log.at(0)->ts_ms == 4001);
    CHECK(log.at(1)->ts_ms == 4000);
  }

  printf(fails ? "%d FAILED\n" : "all ok\n", fails);
  return fails ? 1 : 0;
}
