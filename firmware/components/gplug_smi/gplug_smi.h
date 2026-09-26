#pragma once
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/components/esp32/gpio.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "dsmr_parser.h"
#include "dlms_decoder.h"
#include "frame_log.h"
#include "event_log.h"
#include "history_store.h"
#include "partition_flash.h"
#include "protocol_sniff.h"
#include "ha_values.h"
#include "heap_monitor.h"
#include "mqtt_template.h"
#include "esphome/core/defines.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_UPDATE
#include "esphome/components/update/update_entity.h"
#endif

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct esp_mqtt_client;   // esp-mqtt's handle; mqtt_client.h stays out of this header

namespace esphome {
namespace gplug_smi {

static constexpr size_t MAX_OBIS = 48;
static constexpr size_t RING_LEN = 360;      // 60 min at 10 s
static constexpr uint32_t RING_PERIOD_MS = 10000;
// Heap trend (heap_monitor.h): 288 x 12 B = 3.4 kB of RAM for 24 h, never written to flash.
static constexpr size_t MEM_RING_LEN = 288;
static constexpr uint32_t MEM_PERIOD_MS = 300000;
// Datenstrom capture: the last FRAME_LOG_LEN frames as received -- DLMS HDLC frames or whole DSMR
// telegrams, both raw and (for DLMS) decrypted. 1280 B covers the descriptor buffer ceiling.
static constexpr size_t FRAME_LOG_LEN = 5;
static constexpr size_t FRAME_LOG_RAW_CAP = 1280;    // a DLMS HDLC frame or a whole DSMR telegram
static constexpr size_t FRAME_LOG_PLAIN_CAP = 768;   // decrypted DLMS APDU; unused on DSMR
using FrameLog = ::gplug_framelog::FrameLog<FRAME_LOG_LEN, FRAME_LOG_RAW_CAP, FRAME_LOG_PLAIN_CAP>;
// Persistent history: one record per quarter hour on the `data` partition (see history_store.h).
// Lowering HIST_INTERVAL_S in a dev build is the only practical way to exercise sector rotation --
// at 900 s a sector lasts 2.6 days.
static constexpr uint32_t HIST_INTERVAL_S = ::gplug_hist::HIST_INTERVAL_S;
static constexpr uint32_t HIST_TICKS_PER_INTERVAL = HIST_INTERVAL_S * 1000 / RING_PERIOD_MS;
using HistoryStore = ::gplug_hist::HistoryStore<PartitionFlash>;

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
  char key_hint[5]{};        // first 4 hex digits of SHA-256(key), see apply_meter_json_
  uint8_t n{0};
  ObisEntry obis[MAX_OBIS];
  std::string preset;
};

struct Sample { int16_t pi, po, p1, p2, p3; };   // W

// MQTT publishing (issue 14, mqtt.cpp): the settings as stored in NVS key "mqtt" and returned by
// GET /api/config/mqtt (password excepted).
struct MqttSettings {
  bool enabled{false};
  bool each{false};            // one message per value instead of one per period
  bool retain{false};
  uint8_t qos{0};
  uint16_t port{1883};
  uint16_t period{10};         // s
  char host[64]{};
  char client_id[65]{};        // empty = the device name
  char user[65]{};
  char password[65]{};
  char topic[::gplug_mqtt::TOPIC_TPL_MAX + 1]{};
  char payload[::gplug_mqtt::PAYLOAD_TPL_MAX + 1]{};
};

// What the loop task publishes from: the settings, both templates compiled against one meter
// profile, and that profile's register strings. The strings are a copy so rendering can run
// outside mutex_ while a meter save replaces desc_. ~7 kB, allocated only while MQTT is enabled
// (and briefly on the httpd task to validate a POST).
struct MqttRun {
  MqttSettings cfg;
  uint32_t gen{0};             // GplugSmi::desc_gen_ the templates were compiled against
  bool ok{false};              // false: the profile changed and the templates no longer compile
  uint8_t n{0};
  char names[MAX_OBIS][12];
  char obis[MAX_OBIS][24];
  char units[MAX_OBIS][8];
  const char *np[MAX_OBIS], *op[MAX_OBIS], *up[MAX_OBIS];
  uint8_t prec[MAX_OBIS];
  bool is_string[MAX_OBIS];
  float v[MAX_OBIS];
  bool have[MAX_OBIS];
  char smid[40];
  char mac[13];
  ::gplug_mqtt::Compiled topic, payload;
  char topic_buf[::gplug_mqtt::TOPIC_BUF];
  char payload_buf[::gplug_mqtt::PAYLOAD_BUF];
};

// A POST or recompile failure, as the API reports it: {"error":code,"field":…,"pos":…}.
struct MqttErr {
  const char *code{nullptr};
  const char *field{nullptr};  // "topic" / "payload" for template errors
  uint16_t pos{0};
  uint32_t worst{0};
};

class GplugSmi : public Component, public uart::UARTDevice, public AsyncWebHandler {
 public:
  explicit GplugSmi(web_server_base::WebServerBase *base) : base_(base) {}

