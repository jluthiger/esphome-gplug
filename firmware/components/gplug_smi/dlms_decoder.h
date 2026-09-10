// DLMS/COSEM push decoder: HDLC frames → (LLC) → (GBT segments) → general-glo-ciphering (AES-128-GCM)
// → plaintext data-notification. Values are found Tasmota-style: search the OBIS byte pattern
// (C D E FF) anywhere in the plaintext and decode the COSEM typed value that follows.
// Header-only, no ESPHome deps, host-testable (firmware/test/test_dlms.cpp).
#pragma once
#include "aes_gcm.h"
#include <cstdint>
#include <cstring>
#include <vector>

namespace gplug_dlms {

static inline uint16_t crc16_x25(const uint8_t *p, size_t n) {   // HDLC FCS/HCS (CRC-16/X-25)
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < n; i++) {
    crc ^= p[i];
    for (int b = 0; b < 8; b++) crc = (crc & 1) ? (crc >> 1) ^ 0x8408 : crc >> 1;
  }
  return (uint16_t) ~crc;
}

struct Stats {
  uint32_t frames{0}, fcs_errors{0}, hcs_errors{0}, apdus{0}, auth_failed{0}, overflow{0};
};

struct Value {
  bool found{false};
  bool is_string{false};
  double num{0};
  const uint8_t *str{nullptr};
  uint8_t str_len{0};
};

class DlmsDecoder {
 public:
  void set_key(const uint8_t key[16]) { memcpy(key_, key, 16); have_key_ = true; }
  void set_auth_key(const uint8_t ak[16]) { memcpy(ak_, ak, 16); have_ak_ = true; }
  void clear_keys() { have_key_ = have_ak_ = false; }
  void set_max_frame(size_t n) { max_frame_ = n; }

  // Feed one byte. Returns true when a complete plaintext APDU is available via plaintext()/plaintext_len().
  // HDLC (DLMS flavour) has no byte stuffing: 0x7E can occur inside ciphertext, so the frame length comes
  // from the format field, not from flag scanning.
  bool feed(uint8_t c) {
    if (!in_frame_) {
      if (c == 0x7E) { in_frame_ = true; frame_.clear(); expect_ = 0; }
      return false;
    }
    if (frame_.empty() && c == 0x7E) return false;             // 7E 7E: still at frame start
    if (frame_.empty() && (c & 0xF0) != 0xA0) { in_frame_ = false; return false; }   // not a type-3 frame
    if (expect_ && frame_.size() == expect_) {                 // all bytes in: this must be the closing flag
      bool closed = (c == 0x7E);
      bool done = closed && on_frame_();
      // Snapshot the raw frame for the Datenstrom capture ring regardless of decode outcome --
      // GplugSmi wants to show CRC-fail and wrong-key frames too, not just fully-decoded ones.
      if (closed) {
        last_frame_.assign(frame_.begin(), frame_.end());
        last_frame_ok_ = last_frame_crc_ok_;
        frame_seq_++;
      }
      frame_.clear(); expect_ = 0;
      in_frame_ = closed;                                       // closing flag may double as the next opening flag
      return done;
    }
    if (frame_.size() >= max_frame_) { stats.overflow++; frame_.clear(); in_frame_ = false; return false; }
    frame_.push_back(c);
    if (frame_.size() == 2) {
      expect_ = ((frame_[0] & 0x07) << 8) | frame_[1];
      if (expect_ < 8 || expect_ > max_frame_) { frame_.clear(); in_frame_ = false; expect_ = 0; }
    }
    return false;
  }

  const uint8_t *plaintext() const { return apdu_.data(); }
  size_t plaintext_len() const { return apdu_.size(); }
  bool key_invalid() const { return key_invalid_; }
  bool encrypted_seen() const { return encrypted_seen_; }
  const uint8_t *system_title() const { return system_title_; }
  uint32_t frame_counter() const { return frame_counter_; }
  Stats stats;

  // Most recently closed raw HDLC frame (ciphertext + headers, flags stripped), and whether the
  // frame's FCS/HCS checked out -- true even for a frame that failed to decrypt (wrong key), since
  // that's a genuinely well-formed frame at the HDLC level. Never exposes key material.
  // frame_seq() increments on every closed frame; callers poll it to detect a new capture without
  // relying on feed()'s return value, which only fires on a full successful decode.
  const std::vector<uint8_t> &last_frame() const { return last_frame_; }
  bool last_frame_ok() const { return last_frame_ok_; }
  uint32_t frame_seq() const { return frame_seq_; }

