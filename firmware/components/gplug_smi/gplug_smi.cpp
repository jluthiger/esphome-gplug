#include "gplug_smi.h"
#include "history_csv.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/version.h"
#include "esphome/core/time.h"
#include "esphome/components/wifi/wifi_component.h"
#include "esphome/components/captive_portal/captive_portal.h"
#include "esphome/components/esp32/gpio.h"

#include <ArduinoJson.h>
#include <esp_app_desc.h>
#include <esp_system.h>
#include <esp_http_server.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <cmath>
#include <cstring>
#include <memory>

namespace esphome {
namespace gplug_smi {

static const char *const TAG = "gplug_smi";
static const char *const NVS_NS = "gplug";

// SNTP has not synced until the clock is past a date we know is in the past. Without this check
// the device reports 1970 and every history record would carry a nonsense timestamp.
static constexpr uint32_t EPOCH_SANE = 1600000000u;   // 2020-09-13

static uint32_t qh_from_epoch(uint32_t ep) {
  return ep > ::gplug_hist::HIST_EPOCH ? (ep - ::gplug_hist::HIST_EPOCH) / HIST_INTERVAL_S : 0;
}

// ---------------------------------------------------------------- lifecycle

void GplugSmi::setup() {
  std::string s;
  if (this->nvs_load_("hw", s)) {
    std::string err;
    if (!this->apply_hw_json_(s, err)) ESP_LOGW(TAG, "stored hw config invalid: %s", err.c_str());
  }
  if (this->nvs_load_("meter", s)) {
    std::string err;
    if (!this->apply_meter_json_(s, err)) ESP_LOGW(TAG, "stored meter config invalid: %s", err.c_str());
  }
  // apply_meter_json_() flags the next record HF_CONFIG_CHANGE because a *replaced* config may map
  // another register to "Ei" or belong to a swapped meter. At boot the config is the same one the
  // last record was written with and the meter's counters are absolute, so the chain is intact: a
  // reboot must not cost the export an interval's energy. Keep only what a boot really means --
  // a gap before, and a first interval that was not observed in full.
  pending_flags_ = ::gplug_hist::HF_BOOT_BEFORE | ::gplug_hist::HF_PARTIAL;
  this->apply_button_pin_();
  this->apply_led_pins_();
  this->hist_setup_();
  this->log_setup_();
  // DSMR telegrams are captured for the Datenstrom view just like DLMS frames are. The parser
  // hands them over before it parses (and rewrites its buffer), CRC-valid or not.
  dsmr_.set_raw_callback([this](const char *data, size_t len, bool crc_ok) {
    std::lock_guard<std::mutex> lock(mutex_);
    frames_.push(millis(), crc_ok, (const uint8_t *) data, len, nullptr, 0);
  });
  // Compiled-in WiFi credentials (e.g. the `wifi:` block the ESPHome Device Builder adds to every
  // adopted config) would make the AP button pointless: it clears the *saved* credentials, the
  // device reboots and reconnects to the compiled-in network within 3 s. Once the button has been
  // used, drop the compiled-in networks on every boot and run WiFi from portal-saved credentials
  // only. WiFi has already started by now (this component sets up AFTER_WIFI), so restart it:
  // disable()+enable() re-runs WiFiComponent::start(), which with no STA networks left goes
  // straight to the fallback AP + captive portal. start() also derives its preference key from
  // has_sta(), so the portal-saved credentials only stay reachable if the compiled-in ones are
  // dropped on *every* boot -- hence a persistent flag, never cleared. Harmless on builds without
  // compiled-in credentials (has_sta() false at this point, nothing to do).
  if (this->nvs_load_("ignore_sta", s) && wifi::global_wifi_component->has_sta()) {
    ESP_LOGW(TAG, "AP button was used: ignoring compiled-in WiFi credentials, using portal-saved ones only");
    wifi::global_wifi_component->clear_sta();
    wifi::global_wifi_component->disable();
    wifi::global_wifi_component->enable();
  }
  wifi::global_wifi_component->set_keep_scan_results(true);
  this->base_->init();
  this->base_->add_handler_without_auth(this);   // registered before captive portal → we serve "/"
  ESP_LOGI(TAG, "ready, protocol=%d baud=%u obis=%u", desc_.protocol, (unsigned) desc_.baud, desc_.n);
}

void GplugSmi::dump_config() {
  ESP_LOGCONFIG(TAG, "gPlug SMI\n  preset: %s\n  protocol: %s\n  baud: %u\n  rx: %d\n  obis entries: %u\n  spa: %u B gz\n  presets: %u B gz",
                desc_.preset.c_str(), desc_.protocol == Descriptor::DSMR ? "dsmr" : desc_.protocol == Descriptor::DLMS ? "dlms" : "none",
                (unsigned) desc_.baud, desc_.rx, desc_.n, (unsigned) spa_len_, (unsigned) presets_len_);
  // The partition offset is derived by the generated partition table, so log it rather than
  // documenting a constant -- it is what an `esptool erase_region` to wipe the history needs.
  const auto &m = hist_.meta();
  ESP_LOGCONFIG(TAG, "  history: %s, partition 0x%06X +%u B, %u sectors x %u slots, %u records, seq %u",
                hist_ok_ ? "ok" : "disabled", (unsigned) hist_flash_.address(), (unsigned) hist_flash_.size(),
                m.sectors, (unsigned) ::gplug_hist::HIST_SLOTS, (unsigned) m.count, (unsigned) m.seq);
}

void GplugSmi::loop() {
  this->poll_button_();
  this->update_led_();
  uint8_t c;
  size_t budget = 512;   // bytes per loop, keep the main loop responsive
  uint32_t got = 0;
  bool sniff_hit = false;
  if (sniff_reset_pending_) { sniff_.reset(); sniff_reset_pending_ = false; }
  while (budget-- && this->available() && this->read_byte(&c)) {
    got++;
    // Every byte goes through the header sniffer, profile or not: before the meter step it is what
    // lets the wizard propose the profile, afterwards it polices a profile that contradicts the
    // line (diag "protocol").
    sniff_hit |= sniff_.feed(c);
    if (desc_.protocol == Descriptor::DSMR) {
      dsmr_.feed((char) c, [this](const ::gplug_dsmr::DsmrValue &v) { this->on_dsmr_value_(v); });
    } else if (desc_.protocol == Descriptor::DLMS) {
      bool full_ok = dlms_.feed(c);
      // frame_seq() bumps on every closed HDLC frame, success or failure -- unlike feed()'s bool
      // return, which only fires on a full successful decode. Capture regardless of outcome so the
      // Datenstrom view can show CRC-fail / wrong-key frames too.
      if (dlms_.frame_seq() != dlms_frame_seq_seen_) {
        dlms_frame_seq_seen_ = dlms_.frame_seq();
        const auto &raw = dlms_.last_frame();
        std::lock_guard<std::mutex> lock(mutex_);
        frames_.push(millis(), dlms_.last_frame_ok(), raw.data(), raw.size(),
                     full_ok ? dlms_.plaintext() : nullptr, full_ok ? dlms_.plaintext_len() : 0);
      }
      if (full_ok) this->on_dlms_apdu_();
      else if (dlms_.key_invalid() && dlms_.encrypted_seen()) { std::lock_guard<std::mutex> lock(mutex_); key_invalid_ = true; last_frame_ms_ = millis(); }
    }
  }
  if (got) {
    std::lock_guard<std::mutex> lock(mutex_);
    rx_bytes_ += got;
    last_rx_ms_ = millis();
    // The ciphering tag arrives after the header hit, so copy the verdict on every burst, not
    // just on hits; only the timestamp is tied to a hit.
    detect_proto_ = sniff_.protocol();
    detect_enc_ = sniff_.encrypted();
    detect_hits_ = sniff_.hits();
    if (sniff_hit) detect_ms_ = millis();
  }
  // 10 s ring sample
  uint32_t now = millis();
  bool sampled = false;
  int16_t p_net = 0;
  bool meter_ok = false;
  if (now - last_sample_ms_ >= RING_PERIOD_MS) {
    last_sample_ms_ = now;
    std::lock_guard<std::mutex> lock(mutex_);
    Sample smp{(int16_t) value_w_("Pi"), (int16_t) value_w_("Po"), 0, 0, 0};
    // per phase: Tasmota presets use P1i/P1o (gPlugD) or Pi1/Po1 (gPlugM)
    const char *in[3][2] = {{"P1i", "Pi1"}, {"P2i", "Pi2"}, {"P3i", "Pi3"}};
    const char *out[3][2] = {{"P1o", "Po1"}, {"P2o", "Po2"}, {"P3o", "Po3"}};
    int16_t *dst[3] = {&smp.p1, &smp.p2, &smp.p3};
    for (int i = 0; i < 3; i++)
      *dst[i] = (int16_t) (value_w_(in[i][0]) + value_w_(in[i][1]) - value_w_(out[i][0]) - value_w_(out[i][1]));
    ring_[ring_head_] = smp;
    ring_head_ = (ring_head_ + 1) % RING_LEN;
    if (ring_count_ < RING_LEN) ring_count_++;
    p_net = (int16_t) (smp.pi - smp.po);
    meter_ok = last_frame_ms_ != 0 && (now - last_frame_ms_) < 30000;
    sampled = true;
  }
  // Feed the quarter-hour accumulator outside the lock: it never touches the decoder state, and
  // flash I/O must never happen while mutex_ is held (it would stall /api/live for an erase).
  if (sampled) {
    acc_.add(p_net, meter_ok);
    if (meter_ok) {
      // Fallback energy integration, for meters that send no Ei/Eo at all. Cheap enough to always
      // run; only used when the counters are absent when the interval closes.
      double wh = (double) p_net * (RING_PERIOD_MS / 1000.0) / 3600.0;
      if (wh >= 0) local_ei_wh_ += wh; else local_eo_wh_ -= wh;
    }
  }
  this->hist_service_();
  this->log_service_();
}

// ---------------------------------------------------------------- decoding

void GplugSmi::on_dsmr_value_(const ::gplug_dsmr::DsmrValue &v) {
  std::lock_guard<std::mutex> lock(mutex_);
  last_frame_ms_ = millis();
  setup_pending_ = false;
  for (uint8_t i = 0; i < desc_.n; i++) {
    if (strcmp(desc_.obis[i].obis, v.obis) != 0) continue;
    last_match_ms_ = millis();
    if (desc_.obis[i].is_string) {
      if (strcmp(desc_.obis[i].name, "SMid") == 0) strlcpy(smid_, v.value, sizeof smid_);
      have_[i] = true;
    } else {
      double raw = strtod(v.value, nullptr);
      values_[i] = (float) (raw / (desc_.obis[i].scale == 0 ? 1.0 : desc_.obis[i].scale));
      this->note_energy_exact_(desc_.obis[i], raw);
      have_[i] = true;
    }
  }
}

// Keeps the Ei/Eo counters at full precision for the history record (see ei_wh_exact_). `raw` is
// the decoder's number before the descriptor's scale; the descriptor says which unit that yields.
void GplugSmi::note_energy_exact_(const ObisEntry &e, double raw) {
  bool is_ei = strcmp(e.name, "Ei") == 0;
  if (!is_ei && strcmp(e.name, "Eo") != 0) return;
  double v = raw / (e.scale == 0 ? 1.0 : e.scale);   // in the descriptor's unit
  if (strcmp(e.unit, "Wh") != 0) v *= 1000.0;          // kWh (the presets' unit) -> Wh
  (is_ei ? ei_wh_exact_ : eo_wh_exact_) = v;
}

// Applies one decoded value to a single, already-identified descriptor entry.
void GplugSmi::apply_dlms_value_(ObisEntry &e, uint8_t i, const ::gplug_dlms::Value &v) {
  last_match_ms_ = millis();
  bool is_smid = strcmp(e.name, "SMid") == 0;
  if (v.is_string) {
    if (is_smid) {
      size_t n = v.str_len < sizeof smid_ - 1 ? v.str_len : sizeof smid_ - 1;
      // octet strings may be raw bytes (Kamstrup ID-as-bytes) or ASCII; keep printable, hex otherwise
      bool printable = true;
      for (size_t k = 0; k < n; k++) if (v.str[k] < 0x20 || v.str[k] > 0x7E) { printable = false; break; }
      if (printable) { memcpy(smid_, v.str, n); smid_[n] = 0; }
      else { size_t o = 0; for (size_t k = 0; k < n && o + 2 < sizeof smid_; k++) o += snprintf(smid_ + o, sizeof smid_ - o, "%02X", v.str[k]); }
    }
    have_[i] = true;
  } else {
    // Some meters (Kamstrup) send SM-ID as a plain number, not an octet string.
    if (is_smid) snprintf(smid_, sizeof smid_, "%.0f", v.num);
    values_[i] = (float) (v.num / (e.scale == 0 ? 1.0 : e.scale));
    this->note_energy_exact_(e, v.num);
    have_[i] = true;
  }
}

void GplugSmi::on_dlms_apdu_() {
  std::lock_guard<std::mutex> lock(mutex_);
  last_frame_ms_ = millis();
  key_invalid_ = false;
  setup_pending_ = false;
  const uint8_t *buf = dlms_.plaintext();
  size_t len = dlms_.plaintext_len();

  // Try the flat "name + N x (obis, value)" structure first (Kamstrup-style push; matches
  // esphome-gplugk's decode_cosem_ algorithm). Single pass: for each pair the APDU actually
  // contains, find which descriptor entry (if any) wants it and apply it.
  bool any = ::gplug_dlms::DlmsDecoder::decode_structure(buf, len, [this](const uint8_t obis6[6], const ::gplug_dlms::Value &v) {
    for (uint8_t i = 0; i < desc_.n; i++) {
      ObisEntry &e = desc_.obis[i];
      if (e.plen == 4 && memcmp(e.pat, obis6 + 2, 4) == 0) apply_dlms_value_(e, i, v);
    }
  });

  // Not that shape (or empty): try it as a gPlugM/L+G "capture list" push -- a descriptor array
  // followed by a separate, untagged value array (see DlmsDecoder::find_capture_list). find()
  // tries that first internally, then falls back to flat substring search for anything else.
  if (!any) {
    for (uint8_t i = 0; i < desc_.n; i++) {
      ObisEntry &e = desc_.obis[i];
      if (e.plen != 4) continue;
      auto v = ::gplug_dlms::DlmsDecoder::find(buf, len, e.pat, e.plen);
      if (v.found) apply_dlms_value_(e, i, v);
    }
  }
}

float GplugSmi::value_(const char *name, float def) const {
  for (uint8_t i = 0; i < desc_.n; i++)
    if (have_[i] && !desc_.obis[i].is_string && strcmp(desc_.obis[i].name, name) == 0) return values_[i];
  return def;
}

float GplugSmi::value_w_(const char *name) const {
  for (uint8_t i = 0; i < desc_.n; i++) {
    if (!have_[i] || desc_.obis[i].is_string || strcmp(desc_.obis[i].name, name) != 0) continue;
    float v = values_[i];
    if (strcmp(desc_.obis[i].unit, "kW") == 0) v *= 1000.0f;
    return v;
  }
  return 0.0f;
}

// ---------------------------------------------------------------- config

bool GplugSmi::nvs_load_(const char *key, std::string &out) {
  nvs_handle_t h;
  if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return false;
  size_t len = 0;
  bool ok = nvs_get_blob(h, key, nullptr, &len) == ESP_OK && len > 0;
  if (ok) { out.resize(len); ok = nvs_get_blob(h, key, &out[0], &len) == ESP_OK; }
  nvs_close(h);
  return ok;
}

bool GplugSmi::nvs_save_(const char *key, const std::string &value) {
  nvs_handle_t h;
  if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return false;
  bool ok = nvs_set_blob(h, key, value.data(), value.size()) == ESP_OK && nvs_commit(h) == ESP_OK;
  nvs_close(h);
  return ok;
}

static bool hex_to_bytes(const char *hex, uint8_t *out, size_t n) {
  if (!hex || strlen(hex) != n * 2) return false;
  for (size_t i = 0; i < n; i++) {
    char b[3] = {hex[2 * i], hex[2 * i + 1], 0};
    char *end;
    out[i] = (uint8_t) strtoul(b, &end, 16);
    if (*end) return false;
  }
  return true;
}

// {"keep_key":true} instead of "key": re-use the GUEK (and auth key) from the stored meter config.
// The SPA never gets the key back, so without this, correcting just the profile after a failed
// setup meant typing the 32 hex digits again. Rewrites body into the plain form, key included, so
// what gets applied and saved is the same self-contained config as always.
bool GplugSmi::merge_stored_keys_(std::string &body, std::string &err) {
  JsonDocument doc;
  if (deserializeJson(doc, body)) { err = "json"; return false; }
  if (!(doc["keep_key"] | false)) return true;
  doc.remove("keep_key");
  std::string old;
  JsonDocument prev;
  if (!this->nvs_load_("meter", old) || deserializeJson(prev, old) || !*(prev["key"] | "")) {
    err = "no stored key"; return false;
  }
  doc["key"] = prev["key"].as<const char *>();
  if (*(prev["auth_key"] | "")) doc["auth_key"] = prev["auth_key"].as<const char *>();
  body.clear();
  serializeJson(doc, body);
  return true;
}

// {"preset":"…","key":"32hex"?,"descriptor":{"protocol":"dsmr|dlms","baud":…,"rx":…,"mode":"o|r|rE1","serial_flags":12?,"buffer":…?,"obis":[{obis,name,unit,scale,precision,type}]}}
bool GplugSmi::apply_meter_json_(const std::string &json, std::string &err) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) { err = "json"; return false; }
  JsonObject d = doc["descriptor"].as<JsonObject>();
  if (d.isNull()) { err = "descriptor missing"; return false; }
  // Descriptor is ~3 KB (MAX_OBIS=48 entries) -- too big for the httpd task's small stack
  // (a real payload panic'ed it with a stack-protection fault). Heap-allocate instead.
  auto nd_ptr = std::unique_ptr<Descriptor>(new Descriptor());
  Descriptor &nd = *nd_ptr;
  const char *proto = d["protocol"] | "";
  if (!strcmp(proto, "dsmr")) nd.protocol = Descriptor::DSMR;
  else if (!strcmp(proto, "dlms")) nd.protocol = Descriptor::DLMS;
  else { err = "protocol"; return false; }
  nd.baud = d["baud"] | 115200;
  nd.rx = d["rx"] | -1;
  nd.buffer = d["buffer"] | 0;
  nd.serial_flags = d["serial_flags"] | 0;
  const char *mode = d["mode"] | "";
  nd.parity_even = strcmp(mode, "rE1") == 0;
  nd.preset = (const char *) (doc["preset"] | "");
  const char *key = doc["key"] | "";
  if (*key) {
    if (!hex_to_bytes(key, nd.key, 16)) { err = "key must be 32 hex chars"; return false; }
    nd.encrypted = true;
  }
  const char *ak = doc["auth_key"] | "";
  if (*ak) {
    if (!hex_to_bytes(ak, nd.auth_key, 16)) { err = "auth_key must be 32 hex chars"; return false; }
    nd.has_auth_key = true;
  }
  JsonArray arr = d["obis"].as<JsonArray>();
  for (JsonObject o : arr) {
    if (nd.n >= MAX_OBIS) { err = "too many obis entries"; return false; }
    ObisEntry &e = nd.obis[nd.n];
    strlcpy(e.obis, o["obis"] | "", sizeof e.obis);
    strlcpy(e.name, o["name"] | "", sizeof e.name);
    strlcpy(e.unit, o["unit"] | "", sizeof e.unit);
    e.scale = o["scale"] | 1.0f;
    e.precision = o["precision"] | 0;
    e.is_string = !strcmp(o["type"] | "number", "string");
    if (!e.obis[0] || !e.name[0]) { err = "obis entry needs obis+name"; return false; }
    e.plen = 0;
    if (nd.protocol == Descriptor::DLMS) {
      e.plen = (uint8_t) ::gplug_dlms::DlmsDecoder::obis_pattern(e.obis, e.pat);
      if (!e.plen) { err = std::string("bad dlms obis: ") + e.obis; return false; }
    }
    nd.n++;
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    desc_ = nd;
    memset(have_, 0, sizeof have_);
    ei_wh_exact_ = eo_wh_exact_ = -1;
    smid_[0] = 0;
    last_frame_ms_ = 0;
    last_rx_ms_ = 0;
    last_match_ms_ = 0;
    meter_applied_ms_ = millis();
    key_invalid_ = false;
    setup_pending_ = false;
    // A new profile may change the line parameters; what was sniffed at the old ones is void.
    sniff_reset_pending_ = true;
    detect_proto_ = ::gplug_sniff::ProtocolSniffer::NONE;
    detect_enc_ = ::gplug_sniff::ProtocolSniffer::UNKNOWN;
    detect_hits_ = 0;
    detect_ms_ = 0;
    dlms_ = ::gplug_dlms::DlmsDecoder();
    if (nd.encrypted) dlms_.set_key(nd.key);
    if (nd.has_auth_key) dlms_.set_auth_key(nd.auth_key);
    dlms_.set_max_frame(nd.buffer > 256 ? nd.buffer : 1280);
  }
  // The counter chain breaks here: a different preset may map a different register to "Ei", and a
  // meter swap resets the counters outright. Flag the next record so the client shows a null delta
  // rather than a spike, and restart the fallback integrator.
  pending_flags_ |= ::gplug_hist::HF_CONFIG_CHANGE | ::gplug_hist::HF_PARTIAL;
  local_ei_wh_ = local_eo_wh_ = 0;
  this->apply_uart_();
  return true;
}

