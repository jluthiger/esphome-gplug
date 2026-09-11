// Fixed-size ring of the last N raw DLMS HDLC frames (ciphertext + decrypted plaintext, when
// available), for the SPA's Datenstrom (raw frame stream) view. Header-only, no ESPHome deps,
// host-testable (firmware/test/test_framelog.cpp) -- same convention as dlms_decoder.h.
//
// Deliberately a generic byte-blob ring with no notion of "key": it copies whatever raw/plaintext
// bytes it's given and nothing else, so it is structurally incapable of exposing key material.
#pragma once
#include <array>
#include <cstdint>
#include <cstring>

namespace gplug_framelog {

// RAW_CAP and PLAIN_CAP are separate because the two halves are not the same size problem: `raw`
// holds a DLMS HDLC frame (up to the descriptor's buffer setting, 1280 B default) or a whole DSMR
// ASCII telegram, while `plain` only ever holds a decrypted DLMS APDU -- and is unused entirely on
// DSMR, which has nothing to decrypt.
template <size_t N, size_t RAW_CAP, size_t PLAIN_CAP>
class FrameLog {
 public:
  struct Entry {
    uint32_t ts_ms{0};
    uint16_t raw_len{0}, plain_len{0};
    bool ok{false};            // HDLC FCS/HCS valid, or DSMR telegram CRC valid
    bool raw_trunc{false}, plain_trunc{false};
    std::array<uint8_t, RAW_CAP> raw{};
    std::array<uint8_t, PLAIN_CAP> plain{};
  };

  void push(uint32_t ts_ms, bool ok, const uint8_t *raw, size_t raw_len,
            const uint8_t *plain, size_t plain_len) {
    Entry &e = buf_[head_];
    e.ts_ms = ts_ms;
    e.ok = ok;
    e.raw_len = (uint16_t) (raw_len < RAW_CAP ? raw_len : RAW_CAP);
    e.raw_trunc = raw_len > RAW_CAP;
    if (e.raw_len) memcpy(e.raw.data(), raw, e.raw_len);
    e.plain_len = (uint16_t) (plain_len < PLAIN_CAP ? plain_len : PLAIN_CAP);
    e.plain_trunc = plain_len > PLAIN_CAP;
    if (e.plain_len) memcpy(e.plain.data(), plain, e.plain_len);
    head_ = (head_ + 1) % N;
    if (count_ < N) count_++;
  }

  size_t count() const { return count_; }
  static constexpr size_t len() { return N; }
  static constexpr size_t raw_cap() { return RAW_CAP; }
  static constexpr size_t plain_cap() { return PLAIN_CAP; }

  // i=0 is the most recently pushed entry; nullptr if i is out of range.
  const Entry *at(size_t i) const {
    if (i >= count_) return nullptr;
    return &buf_[(head_ + N - 1 - i) % N];
  }

 private:
  std::array<Entry, N> buf_{};
  size_t head_{0}, count_{0};
};

}  // namespace gplug_framelog
