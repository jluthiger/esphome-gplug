// Protocol sniffer: what the HAN line speaks, read off the first bytes of every frame or telegram
// rather than from a full decode. That makes the verdict available a few seconds after the
// hardware step -- before any profile (protocol, OBIS map, key) exists -- so the wizard can propose
// the profile instead of asking the user to know it. Header-only, no ESPHome deps, host-testable
// (firmware/test/test_sniff.cpp).
//
//   DSMR  a P1 telegram opens with the IEC 62056-21 identification line: '/' + 3-letter
//         manufacturer id + baud-rate digit, e.g. "/KFM5KAIFA-METER", "/LGF5E360". Letters only:
//         "/2026" in a date must not count.
//   DLMS  an HDLC type-3 frame opens with the flag 0x7E followed by a format byte 0xA0..0xAF.
//         Whether the APDU inside is ciphered is read from the tag after the LLC header E6 E7 00:
//         0xDB = general-glo-ciphering (encrypted), 0x0F = plain data-notification. A GBT segment
//         (0xE0, fixed 7-byte header) is skipped to the same tag.
//
// The verdict is the protocol with more header hits since reset(): DLMS ciphertext can contain a
// stray "/XYZ5" (about once per 5e5 bytes) and must not flip the verdict, while 0xA0..0xAF never
// occur in DSMR's ASCII, so a tie goes to DLMS.
#pragma once
#include <cstddef>
#include <cstdint>

namespace gplug_sniff {

class ProtocolSniffer {
 public:
  enum Protocol : uint8_t { NONE, DSMR, DLMS };
  enum Tri : uint8_t { UNKNOWN, NO, YES };

  void reset() { *this = ProtocolSniffer(); }

  // Feed one byte. Returns true when this byte completed a header hit, so the caller can timestamp it.
  bool feed(uint8_t c) {
    bool hit = false;
    // DSMR identification line: '/' XXX d. A '/' anywhere restarts the match.
    if (c == '/') {
      dsmr_pos_ = 1;
    } else if (dsmr_pos_ >= 1 && dsmr_pos_ <= 3) {
      dsmr_pos_ = is_letter_(c) ? dsmr_pos_ + 1 : 0;
    } else if (dsmr_pos_ == 4) {
      dsmr_pos_ = 0;
      if (c >= '0' && c <= '9') { dsmr_hits_++; hit = true; }
    }
    // HDLC: 7E A? ... E6 E7 00 tag
    switch (dlms_st_) {
      case FLAG:
        if (c == 0x7E) dlms_st_ = FORMAT;
        break;
      case FORMAT:
        if (c == 0x7E) break;                                   // 7E 7E: still at frame start
        if ((c & 0xF0) == 0xA0) { dlms_st_ = LLC; dlms_n_ = 0; llc_pos_ = 0; dlms_hits_++; hit = true; }
        else dlms_st_ = FLAG;
        break;
      case LLC:                                                 // addresses + control + HCS, then E6 E7 00
        llc_pos_ = (llc_pos_ == 0 && c == 0xE6) || (llc_pos_ == 1 && c == 0xE7) || (llc_pos_ == 2 && c == 0x00)
                       ? llc_pos_ + 1 : (c == 0xE6 ? 1 : 0);
        if (llc_pos_ == 3) { dlms_st_ = TAG; skip_ = 0; }
        else if (++dlms_n_ > LLC_WINDOW) dlms_st_ = FLAG;       // no LLC this deep in: not a frame we know
        break;
      case TAG:
        if (skip_) { skip_--; break; }
        if (c == 0xE0) { skip_ = 6; break; }                    // GBT: control, seq(2), ack(2), size -> payload tag
        if (c == 0xDB) encrypted_ = YES;
        else if (c == 0x0F) encrypted_ = NO;
        dlms_st_ = FLAG;                                        // anything else: continuation, not conclusive
        break;
    }
    return hit;
  }

  Protocol protocol() const {
    if (dsmr_hits_ == 0 && dlms_hits_ == 0) return NONE;
    return dlms_hits_ >= dsmr_hits_ ? DLMS : DSMR;
  }
  // Ciphering as seen on the line. DSMR (P1 as used here) is always plaintext.
  Tri encrypted() const {
    switch (protocol()) {
      case DSMR: return NO;
      case DLMS: return encrypted_;
      default: return UNKNOWN;
    }
  }
  // Header hits of the winning protocol -- how much evidence the verdict rests on.
  uint32_t hits() const { return protocol() == DLMS ? dlms_hits_ : protocol() == DSMR ? dsmr_hits_ : 0; }

 private:
  // The LLC sits after 2 format bytes, 1-4 bytes each of destination and source address, 1 control
  // byte and a 2-byte HCS: at most 13 bytes in, 16 leaves slack.
  static constexpr uint8_t LLC_WINDOW = 16;
  enum DlmsState : uint8_t { FLAG, FORMAT, LLC, TAG };
  static bool is_letter_(uint8_t c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
  uint8_t dsmr_pos_{0};
  DlmsState dlms_st_{FLAG};
  uint8_t dlms_n_{0}, llc_pos_{0}, skip_{0};
  uint32_t dsmr_hits_{0}, dlms_hits_{0};
  Tri encrypted_{UNKNOWN};
};

}  // namespace gplug_sniff