// {"variant":"gplugm","pins":{"rx":7,"red":1,"green":4,"blue":3,"button":9},"baud":2400,"parity":"E","serial_flags":12}
// baud/parity/serial_flags are optional (older SPAs and stored configs don't send them).
bool GplugSmi::apply_hw_json_(const std::string &json, std::string &err) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) { err = "json"; return false; }
  if (doc["variant"].isNull()) { err = "variant missing"; return false; }
  hw_json_ = json;
  int rx = doc["pins"]["rx"] | -1;
  if (rx >= 0) desc_.rx = rx;   // hardware pin wins over preset default
  // The line parameters belong to the variant's physical interface, so they travel with the
  // hardware step: the UART then already runs right for the sniffer before any profile is chosen.
  // Every preset of a variant carries the same values, so a later meter step changes nothing here.
  if (!doc["baud"].isNull()) desc_.baud = doc["baud"];
  if (!doc["parity"].isNull()) desc_.parity_even = strcmp(doc["parity"] | "N", "E") == 0;
  if (!doc["serial_flags"].isNull()) desc_.serial_flags = doc["serial_flags"];
  {
    std::lock_guard<std::mutex> lock(mutex_);
    sniff_reset_pending_ = true;
    detect_proto_ = ::gplug_sniff::ProtocolSniffer::NONE;
    detect_enc_ = ::gplug_sniff::ProtocolSniffer::UNKNOWN;
    detect_hits_ = 0;
    detect_ms_ = 0;
  }
  this->apply_uart_();
  int button = doc["pins"]["button"] | -1;
  if (button != button_pin_num_) { button_pin_num_ = (int8_t) button; this->apply_button_pin_(); }
  int red = doc["pins"]["red"] | -1;
  int green = doc["pins"]["green"] | -1;
  int blue = doc["pins"]["blue"] | -1;
  if (red != led_red_pin_num_ || green != led_green_pin_num_ || blue != led_blue_pin_num_) {
    led_red_pin_num_ = (int8_t) red;
    led_green_pin_num_ = (int8_t) green;
    led_blue_pin_num_ = (int8_t) blue;
    this->apply_led_pins_();
  }
  return true;
}

