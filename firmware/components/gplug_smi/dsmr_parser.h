// Minimal DSMR / P1 ASCII telegram parser. No ESPHome dependencies so it can be
// unit-tested on the host (see firmware/test/test_dsmr.cpp).
//
// Telegram: '/' ident CRLF CRLF  lines...  '!' CRC16 CRLF
// Line:     OBIS '(' value ')' [ '(' value ')' ]...   e.g. 1-0:1.7.0(00.123*kW)
#pragma once
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>

namespace gplug_dsmr {

struct DsmrValue {
  const char *obis;    // e.g. "1-0:1.7.0" (NUL-terminated, points into telegram buffer)
  const char *value;   // last parenthesised group without unit, e.g. "00.123"
  const char *unit;    // "kW" or "" if none
};

class DsmrParser {
 public:
  using Callback = std::function<void(const DsmrValue &)>;

  // Feed one byte. Returns true when a complete telegram was consumed (and callbacks fired).
  bool feed(char c, const Callback &cb) {
    if (c == '/') {                     // start of telegram
      len_ = 0; in_telegram_ = true;
    }
    if (!in_telegram_) return false;
    if (len_ >= sizeof(buf_) - 1) {     // overflow → drop
      in_telegram_ = false; len_ = 0; ++overflows;
      return false;
    }
    buf_[len_++] = c;
    if (c == '\n' && crc_pending_) {    // line after '!' complete → end of telegram
      buf_[len_] = 0;
      crc_pending_ = false; in_telegram_ = false;
      bool ok = check_crc_();
      if (ok) parse_(cb); else ++crc_errors;
      ++telegrams;
      len_ = 0;
      return ok;
    }
    if (c == '!' && (len_ == 1 || buf_[len_ - 2] == '\n')) crc_pending_ = true;
    return false;
  }

  uint32_t telegrams{0}, crc_errors{0}, overflows{0};
  bool require_crc{true};

  static uint16_t crc16(const char *data, size_t n) {   // CRC16/ARC as used by DSMR
    uint16_t crc = 0;
    for (size_t i = 0; i < n; i++) {
      crc ^= (uint8_t) data[i];
      for (int b = 0; b < 8; b++) crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
    return crc;
  }

 private:
  bool check_crc_() {
    char *bang = nullptr;
    for (size_t i = len_; i > 0; i--) if (buf_[i - 1] == '!') { bang = buf_ + i - 1; break; }
    if (!bang) return false;
    if (!require_crc) return true;
    size_t data_len = (bang - buf_) + 1;
    uint16_t want = (uint16_t) strtoul(bang + 1, nullptr, 16);
    if (bang[1] == '\r' || bang[1] == '\n') return true;   // DSMR 2/3 without CRC
    return crc16(buf_, data_len) == want;
  }

  void parse_(const Callback &cb) {
    char *p = buf_;
    while (*p) {
      char *eol = strpbrk(p, "\r\n");
      if (!eol) eol = p + strlen(p);
      char save = *eol; *eol = 0;
      parse_line_(p, cb);
      *eol = save;
      p = eol;
      while (*p == '\r' || *p == '\n') p++;
    }
  }

  void parse_line_(char *line, const Callback &cb) {
    if (line[0] == '/' || line[0] == '!' || line[0] == 0) return;
    char *open = strchr(line, '(');
    if (!open) return;
    *open = 0;
    // take the last "(...)" group
    char *last_open = open + 1, *q = open + 1;
    while ((q = strchr(q, '(')) != nullptr) { last_open = q + 1; q++; }
    char *close = strchr(last_open, ')');
    if (!close) return;
    *close = 0;
    char *star = strchr(last_open, '*');
    const char *unit = "";
    if (star) { *star = 0; unit = star + 1; }
    cb(DsmrValue{line, last_open, unit});
  }

  char buf_[2048];
  size_t len_{0};
  bool in_telegram_{false}, crc_pending_{false};
};

}  // namespace gplug_dsmr