  void set_spa(const uint8_t *data, size_t len) { spa_ = data; spa_len_ = len; }
  void set_presets(const uint8_t *data, size_t len) { presets_ = data; presets_len_ = len; }
  void set_home_screen(const uint8_t *manifest_gz, size_t manifest_len, const uint8_t *icon_png, size_t icon_len) {
    manifest_ = manifest_gz; manifest_len_ = manifest_len; icon_ = icon_png; icon_len_ = icon_len;
  }
  void set_ota_auth(bool on) { ota_auth_ = on; }
  void set_ota_password(const char *pw) { ota_password_ = pw; }
#ifdef USE_UPDATE
  // Install from the release manifest (issue 12). T is http_request's HttpRequestUpdate: the entity
  // that fetches the manifest and flashes, and the component whose error flag is the only sign a
  // manifest check failed (it publishes no state then). Templated so this header needs no
  // http_request include; the generated main.cpp instantiates it with the concrete type.
  template<typename T> void set_update(T *u) { upd_ = u; upd_comp_ = u; }
#endif
  // Home Assistant entities (sensor.py / text_sensor.py). Any of them may be left out of the YAML.
#ifdef USE_SENSOR
  void set_ha_sensor(uint8_t key, sensor::Sensor *s) { if (key < ::gplug_ha::HA_COUNT) ha_sensors_[key] = s; }
  void set_frame_age_sensor(sensor::Sensor *s) { frame_age_sensor_ = s; }
  void set_free_heap_sensor(sensor::Sensor *s) { free_heap_sensor_ = s; }
  void set_largest_block_sensor(sensor::Sensor *s) { largest_block_sensor_ = s; }
#endif
#ifdef USE_TEXT_SENSOR
  void set_meter_status_text(text_sensor::TextSensor *s) { meter_status_text_ = s; }
  void set_meter_id_text(text_sensor::TextSensor *s) { meter_id_text_ = s; }
#endif

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
  void release_pin_(uint8_t num);
  void poll_button_();
  void apply_led_pins_();
  void update_led_();
  const char *diag_(uint32_t now) const;
  bool merge_stored_keys_(std::string &body, std::string &err);

  // decoding
  void on_dsmr_value_(const ::gplug_dsmr::DsmrValue &v);
  void on_dlms_apdu_();
  void apply_dlms_value_(ObisEntry &e, uint8_t i, const ::gplug_dlms::Value &v);
  void note_energy_exact_(const ObisEntry &e, double raw);
  float value_w_(const char *name) const;   // value of a named sensor converted to W, 0 if absent
  float value_(const char *name, float def) const;
  // Home Assistant publishing, from the 10 s sample tick (see ha_values.h).
  struct HaTick {
    ::gplug_ha::HaSnapshot snap;
    bool meter_ok;
    int32_t frame_age_s;          // -1 = no frame since boot
    const char *diag;             // static string from diag_()
    char smid[40];
  };
  void ha_collect_(uint32_t now, bool meter_ok, HaTick &t) const;   // caller holds mutex_
  void ha_publish_(const HaTick &t);                                   // outside mutex_