// AP-reset button, wired to GND (active low, internal pull-up). Held >= 3 s: clears the saved
// WiFi STA credentials and reboots, so the device comes back up in AP-fallback mode (same effect
// as the esptool nvs-erase workaround, without a cable). Only armed once per boot per press so
// holding it longer than 3 s doesn't retrigger.
void GplugSmi::apply_button_pin_() {
  button_gpio_ = nullptr;
  button_down_ = false;
  button_ap_triggered_ = false;
  if (button_pin_num_ < 0) return;
  auto *pin = new esp32::ESP32InternalGPIOPin();   // NOLINT: lives for the rest of the runtime
  pin->set_pin((gpio_num_t) button_pin_num_);
  pin->set_inverted(false);
  pin->set_flags(gpio::Flags(gpio::FLAG_INPUT | gpio::FLAG_PULLUP));
  pin->setup();
  button_gpio_ = pin;
}

void GplugSmi::poll_button_() {
  if (button_gpio_ == nullptr) return;
  bool pressed = !button_gpio_->digital_read();   // active low
  uint32_t now = millis();
  if (pressed && !button_down_) {
    button_down_ = true;
    button_down_ms_ = now;
  } else if (!pressed) {
    button_down_ = false;
    button_ap_triggered_ = false;
  } else if (button_down_ && !button_ap_triggered_ && now - button_down_ms_ >= 3000) {
    button_ap_triggered_ = true;
    // Written (and flushed) before the credentials go, so the log explains the AP the user is
    // about to find themselves in.
    this->log_event_(::gplug_log::EV_BUTTON);
    // wifi::save_wifi_sta("", "") does NOT clear credentials -- it *saves* an empty-SSID STA
    // entry, which WiFiComponent::start() reloads and retries forever on every future boot too
    // (WiFiComponent::pref_ persists across reboots), producing an endless "No matching network
    // found" / "Restarting adapter" loop that starves the AP instead of leaving it stable.
    // Erase ESPHome's own NVS namespace instead: that's where WiFiComponent's preference lives
    // (esp32/preferences.cpp opens "esphome"). NOT nvs_flash_erase() -- that also wipes our
    // "gplug" namespace, i.e. the hw config with the LED pin numbers, so after the reboot the
    // LED can't be driven at all and just keeps whatever state GPIO left it in (red).
    ESP_LOGW(TAG, "AP button held 3s: clearing WiFi credentials, rebooting into AP mode");
    this->defer([this]() {
      // Compiled-in credentials survive the erase below; tell setup() to drop them from now on.
      this->nvs_save_("ignore_sta", "1");
      nvs_handle_t h;
      if (nvs_open("esphome", NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h);
        nvs_commit(h);
        nvs_close(h);
      }
      esp_restart();
    });
  }
}

// RGB status LED. Mode is derived each loop from live state, not stored, so it always reflects
// reality even across a mid-run hw/meter reconfigure: AP-fallback active -> blue blinking; WiFi
// joined but no meter descriptor configured yet, or WiFi just (re)configured via the portal and
// the wizard hasn't committed the meter / no frame has arrived since -> blue steady; meter
// configured and running -> green steady; error (wrong DLMS decrypt key, or no meter data at all
// 60 s after a meter is configured -- "no smart meter connected") -> red steady. AP beats error.
static constexpr uint32_t LED_NO_DATA_TIMEOUT_MS = 60000;
// WiFi/captive-portal state (and therefore the computed mode) can flap for a moment during a real
// transition, e.g. dropping into AP-fallback goes through a few STA-retry/AP-(re)start cycles
// before settling. Only commit a mode change -- and only then touch the GPIOs -- once the newly
// computed mode has been stable for this long, so the LED never visibly flickers between two
// colors while the underlying state is still settling.
static constexpr uint32_t LED_DEBOUNCE_MS = 300;
static GPIOPin *make_led_pin_(int8_t num) {
  if (num < 0) return nullptr;
  auto *pin = new esp32::ESP32InternalGPIOPin();   // NOLINT: lives for the rest of the runtime
  pin->set_pin((gpio_num_t) num);
  pin->set_inverted(false);
  pin->set_flags(gpio::FLAG_OUTPUT);
  pin->setup();
  pin->digital_write(false);
  return pin;
}

void GplugSmi::apply_led_pins_() {
  led_red_gpio_ = make_led_pin_(led_red_pin_num_);
  led_green_gpio_ = make_led_pin_(led_green_pin_num_);
  led_blue_gpio_ = make_led_pin_(led_blue_pin_num_);
  led_blink_on_ = false;
  led_blink_last_ms_ = 0;
  led_pending_mode_ = -1;
  led_pending_since_ms_ = 0;
}

static const char *led_mode_name_(int8_t mode) {
  switch (mode) {
    case 0: return "AP (blue blink)";
    case 1: return "setup (blue steady)";
    case 2: return "running (green steady)";
    default: return "error (red steady)";
  }
}

