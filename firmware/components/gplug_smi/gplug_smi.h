#pragma once
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "dsmr_parser.h"
#include "dlms_decoder.h"
#include "frame_log.h"

#include <array>
#include <mutex>
#include <string>
#include <vector>

namespace esphome {
namespace gplug_smi {

static constexpr size_t MAX_OBIS = 48;
static constexpr size_t RING_LEN = 360;      // 60 min at 10 s
static constexpr uint32_t RING_PERIOD_MS = 10000;
// Datenstrom capture: last FRAME_LOG_LEN raw DLMS HDLC frames, each capped at FRAME_LOG_CAP bytes
// (real captured frames run 500-700B; 768 leaves headroom without paying for the 1280B worst case).
static constexpr size_t FRAME_LOG_LEN = 5;
static constexpr size_t FRAME_LOG_CAP = 768;
using FrameLog = ::gplug_framelog::FrameLog<FRAME_LOG_LEN, FRAME_LOG_CAP>;

struct ObisEntry {
  char obis[24];
  uint8_t pat[8];   // DLMS: byte pattern searched in the plaintext (C D E FF)
  uint8_t plen;
  char name[12];
  char unit[8];
  float scale;      // Tasmota semantics: value = raw / scale
  uint8_t precision;
  bool is_string;
};

struct Descriptor {
  enum Protocol : uint8_t { NONE, DSMR, DLMS } protocol{NONE};
  uint32_t baud{115200};
  int8_t rx{-1};
  uint8_t serial_flags{0};   // Tasmota so2: bit2 = invert RX line, bit3 = no pullup
  bool parity_even{false};   // Tasmota mode "rE1"
  uint16_t buffer{0};
  bool encrypted{false};
  bool has_auth_key{false};
  uint8_t key[16]{};
  uint8_t auth_key[16]{};
  uint8_t n{0};
  ObisEntry obis[MAX_OBIS];
  std::string preset;
};

struct Sample { int16_t pi, po, p1, p2, p3; };   // W

class GplugSmi : public Component, public uart::UARTDevice, public AsyncWebHandler {
 public:
  explicit GplugSmi(web_server_base::WebServerBase *base) : base_(base) {}

  void set_spa(const uint8_t *data, size_t len) { spa_ = data; spa_len_ = len; }
  void set_presets(const uint8_t *data, size_t len) { presets_ = data; presets_len_ = len; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  // AsyncWebHandler
  bool canHandle(AsyncWebServerRequest *request) const override;
  void handleRequest(AsyncWebServerRequest *request) override;
  bool isRequestHandlerTrivial() const override { return false; }

 protected:
  // config persistence (NVS namespace "gplug", keys "hw", "meter")
  bool nvs_load_(const char *key, std::string &out);
  bool nvs_save_(const char *key, const std::string &value);
  bool apply_meter_json_(const std::string &json, std::string &err);
  bool apply_hw_json_(const std::string &json, std::string &err);
  void apply_uart_();
  void apply_button_pin_();
  void poll_button_();
  void apply_led_pins_();
  void update_led_();

  // decoding
  void on_dsmr_value_(const ::gplug_dsmr::DsmrValue &v);
  void on_dlms_apdu_();
  void apply_dlms_value_(ObisEntry &e, uint8_t i, const ::gplug_dlms::Value &v);
  float value_w_(const char *name) const;   // value of a named sensor converted to W, 0 if absent
  float value_(const char *name, float def) const;

  // http
  void send_gz_(AsyncWebServerRequest *req, const char *ctype, const uint8_t *data, size_t len);
  void send_json_(AsyncWebServerRequest *req, int code, const std::string &body);
  bool read_body_(AsyncWebServerRequest *req, std::string &out);
  std::string json_status_();
  std::string json_live_();
  std::string json_ring_();
  std::string json_frames_();
  void handle_frame_detail_(AsyncWebServerRequest *req, const char *url);
  std::string json_wifi_scan_();

  web_server_base::WebServerBase *base_;
  const uint8_t *spa_{nullptr}; size_t spa_len_{0};
  const uint8_t *presets_{nullptr}; size_t presets_len_{0};

  Descriptor desc_;
  std::string hw_json_{"{}"};
  int8_t button_pin_num_{-1};
  GPIOPin *button_gpio_{nullptr};
  bool button_down_{false};
  uint32_t button_down_ms_{0};
  bool button_ap_triggered_{false};
  int8_t led_red_pin_num_{-1};
  int8_t led_green_pin_num_{-1};
  int8_t led_blue_pin_num_{-1};
  GPIOPin *led_red_gpio_{nullptr};
  GPIOPin *led_green_gpio_{nullptr};
  GPIOPin *led_blue_gpio_{nullptr};
  bool led_blink_on_{false};
  uint32_t led_blink_last_ms_{0};
  int8_t led_mode_{-1};
  int8_t led_pending_mode_{-1};
  uint32_t led_pending_since_ms_{0};
  bool led_ap_prev_{false};
  bool setup_pending_{false};   // just left AP-fallback; stay in setup until meter commit or first frame
  bool no_data_{false};         // LED's no-data verdict, exposed in /api/live so the SPA agrees with the LED
  ::gplug_dsmr::DsmrParser dsmr_;
  ::gplug_dlms::DlmsDecoder dlms_;

  // live state, guarded by mutex_ (HTTP runs on the httpd task)
  mutable std::mutex mutex_;
  float values_[MAX_OBIS];
  bool have_[MAX_OBIS]{};
  char smid_[40]{};
  uint32_t last_frame_ms_{0};
  uint32_t meter_applied_ms_{0};
  bool key_invalid_{false};
  std::array<Sample, RING_LEN> ring_{};
  size_t ring_head_{0}, ring_count_{0};
  uint32_t last_sample_ms_{0};
  FrameLog frames_;             // Datenstrom capture ring, DLMS only (see loop())
  uint32_t dlms_frame_seq_seen_{0};
};

}  // namespace gplug_smi
}  // namespace esphome