  // http
  void send_gz_(AsyncWebServerRequest *req, const char *ctype, const uint8_t *data, size_t len);
  void send_json_(AsyncWebServerRequest *req, int code, const std::string &body);
  bool read_body_(AsyncWebServerRequest *req, std::string &out);
  std::string json_status_();
  std::string json_update_();
  void handle_update_post_(AsyncWebServerRequest *req, bool install);
  void update_service_();
  void update_start_check_();
  void update_snapshot_(bool check_done);
  std::string json_live_();
  std::string json_ring_();
  std::string json_frames_();
  void handle_frame_detail_(AsyncWebServerRequest *req, const char *url);
  std::string json_history_(const char *range);
  std::string json_log_();
  void handle_heap_(AsyncWebServerRequest *req);
  void mem_service_();
  // Event log (event_log.h): rare, persistent "what happened" records in NVS.
  void log_setup_();
  void log_event_(uint8_t code, uint8_t detail = 0, uint8_t value = 0);
  void log_service_();
  void log_flush_();
  void handle_history_csv_(AsyncWebServerRequest *req);
  void hist_setup_();
  void hist_close_interval_(uint32_t qh_tag, bool expect_full);
  void hist_backpatch_(uint32_t qh_now);
  void hist_service_();
  std::string json_wifi_scan_();
  // MQTT (mqtt.cpp)
  void mqtt_setup_();
  void mqtt_service_();
  void mqtt_start_();
  void mqtt_stop_();
  void mqtt_publish_(MqttRun &r);
  bool mqtt_parse_(const std::string &json, MqttSettings &s, MqttErr &e);
  bool mqtt_compile_(MqttRun &r, MqttErr &e);
  std::string mqtt_json_(const MqttSettings &s, bool for_nvs);
  void handle_mqtt_get_(AsyncWebServerRequest *req);
  void handle_mqtt_post_(AsyncWebServerRequest *req, const std::string &body);
  void json_mqtt_status_(std::string &s);
  static void mqtt_event_(void *arg, const char *base, int32_t id, void *data);

  web_server_base::WebServerBase *base_;
  // The httpd task's stack is ESPHome's 4096 + 256 B and measured 784 B unused on the gPlugK
  // (2026-09-14, /api/status mem.stack_httpd), so the 513 B URL copy is a member, not a local.
  // Safe because one httpd task serves every request in turn: canHandle and handleRequest never
  // run concurrently. For the same reason nothing on that task may write NVS -- config saves and
  // their event-log records are deferred to the loop.
  mutable char url_buf_[AsyncWebServerRequest::URL_BUF_SIZE];
  const uint8_t *spa_{nullptr}; size_t spa_len_{0};
  const uint8_t *presets_{nullptr}; size_t presets_len_{0};
  const uint8_t *manifest_{nullptr}; size_t manifest_len_{0};
  const uint8_t *icon_{nullptr}; size_t icon_len_{0};

  Descriptor desc_;
  std::string hw_json_{"{}"};
  // Pin objects are members and reconfigured in place: a hw or meter POST re-applies them, and a
  // fresh `new` per call leaked one object per pin and wizard run (nothing ever freed the old one).
  esp32::ESP32InternalGPIOPin uart_rx_pin_;
  esp32::ESP32InternalGPIOPin button_pin_;
  esp32::ESP32InternalGPIOPin led_red_pin_, led_green_pin_, led_blue_pin_;
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
  // Header sniffer, fed every HAN byte whatever the profile (also before one exists). Touched by the
  // loop task only; config writers ask for a reset via sniff_reset_pending_ instead of reaching in.
  ::gplug_sniff::ProtocolSniffer sniff_;
  bool sniff_reset_pending_{false};