// Setup verdict for the SPA (/api/live "diag"): why there are no values, so the app can point at
// the wizard step that fixes it. Uses the LED's 60 s grace after a config change or boot, so a
// meter that pushes only every 10-30 s isn't declared broken while the user is still watching.
//   ok        -- a configured OBIS code matched within the window
//   protocol  -- the line's header bytes say DSMR/DLMS, the profile expects the other -> meter step, profile
//   waiting   -- grace period, nothing conclusive yet
//   key       -- frames arrive but the GUEK doesn't decrypt them          -> meter step, key
//   no_match  -- frames decode but no configured OBIS code is in them     -> meter step, profile
//   garbled   -- bytes arrive but never form a valid frame/telegram       -> meter step (baud/protocol), maybe pins
//   silent    -- nothing on the line at all                               -> hardware step, cable, or
//                the utility hasn't enabled the meter's customer port
// Caller holds mutex_.
const char *GplugSmi::diag_(uint32_t now) const {
  if (desc_.protocol == Descriptor::NONE) return "unconfigured";
  if (key_invalid_) return "key";
  auto recent = [now](uint32_t t) { return t != 0 && now - t < LED_NO_DATA_TIMEOUT_MS; };
  if (recent(last_match_ms_)) return "ok";
  // Two header hits of the other protocol are conclusive on their own -- no need to sit out the
  // grace period, and the meter step will propose the right profile.
  using Sniff = ::gplug_sniff::ProtocolSniffer;
  Sniff::Protocol expected = desc_.protocol == Descriptor::DSMR ? Sniff::DSMR : Sniff::DLMS;
  if (recent(detect_ms_) && detect_hits_ >= 2 && detect_proto_ != Sniff::NONE && detect_proto_ != expected) return "protocol";
  bool grace = now - meter_applied_ms_ < LED_NO_DATA_TIMEOUT_MS;
  if (grace) return "waiting";
  if (recent(last_frame_ms_)) return "no_match";
  return recent(last_rx_ms_) ? "garbled" : "silent";
}

void GplugSmi::update_led_() {
  bool ap_mode = captive_portal::global_captive_portal != nullptr && captive_portal::global_captive_portal->is_active();
  uint32_t now = millis();
  if (ap_mode != led_ap_prev_) {
    led_ap_prev_ = ap_mode;
    if (!ap_mode) {
      // Just left AP-fallback: WiFi was (re)configured and the user is now in setup. Whatever
      // error state accumulated before or during AP mode is stale -- key_invalid_ latches, and the
      // no-data grace timer (which started at boot) has usually run out while the user was still
      // on the portal, so without this the LED went red the instant WiFi came up. Stay in setup
      // (blue steady) until the wizard commits the meter or the first frame arrives.
      std::lock_guard<std::mutex> lock(mutex_);
      key_invalid_ = false;
      last_frame_ms_ = 0;
      last_rx_ms_ = 0;
      last_match_ms_ = 0;
      meter_applied_ms_ = now;
      setup_pending_ = true;
    }
  }
  bool has_meter = desc_.protocol != Descriptor::NONE && !setup_pending_;
  uint32_t since_data = last_frame_ms_ == 0 ? now - meter_applied_ms_ : now - last_frame_ms_;
  bool no_data = !ap_mode && has_meter && since_data > LED_NO_DATA_TIMEOUT_MS;
  no_data_ = no_data;
  // Frames arriving without a single configured OBIS code, or in the other protocol altogether
  // (wrong profile), is as broken as no data, and the SPA says so (diag "no_match" / "protocol")
  // -- the LED must not stay green meanwhile.
  const char *d = has_meter ? diag_(now) : "";
  bool wrong_profile = !ap_mode && has_meter && (!strcmp(d, "no_match") || !strcmp(d, "protocol"));
  // AP-fallback is the state that actually needs the user's attention, so it always wins the LED.
  bool error = !ap_mode && (key_invalid_ || no_data || wrong_profile);
  bool running = !ap_mode && has_meter && !error;
  int8_t candidate = ap_mode ? 0 : (error ? 3 : (running ? 2 : 1));

  if (candidate != led_pending_mode_) {
    led_pending_mode_ = candidate;
    led_pending_since_ms_ = now;
  } else if (candidate != led_mode_ && now - led_pending_since_ms_ >= LED_DEBOUNCE_MS) {
    led_mode_ = candidate;
    ESP_LOGI(TAG, "led mode -> %s", led_mode_name_(led_mode_));
  }

  // Always drive GPIOs from the debounced led_mode_, never the raw candidate computed above --
  // that's what keeps a transient flap in ap_mode/error/running from ever reaching the LED.
  if (led_red_gpio_ == nullptr && led_green_gpio_ == nullptr && led_blue_gpio_ == nullptr) return;
  bool show_error = led_mode_ == 3, show_running = led_mode_ == 2, show_ap = led_mode_ == 0;
  if (led_red_gpio_ != nullptr) led_red_gpio_->digital_write(show_error);
  if (show_error) {
    if (led_green_gpio_ != nullptr) led_green_gpio_->digital_write(false);
    if (led_blue_gpio_ != nullptr) led_blue_gpio_->digital_write(false);
    return;
  }
  if (show_running) {
    if (led_green_gpio_ != nullptr) led_green_gpio_->digital_write(true);
    if (led_blue_gpio_ != nullptr) led_blue_gpio_->digital_write(false);
    return;
  }
  if (led_green_gpio_ != nullptr) led_green_gpio_->digital_write(false);
  if (led_blue_gpio_ == nullptr) return;
  if (!show_ap) { led_blue_gpio_->digital_write(true); return; }   // setup mode: steady

  if (now - led_blink_last_ms_ >= 500) {
    led_blink_last_ms_ = now;
    led_blink_on_ = !led_blink_on_;
    led_blue_gpio_->digital_write(led_blink_on_);
  }
}

void GplugSmi::apply_uart_() {
  auto *u = this->parent_;
  if (u == nullptr) return;
  u->set_baud_rate(desc_.baud);
  u->set_parity(desc_.parity_even ? uart::UART_CONFIG_PARITY_EVEN : uart::UART_CONFIG_PARITY_NONE);
  if (desc_.rx >= 0) {
    auto *pin = new esp32::ESP32InternalGPIOPin();   // NOLINT: lives for the rest of the runtime
    pin->set_pin((gpio_num_t) desc_.rx);
    pin->set_inverted(desc_.serial_flags & 0x04);
    gpio::Flags flags = gpio::FLAG_INPUT;
    if (!(desc_.serial_flags & 0x08)) flags = gpio::Flags(flags | gpio::FLAG_PULLUP);
    pin->set_flags(flags);
    u->set_rx_pin(pin);
  }
  u->load_settings(false);
  ESP_LOGI(TAG, "uart: %u baud, rx=%d, parity=%s, invert=%d", (unsigned) desc_.baud, desc_.rx,
           desc_.parity_even ? "E" : "N", (desc_.serial_flags & 0x04) != 0);
}

// ---------------------------------------------------------------- http

static bool starts_with(StringRef s, const char *prefix) {
  size_t n = strlen(prefix);
  return s.size() >= n && memcmp(s.c_str(), prefix, n) == 0;
}

bool GplugSmi::canHandle(AsyncWebServerRequest *request) const {
  // While the WiFi component has fallen back to AP-only mode, defer every GET to ESPHome's own
  // captive_portal: it registers its own handler later (event-driven, from WiFiComponent's loop,
  // not at setup() time) and would never win the race against ours otherwise, since this handler
  // claims all GET unconditionally and registers first, at setup(). Phones show that page inside a
  // constrained OS captive-portal webview (iOS Captive Network Assistant, Android's equivalent) --
  // a small, framework-free HTML form is far more reliably rendered there than this SPA is. Once
  // the device actually joins the configured WiFi (or a user manually opens the device's real IP,
  // where no restricted webview is involved), captive_portal is no longer active and this handler
  // resumes serving the full SPA for hardware/meter setup and live/history viewing.
  if (captive_portal::global_captive_portal != nullptr && captive_portal::global_captive_portal->is_active())
    return false;
  if (request->method() == HTTP_GET) return true;   // SPA for everything that is not /api
  if (request->method() == HTTP_POST) {
    char buf[AsyncWebServerRequest::URL_BUF_SIZE];
    return starts_with(request->url_to(buf), "/api/");
  }
  return false;
}

void GplugSmi::send_gz_(AsyncWebServerRequest *req, const char *ctype, const uint8_t *data, size_t len) {
  auto *res = req->beginResponse(200, ctype, data, len);
  res->addHeader("Content-Encoding", "gzip");
  res->addHeader("Cache-Control", "no-cache");
  req->send(res);
}

void GplugSmi::send_json_(AsyncWebServerRequest *req, int code, const std::string &body) {
  req->send(code, "application/json", body.c_str());
}

// The ESP-IDF shim only pre-reads x-www-form-urlencoded bodies; JSON bodies are still on the socket.
bool GplugSmi::read_body_(AsyncWebServerRequest *req, std::string &out) {
  httpd_req_t *r = *req;
  size_t len = r->content_len;
  if (len == 0 || len > 16384) return false;
  out.resize(len);
  size_t got = 0;
  while (got < len) {
    int n = httpd_req_recv(r, &out[got], len - got);
    if (n <= 0) return false;
    got += n;
  }
  return true;
}