  // Pattern for pm(x.y.z): the three decimal groups + 0xFF. Returns pattern length (0 if not parseable).
  static size_t obis_pattern(const char *obis, uint8_t out[8]) {
    size_t n = 0;
    const char *p = obis;
    while (*p && n < 6) {
      char *end;
      long v = strtol(p, &end, 10);
      if (end == p || v < 0 || v > 255) return 0;
      out[n++] = (uint8_t) v;
      p = end;
      if (*p == '.') p++; else if (*p) return 0;
    }
    if (n == 3) out[n++] = 0xFF;
    return n;
  }

  // Decode one typed DLMS value at p (rem bytes available). Value{} (found=false) if the tag is
  // not one we understand -- callers must treat that as "stop", never guess.
  static Value decode_value(const uint8_t *p, size_t rem) {
    Value v;
    if (rem < 2) return v;
    switch (p[0]) {
      case 0x09: case 0x0A:   // octet string / visible string
        if (rem < (size_t) 2 + p[1]) return v;
        v.found = true; v.is_string = true; v.str = p + 2; v.str_len = p[1]; return v;
      case 0x03: case 0x0F: case 0x11: case 0x16:   // bool, int8, uint8, enum
        v.found = true; v.num = (p[0] == 0x0F) ? (int8_t) p[1] : p[1]; return v;
      case 0x10: case 0x12:   // int16 / uint16
        if (rem < 3) return v;
        { uint16_t u = (uint16_t) ((p[1] << 8) | p[2]); v.found = true; v.num = (p[0] == 0x10) ? (int16_t) u : u; return v; }
      case 0x05: case 0x06:   // int32 / uint32
        if (rem < 5) return v;
        { uint32_t u = ((uint32_t) p[1] << 24) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 8) | p[4];
          v.found = true; v.num = (p[0] == 0x05) ? (double) (int32_t) u : (double) u; return v; }
      case 0x14: case 0x15:   // int64 / uint64
        if (rem < 9) return v;
        { uint64_t u = 0; for (int k = 1; k <= 8; k++) u = (u << 8) | p[k];
          v.found = true; v.num = (p[0] == 0x14) ? (double) (int64_t) u : (double) u; return v; }
      default: return v;
    }
  }

  // Byte length (tag included) of one DLMS-encoded item at p, to skip it without decoding.
  // Returns 0 on any tag not recognised: callers must refuse to guess past it.
  static size_t skip_len(const uint8_t *p, size_t rem) {
    if (rem < 1) return 0;
    switch (p[0]) {
      case 0x03: return 2;
      case 0x05: case 0x06: return 5;
      case 0x09: case 0x0A: if (rem < 2) return 0; return (size_t) 2 + p[1];
      case 0x0F: case 0x11: case 0x16: return 2;
      case 0x10: case 0x12: return 3;
      case 0x14: case 0x15: return 9;
      default: return 0;
    }
  }

  // Structural decode of a DLMS data-notification APDU -- the algorithm the real, hardware-proven
  // github.com/jluthiger/esphome-gplugk component uses (gplugk.cpp: decode_cosem_), replacing the
  // Tasmota-style substring search this file used before: strip the fixed 18-byte data-notification
  // header (tag 0x0F + 4-byte invoke-id + 1+12-byte date-time) if present, then walk the COSEM
  // STRUCTURE exactly once: an optional leading VISIBLE_STRING (meter name), followed by pairs of
  // (OCTET_STRING obis(6 bytes), typed value). Calls cb(obis6, value) for each pair found -- single
  // pass, no risk of a byte pattern matching somewhere it shouldn't.
  //
  // Does NOT handle the gPlugM/L+G "capture list" push (descriptor array + separate untagged value
  // array) -- that is a genuinely different structure; see the 2026-09-10 finding in intent.md.
  // Returns false if nothing was decoded (either the buffer isn't this structure, or it's empty).
  template<typename Callback>
  static bool decode_structure(const uint8_t *buf, size_t len, Callback &&cb) {
    size_t pos = 0;
    if (len >= 18 && buf[0] == 0x0F) pos = 18;              // strip data-notification header
    if (pos + 2 > len || buf[pos] != 0x02) return false;    // expect STRUCTURE
    uint16_t remaining = buf[pos + 1];
    pos += 2;
    if (pos < len && buf[pos] == 0x0A) {                    // optional VISIBLE_STRING (meter name)
      if (pos + 2 > len) return false;
      uint8_t nlen = buf[pos + 1];
      if (pos + 2 + (size_t) nlen > len || remaining == 0) return false;
      pos += 2 + nlen;
      remaining--;
    }
    bool any = false;
    for (uint16_t k = 0; k < remaining / 2; k++) {
      if (pos + 8 > len || buf[pos] != 0x09 || buf[pos + 1] != 6) break;
      const uint8_t *obis6 = buf + pos + 2;
      pos += 8;
      size_t vlen = skip_len(buf + pos, len - pos);
      if (!vlen) break;
      Value v = decode_value(buf + pos, len - pos);
      if (v.found) { cb(obis6, v); any = true; }
      pos += vlen;
    }
    return any;
  }

  // "Capture list" push (gPlugM / L+G E450/E570 meters, per the official product page the *entire*
  // supported meter list for gPlugM -- this is not a rare format). Anchor `02 N 01 N`: a STRUCTURE of
  // N elements whose first element is an ARRAY of N descriptor items, each
  // `{class(u16), obis(octet-string 6B), attribute-id(int8), dummy-value}`. Followed immediately by
  // more outer-structure elements -- the real, untagged readings.
  //
  // The index rule (verified against two independent real captures, 12 cross-checked values total,
  // including exact byte-offset matches to the device's own "GEAG pattern matched at pos N" debug
  // log): value array index = the descriptor's RANK among *distinct* 6-byte OBIS codes seen so far,
  // first-occurrence order (descriptors 0/1 in one capture share the identical clock OBIS and both
  // collapse to rank 0; ranks are NOT the raw loop index). An earlier attempt at this used the raw
  // loop index instead of a proper dedup rank and was wrongly concluded not to generalise -- it was a
  // bookkeeping bug, not a wrong theory; see the 2026-Jun findings in intent.md for the evidence.
  //
  // Our search pattern only carries the last 4 OBIS bytes (C.D.E.F, Tasmota-style), so when a meter
  // sends the same C.D.E.F under several A.B sub-indices (e.g. per-tariff or per-channel readings),
  // this returns the FIRST such descriptor's value, same ambiguity `find()` has.
  static Value find_capture_list(const uint8_t *buf, size_t len, const uint8_t *pat, size_t plen) {
    Value none;
    if (plen != 4) return none;
    for (size_t a = 0; a + 4 <= len; a++) {
      if (buf[a] != 0x02 || buf[a + 2] != 0x01) continue;
      uint8_t n = buf[a + 1];
      if (buf[a + 3] != n || n == 0 || n > 64) continue;

      size_t pos = a + 4;
      int match_rank = -1;
      unsigned rank = 0;
      uint8_t seen[64][6];
      uint8_t seen_n = 0;
      bool ok = true;
      for (uint8_t k = 0; k < n && ok; k++) {
        if (pos >= len || buf[pos] != 0x02) { ok = false; break; }
        uint8_t m = buf[pos + 1];
        pos += 2;
        const uint8_t *item_obis = nullptr;
        for (uint8_t e = 0; e < m && ok; e++) {
          if (pos >= len) { ok = false; break; }
          if (buf[pos] == 0x09 && pos + 8 <= len && buf[pos + 1] == 6) item_obis = buf + pos + 2;
          size_t sub = skip_len(buf + pos, len - pos);
          if (!sub) { ok = false; break; }
          pos += sub;
        }
        if (!ok) break;
        if (!item_obis) continue;   // malformed item: no logical name found, just skip ranking it
        unsigned this_rank;
        int found_at = -1;
        for (uint8_t s = 0; s < seen_n; s++)
          if (memcmp(seen[s], item_obis, 6) == 0) { found_at = s; break; }
        if (found_at >= 0) {
          this_rank = (unsigned) found_at;
        } else {
          this_rank = rank++;
          if (seen_n < 64) memcpy(seen[seen_n++], item_obis, 6);
        }
        if (match_rank < 0 && memcmp(item_obis + 2, pat, plen) == 0) match_rank = (int) this_rank;
      }
      if (!ok || match_rank < 0) continue;   // this anchor didn't parse, or pattern not in this list; try next

      size_t p = pos;
      bool skip_ok = true;
      for (int k = 0; k < match_rank; k++) {
        if (p >= len) { skip_ok = false; break; }
        size_t sub = skip_len(buf + p, len - p);
        if (!sub) { skip_ok = false; break; }
        p += sub;
      }
      if (!skip_ok || p >= len) continue;
      Value v = decode_value(buf + p, len - p);
      if (v.found) return v;
    }
    return none;
  }

  // Tasmota-compatible lookup: first occurrence of pattern, value decoded from the COSEM tag after it.
  // find() tries find_capture_list() first (see above); if that finds nothing (not a capture-list
  // buffer, or the pattern isn't in one), falls back to flat substring search for simple meters
  // (DSMR, Kamstrup-style DLMS push).
  static Value find(const uint8_t *buf, size_t len, const uint8_t *pat, size_t plen) {
    { Value v = find_capture_list(buf, len, pat, plen); if (v.found) return v; }
    Value v;
    if (!plen || len < plen) return v;
    for (size_t i = 0; i + plen <= len; i++) {
      if (memcmp(buf + i, pat, plen) != 0) continue;
      const uint8_t *p = buf + i + plen;
      size_t rem = len - (i + plen);
      if (rem < 2) return v;
      switch (p[0]) {
        case 0x09: case 0x0A:   // octet string / visible string
          if (rem < (size_t) 2 + p[1]) return v;
          v.found = true; v.is_string = true; v.str = p + 2; v.str_len = p[1]; return v;
        case 0x03: case 0x0F: case 0x11: case 0x16:   // bool, int8, uint8, enum
          v.found = true; v.num = (p[0] == 0x0F) ? (int8_t) p[1] : p[1]; return v;
        case 0x10: case 0x12:   // int16 / uint16
          if (rem < 3) return v;
          { uint16_t u = (uint16_t) ((p[1] << 8) | p[2]); v.found = true; v.num = (p[0] == 0x10) ? (int16_t) u : u; return v; }
        case 0x05: case 0x06:   // int32 / uint32
          if (rem < 5) return v;
          { uint32_t u = ((uint32_t) p[1] << 24) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 8) | p[4];
            v.found = true; v.num = (p[0] == 0x05) ? (double) (int32_t) u : (double) u; return v; }
        case 0x14: case 0x15:   // int64 / uint64
          if (rem < 9) return v;
          { uint64_t u = 0; for (int k = 1; k <= 8; k++) u = (u << 8) | p[k];
            v.found = true; v.num = (p[0] == 0x14) ? (double) (int64_t) u : (double) u; return v; }
        default:
          return v;   // pattern hit something that is not a value (e.g. an OBIS list); Tasmota returns 0 too
      }
    }
    return v;
  }

 private:
  // One HDLC frame (without the 0x7E flags) is in frame_.
  bool on_frame_() {
    stats.frames++;
    last_frame_crc_ok_ = false;
    const uint8_t *d = frame_.data();
    size_t n = frame_.size();
    if ((d[0] & 0xF0) != 0xA0) return false;                    // frame format type 3 only
    size_t len = (((d[0] & 0x07) << 8) | d[1]);                 // length field counts everything between flags
    if (len != n) return false;
    if (crc16_x25(d, n - 2) != (uint16_t) (d[n - 2] | (d[n - 1] << 8))) { stats.fcs_errors++; return false; }
    size_t i = 2;
    while (i < n && !(d[i] & 1)) i++; i++;                      // dest address (LSB=1 on last byte)
    while (i < n && !(d[i] & 1)) i++; i++;                      // src address
    if (i + 3 > n) return false;
    i++;                                                        // control
    if (crc16_x25(d, i) != (uint16_t) (d[i] | (d[i + 1] << 8))) { stats.hcs_errors++; return false; }
    i += 2;
    // FCS+HCS both valid from here: a genuinely well-formed HDLC frame, independent of whether the
    // payload below decrypts/authenticates -- this is the "CRC ok" the Datenstrom capture reports.
    last_frame_crc_ok_ = true;
    const uint8_t *p = d + i;
    size_t plen = n - 2 - i;
    return on_payload_(p, plen);
  }

  bool on_payload_(const uint8_t *p, size_t n) {
    if (n >= 3 && p[0] == 0xE6 && p[1] == 0xE7 && p[2] == 0x00) { p += 3; n -= 3; }
    if (n && p[0] == 0xE0) {
      // General-Block-Transfer segment. Real (library-confirmed) header layout is fixed 7 bytes:
      // flag(1) control(1) sequence(u16 BE) block-number-ack(u16 BE) size(1), then `size` payload bytes.
      // control bit 0x80 = last segment. (An earlier guessed variable-length-prefix layout was wrong;
      // caught by a real capture where segment sizes are 112/108/121 bytes, not 0x81/0x82-prefixed.)
      if (n < 7) return false;
      uint8_t control = p[1];
      uint16_t seq = (uint16_t) ((p[2] << 8) | p[3]);
      uint8_t blen = p[6];
      bool last = control & 0x80;
      size_t off = 7;
      if (off + blen > n) return false;
      if (seq == 1) gbt_.clear();
      gbt_.insert(gbt_.end(), p + off, p + off + blen);
      if (!last) return false;
      std::vector<uint8_t> whole; whole.swap(gbt_);
      if (!whole.empty() && whole[0] == 0xDB) return on_apdu_(whole.data(), whole.size());
      // Unencrypted, GBT-reassembled: the whole buffer is the final APDU (may still carry a leading
      // DLMS-tag header; find() searches the whole buffer so an unstripped header does not matter).
      apdu_.assign(whole.begin(), whole.end());
      stats.apdus++;
      return true;
    }
    if (n && p[0] == 0xDB) {                                    // start of a (possibly multi-frame) ciphered APDU
      pending_.assign(p, p + n);
      pending_total_ = ciphered_total_len_(p, n);
      if (pending_total_ && pending_.size() < pending_total_) return false;   // wait for more HDLC frames
      std::vector<uint8_t> whole; whole.swap(pending_); pending_total_ = 0;
      return on_apdu_(whole.data(), whole.size());
    }
    if (pending_total_) {                                       // continuation of a split ciphered APDU
      pending_.insert(pending_.end(), p, p + n);
      if (pending_.size() < pending_total_) return false;
      std::vector<uint8_t> whole; whole.swap(pending_); pending_total_ = 0;
      return on_apdu_(whole.data(), whole.size());
    }
    if (n && p[0] == 0x0F) {                                    // unencrypted data-notification
      apdu_.assign(p, p + n);
      stats.apdus++;
      return true;
    }
    return false;
  }

  // total length of a DB APDU from its header (0 if unknown)
  static size_t ciphered_total_len_(const uint8_t *p, size_t n) {
    if (n < 3) return 0;
    size_t st = p[1], i = 2 + st;
    if (i >= n) return 0;
    size_t len, lf;
    if (p[i] == 0x82) { if (i + 3 > n) return 0; len = (p[i + 1] << 8) | p[i + 2]; lf = 3; }
    else if (p[i] == 0x81) { if (i + 2 > n) return 0; len = p[i + 1]; lf = 2; }
    else { len = p[i]; lf = 1; }
    return i + lf + len;
  }

  bool on_apdu_(uint8_t *p, size_t n) {
    if (p[0] != 0xDB) return false;
    encrypted_seen_ = true;
    if (!have_key_) { key_invalid_ = true; return false; }
    size_t st = p[1];
    if (st != 8 || n < 2 + st + 1 + 1 + 4) return false;
    memcpy(system_title_, p + 2, 8);
    size_t i = 2 + st;
    size_t len;
    if (p[i] == 0x82) { len = (p[i + 1] << 8) | p[i + 2]; i += 3; }
    else if (p[i] == 0x81) { len = p[i + 1]; i += 2; }
    else { len = p[i]; i += 1; }
    if (i + len > n || len < 5) return false;
    uint8_t sec = p[i];
    frame_counter_ = ((uint32_t) p[i + 1] << 24) | ((uint32_t) p[i + 2] << 16) | ((uint32_t) p[i + 3] << 8) | p[i + 4];
    uint8_t iv[12];
    memcpy(iv, system_title_, 8);
    memcpy(iv + 8, p + i + 1, 4);
    bool authenticated = sec & 0x10;
    size_t tag_len = authenticated ? 12 : 0;
    size_t ct_len = len - 5 - tag_len;
    uint8_t *ct = p + i + 5;
    uint8_t aad[17] = {sec};
    size_t aad_len = 0;
    if (authenticated && have_ak_) { memcpy(aad + 1, ak_, 16); aad_len = 17; }
    uint8_t tag[16];
    gplug_aes::Gcm gcm(key_);
    gcm.run(true, iv, aad, aad_len, ct, ct_len, tag);
    if (authenticated && have_ak_ && memcmp(tag, ct + ct_len, 12) != 0) {
      stats.auth_failed++; key_invalid_ = true; return false;
    }
    // Without an authentication key we cannot verify the tag; sanity-check the plaintext instead.
    key_invalid_ = ct[0] != 0x0F;
    if (key_invalid_) return false;
    apdu_.assign(ct, ct + ct_len);
    stats.apdus++;
    return true;
  }

  std::vector<uint8_t> frame_, apdu_, gbt_, pending_;
  size_t pending_total_{0};
  size_t expect_{0};
  bool in_frame_{false};
  size_t max_frame_{1280};
  uint8_t key_[16]{}, ak_[16]{}, system_title_[8]{};
  uint32_t frame_counter_{0};
  bool have_key_{false}, have_ak_{false}, key_invalid_{false}, encrypted_seen_{false};

  std::vector<uint8_t> last_frame_;
  bool last_frame_ok_{false}, last_frame_crc_ok_{false};
  uint32_t frame_seq_{0};
};

}  // namespace gplug_dlms