  // live state, guarded by mutex_ (HTTP runs on the httpd task)
  mutable std::mutex mutex_;
  float values_[MAX_OBIS];
  bool have_[MAX_OBIS]{};
  // Ei/Eo again, in Wh as a double, straight from the decoder. values_[] is float: past ~10 MWh a
  // counter only has ~10 Wh of resolution there, which is invisible on a gauge but is exactly the
  // jitter a 15-min settlement export must not carry. Negative = not seen since the last config.
  double ei_wh_exact_{-1}, eo_wh_exact_{-1};
  char smid_[40]{};
  uint32_t last_frame_ms_{0};
  // Setup diagnosis (diag_()): any byte on the HAN line, and any value that matched a configured OBIS
  // code. last_frame_ms_ alone can't tell "silent line" from "garbage" from "wrong register map".
  uint32_t last_rx_ms_{0};
  uint32_t last_match_ms_{0};
  uint32_t rx_bytes_{0};
  uint32_t meter_applied_ms_{0};
  // The sniffer's verdict as published for /api/live "detect" and diag_() (copied out of sniff_
  // after each read burst). detect_ms_ is the last header hit, so a silent line goes stale.
  ::gplug_sniff::ProtocolSniffer::Protocol detect_proto_{::gplug_sniff::ProtocolSniffer::NONE};
  ::gplug_sniff::ProtocolSniffer::Tri detect_enc_{::gplug_sniff::ProtocolSniffer::UNKNOWN};
  uint32_t detect_hits_{0};
  uint32_t detect_ms_{0};
  bool key_invalid_{false};
  std::array<Sample, RING_LEN> ring_{};
  size_t ring_head_{0}, ring_count_{0};
  uint32_t last_sample_ms_{0};
  FrameLog frames_;             // Datenstrom capture ring: DLMS frames and DSMR telegrams alike
  uint32_t dlms_frame_seq_seen_{0};

  // Persistent quarter-hour history. Guarded by hist_mutex_, never by mutex_: a range=year scan
  // reads the whole partition (~150 ms) and must not block /api/live or the sampling path.
  mutable std::mutex hist_mutex_;
  PartitionFlash hist_flash_;
  HistoryStore hist_{hist_flash_};
  bool hist_ok_{false};
  bool ota_auth_{false};   // /update needs Basic auth (ota_password set); the SPA's firmware card asks for it
  ::gplug_hist::QhAccum acc_;
  uint32_t acc_qh_{0};                  // wall-clock quarter hour being accumulated, 0 = unknown
  bool acc_time_valid_{false};
  uint8_t pending_flags_{::gplug_hist::HF_BOOT_BEFORE};   // carried into the next record written
  uint32_t unk_first_addr_{0};          // first record of this boot written with an unknown time
  uint16_t unk_count_{0};
  double local_ei_wh_{0}, local_eo_wh_{0};   // fallback integrator for meters without Ei/Eo
  // "Zählerwerte des letzten 1/4-h Updates für alle Quellen": every register the meter sends,
  // snapshotted when the interval closed. /api/live exposes it as `last_qh`.
  float qh_values_[MAX_OBIS]{};
  bool qh_have_[MAX_OBIS]{};
  uint32_t qh_snapshot_qh_{0};
  uint8_t hist_buf_[256];               // scan scratch; a member, not an httpd-task stack local

  // Persistent event log. Guarded by its own mutex for the same reason hist_mutex_ exists: the
  // HTTP task renders it while the loop task appends to it.
  mutable std::mutex log_mutex_;
  ::gplug_log::EventLog log_;
  uint32_t log_flush_ms_{0};            // rate limit for folded events; new records flush at once
  uint32_t log_service_ms_{0};          // log_service_() runs at 1 Hz, not every loop iteration
  bool log_backdated_{false};
  bool wifi_was_up_{false};
  std::string last_diag_{"unconfigured"};

  // Heap trend. Its own mutex, like the log: the loop task samples, the httpd task renders.
  std::mutex mem_mutex_;
  ::gplug_mem::Ring<MEM_RING_LEN> mem_ring_;
  ::gplug_mem::LowLatch mem_latch_;
  uint32_t mem_sample_ms_{0};
  bool mem_sampled_{false};             // the first sample is taken at once, not 5 min after boot
  // The loop task's stack high-water mark, read in mem_service_() because only the task itself can
  // cheaply ask for it; the HTTP handler reports the httpd task's own mark directly.
  uint32_t stack_loop_free_{0};