void GplugSmi::handleRequest(AsyncWebServerRequest *req) {
  char buf[AsyncWebServerRequest::URL_BUF_SIZE];
  StringRef url = req->url_to(buf);

  if (req->method() == HTTP_GET) {
    if (url == "/api/status") return send_json_(req, 200, json_status_());
    if (url == "/api/live") return send_json_(req, 200, json_live_());
    if (url == "/api/ring") return send_json_(req, 200, json_ring_());
    if (url == "/api/wifi/scan") return send_json_(req, 200, json_wifi_scan_());
    if (url == "/api/presets") return send_gz_(req, "application/json", presets_, presets_len_);
    if (url == "/api/config/hardware") return send_json_(req, 200, hw_json_);
    if (url == "/api/frames") return send_json_(req, 200, json_frames_());
    // url_to() strips the query string, so the literal compare still matches /api/history?range=...
    if (url == "/api/history") {
      auto *p = req->getParam("range");
      return send_json_(req, 200, json_history_(p ? p->value().c_str() : "day"));
    }
    if (url == "/api/history.csv") return handle_history_csv_(req);
    if (url == "/api/log") return send_json_(req, 200, json_log_());
    if (starts_with(url, "/api/frames/")) return handle_frame_detail_(req, url.c_str());
    if (starts_with(url, "/api/")) return send_json_(req, 404, "{\"error\":\"not found\"}");
    return send_gz_(req, "text/html", spa_, spa_len_);   // SPA fallback (also captive page)
  }

  std::string body;
  if (url == "/api/reboot") {
    send_json_(req, 200, "{\"ok\":true}");
    this->defer([]() { App.safe_reboot(); });
    return;
  }
  if (!read_body_(req, body)) return send_json_(req, 400, "{\"error\":\"body\"}");

  if (url == "/api/config/hardware") {
    std::string err;
    if (!apply_hw_json_(body, err)) return send_json_(req, 400, "{\"error\":\"" + err + "\"}");
    this->log_event_(::gplug_log::EV_CONFIG, 1);
    this->defer([this, body]() { if (!this->nvs_save_("hw", body)) ESP_LOGE(TAG, "nvs_save_(hw) failed -- config applied live but won't survive a reboot"); });
    return send_json_(req, 200, "{\"ok\":true}");
  }
  if (url == "/api/config/meter") {
    std::string err;
    if (!merge_stored_keys_(body, err)) return send_json_(req, 400, "{\"error\":\"" + err + "\"}");
    if (!apply_meter_json_(body, err)) return send_json_(req, 400, "{\"error\":\"" + err + "\"}");
    this->log_event_(::gplug_log::EV_CONFIG, 2);
    this->defer([this, body]() { if (!this->nvs_save_("meter", body)) ESP_LOGE(TAG, "nvs_save_(meter) failed -- config applied live but won't survive a reboot"); });
    return send_json_(req, 200, "{\"ok\":true}");
  }
  if (url == "/api/config/wifi") {
    JsonDocument doc;
    if (deserializeJson(doc, body) || doc["ssid"].isNull()) return send_json_(req, 400, "{\"error\":\"ssid\"}");
    std::string ssid = doc["ssid"].as<const char *>(), psk = doc["psk"] | "";
    this->log_event_(::gplug_log::EV_CONFIG, 3);
    this->defer([ssid, psk]() { wifi::global_wifi_component->save_wifi_sta(ssid.c_str(), psk.c_str()); });
    return send_json_(req, 200, "{\"ok\":true}");
  }
  send_json_(req, 404, "{\"error\":\"not found\"}");
}

static void json_escape(std::string &out, const char *s) {
  for (; *s; s++) {
    if (*s == '"' || *s == '\\') out += '\\';
    if ((uint8_t) *s < 0x20) continue;
    out += *s;
  }
}

// A DSMR telegram verbatim, with control bytes other than CR/LF/TAB replaced so a stray byte can
// neither break the response nor hide.
static void bytes_to_text(std::string &out, const uint8_t *data, size_t n) {
  out.reserve(out.size() + n + 1);
  for (size_t i = 0; i < n; i++) {
    uint8_t c = data[i];
    out += (c >= 0x20 || c == '\r' || c == '\n' || c == '\t') && c != 0x7F ? (char) c : '.';
  }
  if (n && data[n - 1] != '\n') out += '\n';
}

// Uppercase hex, 16 bytes/line -- readable both inline (<pre>) and as a downloaded .txt.
static void bytes_to_hex(std::string &out, const uint8_t *data, size_t n, size_t wrap = 16) {
  static const char *H = "0123456789ABCDEF";
  out.reserve(out.size() + n * 3 + n / wrap + 1);
  for (size_t i = 0; i < n; i++) {
    if (i && (i % wrap) == 0) out += '\n';
    else if (i) out += ' ';
    out += H[data[i] >> 4];
    out += H[data[i] & 0xF];
  }
  out += '\n';
}

std::string GplugSmi::json_status_() {
  auto *w = wifi::global_wifi_component;
  bool conn = w->is_connected();
  std::string ip;
  if (conn) for (auto &a : w->wifi_sta_ip_addresses()) if (a.is_set()) { char ipb[network::IP_ADDRESS_BUFFER_SIZE]; a.str_to(ipb); ip = ipb; break; }
  char ssid_buf[wifi::SSID_BUFFER_SIZE] = {};
  std::string s = "{\"version\":\"" ESPHOME_VERSION "\",\"hostname\":\"";
  json_escape(s, App.get_name().c_str());
  s += "\",\"uptime\":" + std::to_string(millis() / 1000);
  // build: lets the SPA's firmware card tell the new image from the old one after an OTA reboot
  // (version is the ESPHome release and usually doesn't change between our builds).
  s += ",\"build\":" + std::to_string((uint32_t) App.get_build_time());
  // Which image is actually running, as the first 16 hex digits of its ELF SHA-256. `build` cannot
  // answer that: it is ESPHome's ESPHOME_BUILD_TIME, which ESPHome caches per config hash, so an
  // update that changes only embedded assets (the SPA, the presets, the captive page) ships the
  // previous timestamp and looks like "nothing was installed" to the firmware card. The ELF hash
  // moves with any byte of the image, and the same 32 bytes sit at offset 0xB0 of an OTA file
  // (esp_app_desc_t.app_elf_sha256), so the browser can compare the file it just uploaded against
  // what came back up -- a positive identification instead of "the timestamp differs".
  // Straight from the descriptor, not esp_app_get_elf_sha256(): that helper truncates to
  // CONFIG_APP_RETRIEVE_LEN_ELF_SHA, which is 9 characters by default and would not line up with
  // the 16 the browser reads out of the file.
  s += ",\"app\":\"";
  if (const esp_app_desc_t *d = esp_app_get_description()) {
    static const char H[] = "0123456789abcdef";
    for (int i = 0; i < 8; i++) {
      s += H[d->app_elf_sha256[i] >> 4];
      s += H[d->app_elf_sha256[i] & 0xF];
    }
  }
  s += "\"";
  s += ",\"ota_auth\":" + std::string(ota_auth_ ? "true" : "false");
  s += ",\"heap\":" + std::to_string(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  s += ",\"wifi\":{\"connected\":" + std::string(conn ? "true" : "false");
  if (conn) {
    s += ",\"ssid\":\""; json_escape(s, w->wifi_ssid_to(ssid_buf)); s += "\"";
    s += ",\"ip\":\"" + ip + "\",\"rssi\":" + std::to_string(w->wifi_rssi());
  }
  s += "},\"hardware\":" + hw_json_;
  s += ",\"meter\":{\"preset\":\""; json_escape(s, desc_.preset.c_str()); s += "\"";
  s += ",\"protocol\":" + std::to_string(desc_.protocol) + ",\"obis\":" + std::to_string(desc_.n);
  s += ",\"encrypted\":" + std::string(desc_.encrypted ? "true" : "false");
  s += ",\"dsmr_telegrams\":" + std::to_string(dsmr_.telegrams) + ",\"dsmr_crc_errors\":" + std::to_string(dsmr_.crc_errors);
  s += ",\"dlms_frames\":" + std::to_string(dlms_.stats.frames) + ",\"dlms_apdus\":" + std::to_string(dlms_.stats.apdus);
  s += ",\"dlms_fcs_errors\":" + std::to_string(dlms_.stats.fcs_errors) + ",\"dlms_auth_failed\":" + std::to_string(dlms_.stats.auth_failed) + "}";
  uint32_t ep = (uint32_t) ::time(nullptr);
  bool epoch_valid = ep > EPOCH_SANE;
  s += ",\"time\":{\"valid\":" + std::string(epoch_valid ? "true" : "false");
  s += ",\"epoch\":" + std::to_string(epoch_valid ? ep : 0) + "}";
  {
    std::lock_guard<std::mutex> lock(hist_mutex_);
    const auto &m = hist_.meta();
    s += ",\"history\":{\"ok\":" + std::string(hist_ok_ ? "true" : "false");
    s += ",\"addr\":" + std::to_string(hist_flash_.address()) + ",\"size\":" + std::to_string(hist_flash_.size());
    s += ",\"sectors\":" + std::to_string(m.sectors) + ",\"slots\":" + std::to_string((unsigned) ::gplug_hist::HIST_SLOTS);
    s += ",\"interval\":" + std::to_string(HIST_INTERVAL_S);
    s += ",\"count\":" + std::to_string(m.count) + ",\"seq\":" + std::to_string(m.seq);
    s += ",\"oldest_qh\":" + std::to_string(m.oldest_qh) + ",\"newest_qh\":" + std::to_string(m.newest_qh);
    s += ",\"erases\":" + std::to_string(m.erases) + ",\"writes\":" + std::to_string(m.writes);
    s += ",\"crc_errors\":" + std::to_string(m.crc_errors) + "}";
  }
  s += "}";
  return s;
}

static void append_num(std::string &s, float v, int prec) {
  char b[32];
  if (std::isnan(v) || std::isinf(v)) { s += "null"; return; }
  snprintf(b, sizeof b, "%.*f", prec, (double) v);
  s += b;
}

std::string GplugSmi::json_live_() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string s = "{";
  if (last_frame_ms_ == 0) { s += "\"age\":null"; }
  else { s += "\"age\":" + std::to_string((millis() - last_frame_ms_) / 1000); }
  s += ",\"key_invalid\":" + std::string(key_invalid_ ? "true" : "false");
  s += ",\"no_data\":" + std::string(no_data_ ? "true" : "false");
  s += ",\"diag\":\"" + std::string(diag_(millis())) + "\"";
  s += ",\"rx_bytes\":" + std::to_string(rx_bytes_);
  s += ",\"rx_age\":" + (last_rx_ms_ ? std::to_string((millis() - last_rx_ms_) / 1000) : "null");
  // What the line speaks according to its header bytes (protocol_sniff.h), available before any
  // profile is configured: the wizard's meter step proposes the profile from it.
  using Sniff = ::gplug_sniff::ProtocolSniffer;
  s += ",\"detect\":{\"protocol\":";
  s += detect_proto_ == Sniff::DSMR ? "\"dsmr\"" : detect_proto_ == Sniff::DLMS ? "\"dlms\"" : "null";
  s += ",\"encrypted\":";
  s += detect_enc_ == Sniff::YES ? "true" : detect_enc_ == Sniff::NO ? "false" : "null";
  s += ",\"hits\":" + std::to_string(detect_hits_);
  s += ",\"age\":" + (detect_ms_ ? std::to_string((millis() - detect_ms_) / 1000) : "null") + "}";
  if (smid_[0]) { s += ",\"smid\":\""; json_escape(s, smid_); s += "\""; }
  float pi = value_w_("Pi"), po = value_w_("Po");
  s += ",\"p\":"; append_num(s, (pi - po) / 1000.0f, 3);
  s += ",\"pi\":"; append_num(s, pi, 0);
  s += ",\"po\":"; append_num(s, po, 0);
  s += ",\"ei\":"; append_num(s, value_("Ei", NAN), 3);
  s += ",\"eo\":"; append_num(s, value_("Eo", NAN), 3);
  s += ",\"values\":{";
  bool first = true;
  for (uint8_t i = 0; i < desc_.n; i++) {
    if (!have_[i] || desc_.obis[i].is_string) continue;
    if (!first) s += ",";
    first = false;
    s += "\""; json_escape(s, desc_.obis[i].name); s += "\":";
    append_num(s, values_[i], desc_.obis[i].precision);
  }
  s += "}";
  // Every register the meter sent, as of the moment the last quarter-hour record was written -- the
  // stored record itself only carries Ei/Eo plus the power aggregate.
  s += ",\"last_qh\":{\"qh\":" + std::to_string(qh_snapshot_qh_) + ",\"values\":{";
  first = true;
  for (uint8_t i = 0; i < desc_.n; i++) {
    if (!qh_have_[i] || desc_.obis[i].is_string) continue;
    if (!first) s += ",";
    first = false;
    s += "\""; json_escape(s, desc_.obis[i].name); s += "\":";
    append_num(s, qh_values_[i], desc_.obis[i].precision);
  }
  s += "}}}";
  return s;
}

