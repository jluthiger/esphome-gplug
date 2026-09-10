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

template <size_t N, size_t CAP>
class FrameLog {
 public:
  struct Entry {
    uint32_t ts_ms{0};
    uint16_t raw_len{0}, plain_len{0};
    bool ok{false};            // HDLC FCS/HCS valid (see DlmsDecoder::last_frame_ok())
    bool raw_trunc{false}, plain_trunc{false};
    std::array<uint8_t, CAP> raw{};
    std::array<uint8_t, CAP> plain{};
  };

  void push(uint32_t ts_ms, bool ok, const uint8_t *raw, size_t raw_len,
            const uint8_t *plain, size_t plain_len) {
    Entry &e = buf_[head_];
    e.ts_ms = ts_ms;
    e.ok = ok;
    e.raw_len = (uint16_t) (raw_len < CAP ? raw_len : CAP);
    e.raw_trunc = raw_len > CAP;
    if (e.raw_len) memcpy(e.raw.data(), raw, e.raw_len);
    e.plain_len = (uint16_t) (plain_len < CAP ? plain_len : CAP);
    e.plain_trunc = plain_len > CAP;
    if (e.plain_len) memcpy(e.plain.data(), plain, e.plain_len);
    head_ = (head_ + 1) % N;
    if (count_ < N) count_++;
  }

  size_t count() const { return count_; }
  static constexpr size_t len() { return N; }
  static constexpr size_t cap() { return CAP; }

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