  // Home Assistant entities and what was last sent, so unavailable states and text are published on
  // change only; the numbers themselves go out on every tick.
#ifdef USE_SENSOR
  sensor::Sensor *ha_sensors_[::gplug_ha::HA_COUNT]{};
  sensor::Sensor *frame_age_sensor_{nullptr};
  sensor::Sensor *free_heap_sensor_{nullptr};
  sensor::Sensor *largest_block_sensor_{nullptr};
  bool ha_sent_have_[::gplug_ha::HA_COUNT]{};
  bool ha_sent_once_{false};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *meter_status_text_{nullptr};
  text_sensor::TextSensor *meter_id_text_{nullptr};
#endif

  // Firmware update from the release (GET/POST /api/update*). The loop task owns the entity and
  // copies what the SPA needs into upd_snap_; the httpd task only ever reads that copy, because the
  // entity's std::strings are reassigned on the loop task when a check completes.
  const char *ota_password_{""};
  enum UpdState : uint8_t { UPD_UNCHECKED, UPD_CHECKING, UPD_NONE, UPD_AVAILABLE, UPD_INSTALLING, UPD_ERROR };
  struct UpdSnap {
    UpdState state{UPD_UNCHECKED};
    bool newer{false};
    uint8_t progress{0};          // percent, while installing
    char error[8]{};              // "check" or "install" when state is UPD_ERROR
    char latest[24]{};
    char release_url[112]{};
    uint32_t checked_ms{0};       // millis() of the last completed check, 0 = none since boot
  };
  mutable std::mutex upd_mutex_;
  UpdSnap upd_snap_;
#ifdef USE_UPDATE
  update::UpdateEntity *upd_{nullptr};
  Component *upd_comp_{nullptr};
#endif
  // MQTT. The httpd task validates a POST into a complete MqttRun and hands it over through
  // mqtt_pending_; the loop task owns the client and the running MqttRun, so a slow broker can
  // never hold a lock the web server waits for. The esp-mqtt task only writes the atomics.
  mutable std::mutex mqtt_mutex_;               // mqtt_cfg_, mqtt_pending_*
  std::unique_ptr<MqttSettings> mqtt_cfg_;      // current settings, for GET and the next POST
  std::unique_ptr<MqttRun> mqtt_pending_;
  bool mqtt_pending_set_{false};
  bool mqtt_pending_reconnect_{false};
  std::unique_ptr<MqttRun> mqtt_run_;           // loop task only
  // Written by the loop task only; atomic because the event handler compares against it to ignore a
  // client that is being torn down.
  std::atomic<::esp_mqtt_client *> mqtt_client_{nullptr};
  uint32_t mqtt_retry_ms_{0};                   // loop task only: last failed start, 0 = none
  uint32_t mqtt_last_ms_{0};                    // loop task only
  uint32_t desc_gen_{0};                        // under mutex_; apply_meter_json_ bumps it
  enum MqttState : uint8_t { MQ_OFF, MQ_CONNECTING, MQ_CONNECTED };
  enum MqttConnErr : uint8_t { MQE_NONE, MQE_TCP, MQE_REFUSED, MQE_AUTH };
  std::atomic<uint8_t> mqtt_state_{MQ_OFF};
  std::atomic<uint8_t> mqtt_conn_err_{MQE_NONE};
  std::atomic<const char *> mqtt_tpl_err_{nullptr};   // tpl_unknown / tpl_overflow after a profile change
  std::atomic<uint32_t> mqtt_sent_{0}, mqtt_dropped_{0}, mqtt_skipped_{0};
  std::atomic<uint32_t> mqtt_pub_ms_{0};        // millis() of the last message handed to the client
  std::atomic<uint32_t> mqtt_stack_free_{0};    // esp-mqtt task's stack high-water mark

  bool upd_checking_{false};      // loop task only
  bool upd_installing_{false};    // loop task only
  uint32_t upd_check_ms_{0};      // loop task only: when the running check started
};

}  // namespace gplug_smi
}  // namespace esphome