std::string GplugSmi::json_ring_() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string s = "{\"period\":10,\"samples\":[";
  s.reserve(ring_count_ * 24 + 32);
  for (size_t k = 0; k < ring_count_; k++) {
    const Sample &m = ring_[(ring_head_ + RING_LEN - ring_count_ + k) % RING_LEN];
    if (k) s += ",";
    s += "[" + std::to_string(m.pi) + "," + std::to_string(m.po) + "," + std::to_string(m.p1) + "," +
         std::to_string(m.p2) + "," + std::to_string(m.p3) + "]";
  }
  s += "]}";
  return s;
}

// Metadata only, newest-first (i=0 = most recent) -- no frame bytes here, those are fetched
// per-frame via /api/frames/<i>/raw|plain (handle_frame_detail_) so the list stays cheap to poll.
std::string GplugSmi::json_frames_() {
  std::lock_guard<std::mutex> lock(mutex_);
  bool dsmr = desc_.protocol == Descriptor::DSMR;
  const char *proto = desc_.protocol == Descriptor::DLMS ? "dlms" : dsmr ? "dsmr" : "none";
  std::string s = "{\"protocol\":\""; s += proto; s += "\"";
  // What the readable ("plain") view of a frame is: a DSMR telegram is ASCII to begin with, a DLMS
  // frame only becomes readable after AES-GCM decryption and is served as hex either way.
  s += ",\"encoding\":\""; s += dsmr ? "text" : "hex"; s += "\"";
  s += ",\"cap\":" + std::to_string(FRAME_LOG_RAW_CAP) + ",\"len\":" + std::to_string(FRAME_LOG_LEN);
  s += ",\"count\":" + std::to_string(frames_.count()) + ",\"frames\":[";
  uint32_t now = millis();
  for (size_t i = 0; i < frames_.count(); i++) {
    const auto *e = frames_.at(i);
    if (i) s += ",";
    s += "{\"i\":" + std::to_string(i) + ",\"age\":" + std::to_string((now - e->ts_ms) / 1000);
    s += ",\"ok\":" + std::string(e->ok ? "true" : "false");
    s += ",\"raw_len\":" + std::to_string(e->raw_len) + ",\"raw_trunc\":" + std::string(e->raw_trunc ? "true" : "false");
    s += ",\"plain_len\":" + std::to_string(e->plain_len) + ",\"plain_trunc\":" + std::string(e->plain_trunc ? "true" : "false") + "}";
  }
  s += "]}";
  return s;
}

// /api/frames/<i>/raw|plain -- text/plain hex dump of one captured frame. One endpoint serves
// preview, clipboard-copy, and download alike; the SPA decides client-side which UI action a
// fetched body drives, and can concatenate several fetches for a multi-frame export. Never touches
// key material -- frames_ only ever holds ciphertext/plaintext byte copies (see frame_log.h).
void GplugSmi::handle_frame_detail_(AsyncWebServerRequest *req, const char *url) {
  const char *p = url + strlen("/api/frames/");
  char *end = nullptr;
  long idx = strtol(p, &end, 10);
  if (end == p || idx < 0 || *end != '/') return send_json_(req, 404, "{\"error\":\"not found\"}");
  const char *kind = end + 1;
  bool is_raw = !strcmp(kind, "raw");
  if (!is_raw && strcmp(kind, "plain") != 0) return send_json_(req, 404, "{\"error\":\"not found\"}");

  std::string body;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto *e = frames_.at((size_t) idx);
    if (!e) return send_json_(req, 404, "{\"error\":\"not found\"}");
    if (desc_.protocol == Descriptor::DSMR) {
      // One buffer, two views: the telegram's own ASCII, or a hex dump of the same bytes for
      // looking at framing/stray bytes. Nothing is stored twice.
      if (!e->raw_len) return send_json_(req, 404, "{\"error\":\"empty\"}");
      if (is_raw) bytes_to_hex(body, e->raw.data(), e->raw_len);
      else bytes_to_text(body, e->raw.data(), e->raw_len);
    } else {
      size_t n = is_raw ? e->raw_len : e->plain_len;
      if (!n) return send_json_(req, 404, "{\"error\":\"empty\"}");
      bytes_to_hex(body, is_raw ? e->raw.data() : e->plain.data(), n);
    }
  }
  auto *res = req->beginResponse(200, "text/plain; charset=utf-8", body.c_str());
  res->addHeader("Cache-Control", "no-cache");
  req->send(res);
}

// ---------------------------------------------------------------- event log (NVS)

// esp_reset_reason_t -> our own stable numbering (event_log.h): these values go to flash, IDF's
// do not have to stay put.
static uint8_t reset_reason_code() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return ::gplug_log::RR_POWERON;
    case ESP_RST_EXT: return ::gplug_log::RR_EXT;
    case ESP_RST_SW: return ::gplug_log::RR_SW;
    case ESP_RST_PANIC: return ::gplug_log::RR_PANIC;
    case ESP_RST_INT_WDT: return ::gplug_log::RR_INT_WDT;
    case ESP_RST_TASK_WDT: return ::gplug_log::RR_TASK_WDT;
    case ESP_RST_WDT: return ::gplug_log::RR_WDT;
    case ESP_RST_DEEPSLEEP: return ::gplug_log::RR_DEEPSLEEP;
    case ESP_RST_BROWNOUT: return ::gplug_log::RR_BROWNOUT;
    case ESP_RST_SDIO: return ::gplug_log::RR_SDIO;
    case ESP_RST_USB: return ::gplug_log::RR_USB;
    case ESP_RST_JTAG: return ::gplug_log::RR_JTAG;
    default: return ::gplug_log::RR_UNKNOWN;
  }
}

static uint8_t app_id_hex(std::string &out) {
  out.clear();
  const esp_app_desc_t *d = esp_app_get_description();
  if (!d) return 0;
  static const char H[] = "0123456789abcdef";
  for (int i = 0; i < 8; i++) { out += H[d->app_elf_sha256[i] >> 4]; out += H[d->app_elf_sha256[i] & 0xF]; }
  return 1;
}

void GplugSmi::log_setup_() {
  std::string blob;
  if (this->nvs_load_("log", blob)) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    if (!log_.load(blob.data(), blob.size())) ESP_LOGW(TAG, "event log: stored blob rejected, starting fresh");
  }
  // Why the last run ended. This is the whole point of the log: a panic, a watchdog or a brownout
  // is invisible once the device is back up, and the serial console nobody was attached to is the
  // only place it would otherwise have appeared.
  uint32_t heap_kb = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) / 1024;
  this->log_event_(::gplug_log::EV_BOOT, reset_reason_code(), (uint8_t) (heap_kb > 255 ? 255 : heap_kb));

  // An update is a reboot into a different image, which is exactly what the identity comparison
  // in /api/status sees -- so the log can record "updated" without hooking ESPHome's OTA at all.
  std::string id, seen;
  if (app_id_hex(id)) {
    if (this->nvs_load_("app", seen) && seen != id) this->log_event_(::gplug_log::EV_OTA);
    if (seen != id) this->nvs_save_("app", id);
  }
  this->log_flush_();
}

void GplugSmi::log_event_(uint8_t code, uint8_t detail, uint8_t value) {
  uint32_t ep = (uint32_t) ::time(nullptr);
  uint32_t up = millis() / 1000;
  bool fresh;
  {
    std::lock_guard<std::mutex> lock(log_mutex_);
    fresh = log_.append(code, detail, value, ep > EPOCH_SANE ? ep : 0, up);
  }
  // A new record is worth a flash write immediately -- the next event may be a crash. A folded
  // repeat (a flapping Wi-Fi) is not, so those are left to log_service_()'s rate limit.
  if (fresh) this->log_flush_();
}

void GplugSmi::log_flush_() {
  std::string blob;
  {
    std::lock_guard<std::mutex> lock(log_mutex_);
    if (!log_.dirty()) return;
    blob.assign((const char *) log_.data(), log_.size());
    log_.mark_clean();
  }
  log_flush_ms_ = millis();
  if (!this->nvs_save_("log", blob)) ESP_LOGW(TAG, "event log: NVS write failed");
}

// Watches the two states worth a record -- the Wi-Fi link and the meter verdict -- plus the
// deferred work: back-dating this boot's entries once the clock syncs, and flushing folded
// repeats. Called from loop(); everything here is cheap unless something actually changed.
void GplugSmi::log_service_() {
  // Once a second is plenty for state that changes on a human timescale, and it keeps the loop
  // off mutex_ (which the HTTP task holds while rendering /api/live) on every single iteration.
  uint32_t now = millis();
  if (now - log_service_ms_ < 1000) return;
  log_service_ms_ = now;

  bool up = wifi::global_wifi_component->is_connected();
  if (up != wifi_was_up_) {
    wifi_was_up_ = up;
    int rssi = up ? wifi::global_wifi_component->wifi_rssi() : 0;
    if (rssi < -255) rssi = -255;
    this->log_event_(up ? ::gplug_log::EV_WIFI_UP : ::gplug_log::EV_WIFI_LOST, 0,
                     (uint8_t) (up ? -rssi : 0));
  }

  // The meter going quiet (and coming back) is the single most common support question, and diag_
  // already distinguishes why. "waiting" is the grace period, not a verdict, so it is not logged.
  std::string d;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    d = this->diag_(millis());
  }
  if (d != "waiting" && d != last_diag_) {
    bool was_ok = last_diag_ == "ok";
    last_diag_ = d;
    if (d == "ok") { if (!was_ok) this->log_event_(::gplug_log::EV_METER_OK); }
    else if (d != "unconfigured") {
      uint8_t why = d == "key" ? 1 : d == "no_match" ? 2 : d == "protocol" ? 3 : d == "garbled" ? 4 : 5;
      this->log_event_(::gplug_log::EV_METER_LOST, why);
    }
  }

  uint32_t ep = (uint32_t) ::time(nullptr);
  if (!log_backdated_ && ep > EPOCH_SANE) {
    log_backdated_ = true;
    {
      std::lock_guard<std::mutex> lock(log_mutex_);
      log_.backdate(ep, millis() / 1000);
    }
    this->log_flush_();
  }
  if (millis() - log_flush_ms_ > 60000) this->log_flush_();
}

std::string GplugSmi::json_log_() {
  std::lock_guard<std::mutex> lock(log_mutex_);
  uint32_t ep = (uint32_t) ::time(nullptr);
  std::string s = "{\"now\":" + std::to_string(ep > EPOCH_SANE ? ep : 0);
  s += ",\"uptime\":" + std::to_string(millis() / 1000);
  s += ",\"cap\":" + std::to_string((unsigned) ::gplug_log::LOG_CAP);
  s += ",\"events\":[";
  for (uint8_t i = 0; i < log_.count(); i++) {
    const auto &e = log_.at(i);
    if (i) s += ",";
    s += "{\"t\":" + std::to_string(e.epoch) + ",\"up\":" + std::to_string(e.uptime_s);
    s += ",\"code\":" + std::to_string((unsigned) e.code) + ",\"detail\":" + std::to_string((unsigned) e.detail);
    s += ",\"value\":" + std::to_string((unsigned) e.value) + ",\"repeat\":" + std::to_string((unsigned) e.repeat) + "}";
  }
  s += "]}";
  return s;
}

// ---------------------------------------------------------------- history (15 min, on flash)

// Exact Wh (see ei_wh_exact_) to the record's uint32; negative = counter not seen. The cap keeps
// the value clear of the WH_ABSENT sentinel and of any sane meter (4 GWh).
static uint32_t wh_to_record(double wh) {
  if (std::isnan(wh) || std::isinf(wh) || wh < 0 || wh > 4000000000.0) return ::gplug_hist::WH_ABSENT;
  return (uint32_t) llround(wh);
}

void GplugSmi::hist_setup_() {
  if (!hist_flash_.begin()) {
    ESP_LOGW(TAG, "history: `data` partition not found, history disabled");
    this->log_event_(::gplug_log::EV_STORAGE, 2);
    return;
  }
  std::lock_guard<std::mutex> lock(hist_mutex_);
  hist_ok_ = hist_.begin();
  if (!hist_ok_) {
    ESP_LOGW(TAG, "history: store init failed, history disabled");
    this->log_event_(::gplug_log::EV_STORAGE, 2);
    return;
  }
  // Resume the fallback integrator where the last record left it, so a meter without Ei/Eo still
  // produces a continuous energy series across a reboot.
  if (const auto *l = hist_.last()) {
    if (l->flags & ::gplug_hist::HF_LOCAL_ENERGY) {
      if (l->ei_wh != ::gplug_hist::WH_ABSENT) local_ei_wh_ = l->ei_wh;
      if (l->eo_wh != ::gplug_hist::WH_ABSENT) local_eo_wh_ = l->eo_wh;
    }
  }
  const auto &m = hist_.meta();
  ESP_LOGI(TAG, "history: %u records, %u sectors, seq %u, qh %u..%u", (unsigned) m.count, m.sectors,
           (unsigned) m.seq, (unsigned) m.oldest_qh, (unsigned) m.newest_qh);
}

void GplugSmi::hist_close_interval_(uint32_t qh_tag, bool expect_full) {
  // try_lock, never lock: a range=year request holds hist_mutex_ for ~150 ms and the main loop must
  // not stall behind it. Failing here just means the interval closes a tick later.
  std::unique_lock<std::mutex> lk(hist_mutex_, std::try_to_lock);
  if (!lk.owns_lock()) return;

  uint32_t ei, eo;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    ei = wh_to_record(ei_wh_exact_);
    eo = wh_to_record(eo_wh_exact_);
    memcpy(qh_values_, values_, sizeof qh_values_);
    memcpy(qh_have_, have_, sizeof qh_have_);
    qh_snapshot_qh_ = acc_qh_;
  }
  uint8_t flags = pending_flags_;
  if (ei == ::gplug_hist::WH_ABSENT && eo == ::gplug_hist::WH_ABSENT) {
    ei = (uint32_t) local_ei_wh_;
    eo = (uint32_t) local_eo_wh_;
    flags |= ::gplug_hist::HF_LOCAL_ENERGY;
  }
  acc_.flags |= flags;
  ::gplug_hist::HistRecord r = acc_.close(ei, eo, qh_tag, expect_full);
  if (hist_.append(r)) {
    if (qh_tag == ::gplug_hist::QH_UNKNOWN) {
      if (!unk_count_) unk_first_addr_ = hist_.last_addr();
      unk_count_++;
    }
  } else {
    ESP_LOGW(TAG, "history: append failed");
    this->log_event_(::gplug_log::EV_STORAGE, 1);
  }
  pending_flags_ = 0;
  acc_.reset(0);
}

// Give the records written before the clock synced their (approximate) timestamps. Only possible
// because qh_tag is a separate word excluded from the record's crc and still erased -- programming
// it is a legal 1 -> 0 write, no erase, no rewrite. Only this boot's records can be dated: across a
// power cycle with no clock the number of missed intervals is unknowable.
void GplugSmi::hist_backpatch_(uint32_t qh_now) {
  std::unique_lock<std::mutex> lk(hist_mutex_, std::try_to_lock);
  if (!lk.owns_lock()) return;   // retried on the next service tick
  uint32_t first = qh_now > unk_count_ ? qh_now - unk_count_ : 0;
  uint16_t n = hist_.patch_qh_seq(unk_first_addr_, unk_count_, first, true);
  ESP_LOGI(TAG, "history: back-dated %u of %u records after clock sync", n, unk_count_);
  unk_count_ = 0;
}

void GplugSmi::hist_service_() {
  if (!hist_ok_) return;
  uint32_t ep = (uint32_t) ::time(nullptr);
  bool valid = ep > EPOCH_SANE;

  if (valid && !acc_time_valid_) {
    // Clock just became usable. Close whatever is in flight (it was measured on the millis()
    // cadence, so it is not aligned to a wall-clock quarter hour) and start aligned from here.
    if (acc_.ticks) this->hist_close_interval_(::gplug_hist::QH_UNKNOWN, false);
    acc_time_valid_ = true;
    acc_qh_ = qh_from_epoch(ep);
  } else if (valid) {
    uint32_t qh = qh_from_epoch(ep);
    if (qh > acc_qh_) {
      // A jump of more than one leaves a hole in the qh sequence -- that hole *is* the gap marker;
      // no separate marker record, which would break the one-record-per-interval density.
      this->hist_close_interval_(::gplug_hist::qh_tag_make(acc_qh_, false), qh == acc_qh_ + 1);
      acc_qh_ = qh;
    } else if (qh < acc_qh_) {
      acc_qh_ = qh;   // clock stepped backwards; never write an out-of-order record
      pending_flags_ |= ::gplug_hist::HF_PARTIAL;
    }
  } else if (acc_.ticks >= HIST_TICKS_PER_INTERVAL) {
    this->hist_close_interval_(::gplug_hist::QH_UNKNOWN, true);
  }
  if (valid && unk_count_) this->hist_backpatch_(acc_qh_);

  // Deferred sector recycle. esp_partition_erase_range runs with the flash cache disabled and the
  // UART ISR is not in IRAM (CONFIG_UART_ISR_IN_IRAM is off), so during the 30-50 ms erase nothing
  // drains the 128 B FIFO -- at 115200 baud that overflows after ~11 ms and clips a meter frame.
  // Wait for the quiet stretch after a frame completed (~900 ms at a 1 s meter cadence), or just go
  // ahead if the meter is silent anyway and there is nothing to lose.
  if (hist_.pending_erase()) {
    uint32_t last_frame = last_frame_ms_;   // benign unlocked read: this is only a timing heuristic
    uint32_t since = millis() - last_frame;
    bool silent = !last_frame || since > 5000;
    if (!this->available() && (silent || (since > 150 && since < 600))) {
      std::unique_lock<std::mutex> lk(hist_mutex_, std::try_to_lock);
      if (lk.owns_lock()) hist_.service_erase();
    }
  }
}

// Downsampled history. One streaming pass, no array of records in RAM, response bounded to a few
// hundred points regardless of range.
std::string GplugSmi::json_history_(const char *range) {
  uint32_t bucket = 1, span = 96;   // day: native 15 min resolution, 24 h
  if (!strcmp(range, "week")) { bucket = 4; span = 672; }
  else if (!strcmp(range, "month")) { bucket = 24; span = 2880; }
  else if (!strcmp(range, "year")) { bucket = 96; span = 35040; }
  else range = "day";

  std::lock_guard<std::mutex> lock(hist_mutex_);
  uint32_t ep = (uint32_t) ::time(nullptr);
  bool epoch_valid = ep > EPOCH_SANE;
  const auto &m = hist_.meta();
  uint32_t now_qh = epoch_valid ? qh_from_epoch(ep) : m.newest_qh;

  std::string s = "{\"period\":" + std::to_string(HIST_INTERVAL_S);
  s += ",\"range\":\""; s += range; s += "\"";
  s += ",\"bucket\":" + std::to_string(bucket);
  s += ",\"qh_epoch\":" + std::to_string(::gplug_hist::HIST_EPOCH);
  s += ",\"epoch_valid\":" + std::string(epoch_valid ? "true" : "false");
  s += ",\"epoch\":" + std::to_string(epoch_valid ? ep : 0);
  s += ",\"now_qh\":" + std::to_string(now_qh);
  s += ",\"oldest_qh\":" + std::to_string(m.oldest_qh) + ",\"newest_qh\":" + std::to_string(m.newest_qh);
  s += ",\"count\":" + std::to_string(m.count);
  s += ",\"ok\":" + std::string(hist_ok_ ? "true" : "false");
  s += ",\"pts\":[";
  if (!hist_ok_) { s += "]}"; return s; }

  // from_qh 0 means "no sector skipping"; when the clock has never synced there is nothing to skip
  // by, so records are windowed by position instead.
  uint32_t from_qh = (now_qh > span) ? now_qh - span : (now_qh ? 1 : 0);
  uint32_t idx_from = (!now_qh && m.count > span) ? m.count - span : 0;
  const uint32_t max_delta_wh = bucket * 7500;   // >30 kW average over a quarter hour: not real

  s.reserve(4096);
  ::gplug_hist::HistBucket b;
  uint32_t idx = 0, prev_ei = ::gplug_hist::WH_ABSENT, prev_eo = ::gplug_hist::WH_ABSENT;
  bool first_pt = true;

  auto flush = [&]() {
    if (!b.started || !b.n) return;
    if (!first_pt) s += ",";
    first_pt = false;
    s += "[";
    if (b.has_key) s += std::to_string(b.qh); else s += "null";
    uint32_t d;
    bool broken = (b.flags & ::gplug_hist::HF_CONFIG_CHANGE) != 0;
    if (!broken && ::gplug_hist::HistBucket::delta(prev_ei, b.last_ei, max_delta_wh, &d))
      s += "," + std::to_string(d);
    else s += ",null";
    if (!broken && ::gplug_hist::HistBucket::delta(prev_eo, b.last_eo, max_delta_wh, &d))
      s += "," + std::to_string(d);
    else s += ",null";
    s += "," + std::to_string(b.p_min) + "," + std::to_string(b.p_max) + "," + std::to_string(b.avg());
    s += "," + std::to_string(b.flags) + "]";
    if (b.last_ei != ::gplug_hist::WH_ABSENT) prev_ei = b.last_ei;
    if (b.last_eo != ::gplug_hist::WH_ABSENT) prev_eo = b.last_eo;
  };

  hist_.for_each(hist_buf_, sizeof hist_buf_, from_qh,
                 [&](const ::gplug_hist::HistRecord &r, uint32_t qh, bool have_qh) {
    uint32_t i = idx++;
    bool in_window = have_qh ? (qh >= from_qh) : (i >= idx_from);
    if (!in_window) {   // still useful: carries the counter the first delta is measured against
      if (r.ei_wh != ::gplug_hist::WH_ABSENT) prev_ei = r.ei_wh;
      if (r.eo_wh != ::gplug_hist::WH_ABSENT) prev_eo = r.eo_wh;
      return;
    }
    uint32_t key = have_qh ? qh / bucket : i / bucket;
    if (!b.started || b.key != key || b.has_key != have_qh) { flush(); b.begin(key, have_qh); }
    b.add(r, qh, have_qh);
  });
  flush();
  s += "]}";
  return s;
}

static void csv_local_time(uint32_t epoch, char *out, size_t n) {
  auto t = ESPTime::from_epoch_local((time_t) epoch);   // the time component's timezone (gplug.yaml)
  snprintf(out, n, "%04d-%02d-%02d %02d:%02d", t.year, t.month, t.day_of_month, t.hour, t.minute);
}

// /api/history.csv?from=<qh>&to=<qh> -- every stored record at native 15-min resolution, as a
// download (columns: history_csv.h). `from` inclusive, `to` exclusive, both quarter-hour indices;
// none = everything, including records that never got a timestamp (a device without NTP), which a
// window has to leave out since it cannot place them.
//
// A full year is ~35k rows, ~2.5 MB: far beyond one response body, so it is chunked, and far
// longer than the store's lock may be held (the main loop only try_locks around the quarter-hour
// append). So: snapshot the sector range, then per sector lock / copy 4 kB / unlock, format from
// the copy and send while unlocked. A sector recycled during the export shows up with a sequence
// number newer than the snapshot's and is skipped: its records would come after the ones already
// sent anyway. Records appended to the current sector meanwhile are picked up (limit is read live).
void GplugSmi::handle_history_csv_(AsyncWebServerRequest *req) {
  using namespace ::gplug_hist;
  if (!hist_ok_) return send_json_(req, 503, "{\"error\":\"history disabled\"}");
  auto *pf = req->getParam("from");
  auto *pt = req->getParam("to");
  uint32_t from = pf ? (uint32_t) strtoul(pf->value().c_str(), nullptr, 10) : 0;
  uint32_t to = pt ? (uint32_t) strtoul(pt->value().c_str(), nullptr, 10) : 0;
  bool windowed = from || to;

  // 4 kB sector copy on the heap: the httpd task's stack is ~4 kB in total.
  std::unique_ptr<uint8_t[]> sec_buf(new (std::nothrow) uint8_t[HIST_SECTOR]);
  if (!sec_buf) return send_json_(req, 500, "{\"error\":\"memory\"}");

  uint16_t sectors, oldest, cur;
  uint32_t seq_max;
  {
    std::lock_guard<std::mutex> lock(hist_mutex_);
    const auto &m = hist_.meta();
    sectors = m.sectors; oldest = m.oldest_sec; cur = m.cur_sec; seq_max = m.seq;
  }
  if (!sectors) return send_json_(req, 503, "{\"error\":\"history disabled\"}");

  httpd_req_t *r = *req;
  char disp[80];
  snprintf(disp, sizeof disp, "attachment; filename=\"%s-lastgang.csv\"", App.get_name().c_str());
  httpd_resp_set_type(r, "text/csv; charset=utf-8");
  httpd_resp_set_hdr(r, "Content-Disposition", disp);
  httpd_resp_set_hdr(r, "Cache-Control", "no-cache");

  std::string out;
  out.reserve(2048 + CSV_ROW_MAX);
  out += csv_header();
  CsvState st;
  uint32_t rows = 0;
  bool alive = true;
  auto flush = [&](bool final) {
    if (!alive) return;
    if (!out.empty() && httpd_resp_send_chunk(r, out.data(), out.size()) != ESP_OK) { alive = false; return; }
    out.clear();
    if (final && httpd_resp_send_chunk(r, nullptr, 0) != ESP_OK) alive = false;
  };

  uint32_t n = (uint32_t) ((cur + sectors - oldest) % sectors) + 1;
  for (uint32_t k = 0; k < n && alive; k++) {
    uint16_t sec = (uint16_t) ((oldest + k) % sectors);
    uint16_t limit;
    uint32_t seq;
    {
      std::lock_guard<std::mutex> lock(hist_mutex_);
      if (!hist_.read_sector(sec, sec_buf.get(), &limit, &seq)) continue;
    }
    if (seq > seq_max) continue;
    for (uint16_t s = 0; s < limit; s++) {
      HistRecord rec;
      memcpy(&rec, sec_buf.get() + HIST_HDR + (size_t) s * HIST_REC, HIST_REC);
      if (record_erased(rec) || !record_valid(rec)) continue;
      uint32_t qh = 0;
      bool est = false;
      bool have = qh_tag_parse(rec.qh_tag, &qh, &est);
      bool emit = !windowed || (have && qh >= from && (!to || qh < to));
      csv_row(emit ? &out : nullptr, rec, qh, have, est, st, csv_local_time);
      if (emit) rows++;
      if (out.size() >= 2048) flush(false);
    }
  }
  flush(true);
  ESP_LOGI(TAG, "history: csv export %u rows%s", (unsigned) rows, alive ? "" : " (client went away)");
}

std::string GplugSmi::json_wifi_scan_() {
  std::string s = "[";
  bool first = true;
  for (const auto &r : wifi::global_wifi_component->get_scan_result()) {
    if (r.get_is_hidden()) continue;
    if (!first) s += ",";
    first = false;
    s += "{\"ssid\":\""; json_escape(s, r.get_ssid().c_str());
    s += "\",\"rssi\":" + std::to_string(r.get_rssi()) + ",\"secure\":" + (r.get_with_auth() ? "true" : "false") + "}";
  }
  s += "]";
  return s;
}

}  // namespace gplug_smi
}  // namespace esphome
