// MQTT publishing with user-edited topic and payload templates (issue 14).
//
// esp-mqtt is called directly rather than through ESPHome's `mqtt:` component: that one takes its
// broker from the YAML at compile time and publishes every entity under its own topic scheme, while
// here the broker, the topics and the payloads are runtime data set in the SPA, like the meter
// profile. The template work itself is in mqtt_template.h (host-tested); this file is the plumbing.
//
// Three tasks are involved, and each piece of state has one owner:
//   - httpd: validates a POST completely (settings and both templates compiled against the current
//     profile) into a fresh MqttRun and hands it over through mqtt_pending_. It never touches the
//     client, so a hung broker cannot stall the web server.
//   - loop: owns the client and the running MqttRun. mqtt_service_() picks up a handover, (re)starts
//     the client, recompiles after a meter save, and once per period snapshots the values under
//     mutex_ and renders outside it. Messages go out with esp_mqtt_client_enqueue(): publish()
//     would write to the socket from the loop and stall the UART reader for as long as the network
//     takes.
//   - esp-mqtt's own task: its event handler only writes atomics. No mutex_, no NVS and no event-log
//     records, so a flapping broker costs neither lock time nor flash writes.
//
// NVS is written only when the user saves (deferred to the loop, DECISIONS.md 2026-09-14); there
// are no periodic writes, so MQTT adds nothing to the flash-wear budget.
#include "gplug_smi.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/wifi/wifi_component.h"

#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mqtt_client.h>
#include <cstring>
#include <ctime>
#include <new>

namespace esphome {
namespace gplug_smi {

static const char *const TAG = "gplug_smi.mqtt";
static constexpr uint32_t EPOCH_SANE = 1600000000u;   // same as gplug_smi.cpp
static constexpr uint16_t PERIOD_MIN = 5, PERIOD_MAX = 3600;
// Enough for two periods of the largest default payload; beyond that a broker that is connected
// but not reading loses messages instead of growing the heap.
static constexpr uint64_t OUTBOX_LIMIT = 4096;
// After a failed client init or start, wait before trying again instead of every loop pass.
static constexpr uint32_t START_RETRY_MS = 10000;

// esp_mqtt_client_destroy() takes the client's lock and then waits without a timeout for the
// esp-mqtt task to exit. That task holds the lock through a DNS lookup and TCP connect and sleeps
// half the reconnect interval between attempts, so on the loop task a save while the broker is
// unreachable blocked for 5-10 s -- past the 5 s task watchdog (found in review, 2026-09-26). The
// old client is therefore torn down on a short-lived task of its own; the new one starts at once.
static void destroy_task(void *client) {
  esp_mqtt_client_destroy(static_cast<esp_mqtt_client_handle_t>(client));
  vTaskDelete(nullptr);
}

static void mqtt_defaults(MqttSettings &s) {
  s = MqttSettings{};
  const auto &p = ::gplug_mqtt::PRESETS[0];
  s.each = p.each;
  strlcpy(s.topic, p.topic, sizeof s.topic);
  strlcpy(s.payload, p.payload, sizeof s.payload);
}

// Whether the change needs a new connection; a template, period, QoS or retain change does not.
static bool conn_differs(const MqttSettings &a, const MqttSettings &b) {
  return a.enabled != b.enabled || a.port != b.port || strcmp(a.host, b.host) || strcmp(a.client_id, b.client_id) ||
         strcmp(a.user, b.user) || strcmp(a.password, b.password);
}

// ---------------------------------------------------------------- config

void GplugSmi::mqtt_setup_() {
  auto s = std::unique_ptr<MqttSettings>(new MqttSettings());
  mqtt_defaults(*s);
  std::string stored;
  if (this->nvs_load_("mqtt", stored)) {
    MqttErr e;
    if (!this->mqtt_parse_(stored, *s, e)) {
      ESP_LOGW(TAG, "stored config invalid (%s), MQTT off", e.code);
      mqtt_defaults(*s);
    }
  }
  if (s->enabled) {
    auto run = std::unique_ptr<MqttRun>(new MqttRun());
    run->cfg = *s;
    MqttErr e;
    // A template that no longer fits the profile pauses publishing and says why in /api/status;
    // it must not stop the boot or discard what the user typed.
    if (!this->mqtt_compile_(*run, e)) {
      ESP_LOGW(TAG, "stored %s template does not compile: %s at %u", e.field ? e.field : "?", e.code, e.pos);
      mqtt_tpl_err_ = e.code;
    }
    mqtt_run_ = std::move(run);
  }
  std::lock_guard<std::mutex> lock(mqtt_mutex_);
  mqtt_cfg_ = std::move(s);
}

static bool copy_str(JsonVariantConst v, char *dst, size_t cap) {
  if (v.isNull()) return true;   // left out: keep
  if (!v.is<const char *>()) return false;
  const char *s = v.as<const char *>();
  if (strlen(s) >= cap) return false;
  strlcpy(dst, s, cap);
  return true;
}

// Fields left out keep the value already in s: the SPA sends everything, and a script can change
// one field. The password follows the same rule, which is what lets the SPA save without ever
// having seen it; "" clears it.
bool GplugSmi::mqtt_parse_(const std::string &json, MqttSettings &s, MqttErr &e) {
  JsonDocument doc;
  if (deserializeJson(doc, json) || !doc.is<JsonObject>()) { e.code = "json"; return false; }
  auto fail = [&](const char *code) { e.code = code; return false; };
  if (!doc["enabled"].isNull()) s.enabled = doc["enabled"] | false;
  if (!doc["retain"].isNull()) s.retain = doc["retain"] | false;
  if (!copy_str(doc["host"], s.host, sizeof s.host)) return fail("host");
  if (!copy_str(doc["client_id"], s.client_id, sizeof s.client_id)) return fail("client_id");
  if (!copy_str(doc["user"], s.user, sizeof s.user)) return fail("user");
  if (!copy_str(doc["password"], s.password, sizeof s.password)) return fail("password");
  if (!doc["port"].isNull()) {
    long p = doc["port"] | -1L;
    if (p < 1 || p > 65535) return fail("port");
    s.port = (uint16_t) p;
  }
  if (!doc["period"].isNull()) {
    long p = doc["period"] | -1L;
    if (p < PERIOD_MIN || p > PERIOD_MAX) return fail("period");
    s.period = (uint16_t) p;
  }
  if (!doc["qos"].isNull()) {
    long q = doc["qos"] | -1L;
    if (q != 0 && q != 1) return fail("qos");
    s.qos = (uint8_t) q;
  }
  if (!doc["mode"].isNull()) {
    const char *m = doc["mode"] | "";
    if (!strcmp(m, "period")) s.each = false;
    else if (!strcmp(m, "each")) s.each = true;
    else return fail("mode");
  }
  // Template length is a template error with a position, like the ones compile() reports.
  struct { const char *key; char *dst; size_t cap; } tpls[] = {
      {"topic", s.topic, sizeof s.topic}, {"payload", s.payload, sizeof s.payload}};
  for (auto &t : tpls) {
    JsonVariantConst v = doc[t.key];
    if (v.isNull()) continue;
    if (!v.is<const char *>()) return fail("json");
    size_t len = strlen(v.as<const char *>());
    if (len >= t.cap) { e.code = "tpl_too_long"; e.field = t.key; e.pos = (uint16_t) len; return false; }
    strlcpy(t.dst, v.as<const char *>(), t.cap);
  }
  if (s.enabled && !s.host[0]) return fail("host");
  return true;
}

// Copies the current profile into r and compiles both templates against it. Runs on the httpd task
// (POST) and the loop task (boot, after a meter save); the lock is held only for the copy.
bool GplugSmi::mqtt_compile_(MqttRun &r, MqttErr &e) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    r.gen = desc_gen_;
    r.n = desc_.n;
    for (uint8_t i = 0; i < desc_.n; i++) {
      const ObisEntry &o = desc_.obis[i];
      memcpy(r.names[i], o.name, sizeof r.names[i]);
      memcpy(r.obis[i], o.obis, sizeof r.obis[i]);
      memcpy(r.units[i], o.unit, sizeof r.units[i]);
      r.prec[i] = o.precision;
      r.is_string[i] = o.is_string;
    }
  }
  for (uint8_t i = 0; i < r.n; i++) { r.np[i] = r.names[i]; r.op[i] = r.obis[i]; r.up[i] = r.units[i]; }
  get_mac_address_into_buffer(r.mac);
  ::gplug_mqtt::Fields f{r.np, r.op, r.up, r.prec, r.is_string, r.n, App.get_name().c_str(), r.mac};
  ::gplug_mqtt::Error te;
  r.ok = false;
  if (!::gplug_mqtt::compile(r.cfg.topic, strlen(r.cfg.topic), r.cfg.each, true, f, r.topic, te)) {
    e.code = te.code; e.field = "topic"; e.pos = te.pos; e.worst = te.worst;
    return false;
  }
  if (!::gplug_mqtt::compile(r.cfg.payload, strlen(r.cfg.payload), r.cfg.each, false, f, r.payload, te)) {
    e.code = te.code; e.field = "payload"; e.pos = te.pos; e.worst = te.worst;
    return false;
  }
  r.ok = true;
  return true;
}

// for_nvs: the stored form, password included. Otherwise the GET body: no password, but what the
// SPA needs to preview exactly what the device would send (device name, MAC, the profile's OBIS
// codes and units, which /api/live does not carry).
std::string GplugSmi::mqtt_json_(const MqttSettings &s, bool for_nvs) {
  JsonDocument doc;
  if (for_nvs) doc["v"] = 1;
  doc["enabled"] = s.enabled;
  doc["host"] = s.host;
  doc["port"] = s.port;
  doc["client_id"] = s.client_id;
  doc["user"] = s.user;
  if (for_nvs) doc["password"] = s.password;
  else doc["password_set"] = s.password[0] != 0;
  doc["mode"] = s.each ? "each" : "period";
  doc["topic"] = s.topic;
  doc["payload"] = s.payload;
  doc["period"] = s.period;
  doc["qos"] = s.qos;
  doc["retain"] = s.retain;
  if (!for_nvs) {
    char mac[13];
    get_mac_address_into_buffer(mac);
    doc["ctx"]["device"] = App.get_name().c_str();
    doc["ctx"]["mac"] = (const char *) mac;
    JsonArray fields = doc["fields"].to<JsonArray>();
    std::lock_guard<std::mutex> lock(mutex_);
    for (uint8_t i = 0; i < desc_.n; i++) {
      const ObisEntry &o = desc_.obis[i];
      JsonObject f = fields.add<JsonObject>();
      f["name"] = (const char *) o.name;
      f["obis"] = (const char *) o.obis;
      f["unit"] = (const char *) o.unit;
      f["prec"] = o.precision;
      if (o.is_string) f["string"] = true;
    }
  }
  std::string out;
  serializeJson(doc, out);
  return out;
}

void GplugSmi::handle_mqtt_get_(AsyncWebServerRequest *req) {
  // MqttSettings is ~1 kB: a copy on the heap, not on the httpd task's stack.
  auto s = std::unique_ptr<MqttSettings>(new (std::nothrow) MqttSettings());
  if (!s) return send_json_(req, 503, "{\"error\":\"memory\"}");
  {
    std::lock_guard<std::mutex> lock(mqtt_mutex_);
    *s = *mqtt_cfg_;
  }
  send_json_(req, 200, this->mqtt_json_(*s, false));
}

static std::string err_json(const MqttErr &e) {
  JsonDocument doc;
  doc["error"] = e.code;
  if (e.field) {
    doc["field"] = e.field;
    doc["pos"] = e.pos;
  }
  if (e.worst) doc["worst"] = e.worst;
  std::string out;
  serializeJson(doc, out);
  return out;
}

void GplugSmi::handle_mqtt_post_(AsyncWebServerRequest *req, const std::string &body) {
  // ~8 kB on top of the running copy: refuse the save rather than abort (no exceptions in this build).
  auto s = std::unique_ptr<MqttSettings>(new (std::nothrow) MqttSettings());
  auto run = std::unique_ptr<MqttRun>(new (std::nothrow) MqttRun());
  if (!s || !run) return send_json_(req, 503, "{\"error\":\"memory\"}");
  {
    std::lock_guard<std::mutex> lock(mqtt_mutex_);
    *s = *mqtt_cfg_;
  }
  MqttErr e;
  if (!this->mqtt_parse_(body, *s, e)) return send_json_(req, 400, err_json(e));
  // Compiled even when disabled, so a template that would not work is refused now rather than
  // discovered when MQTT is switched on.
  run->cfg = *s;
  if (!this->mqtt_compile_(*run, e)) return send_json_(req, 400, err_json(e));
  std::string saved = this->mqtt_json_(*s, true);
  {
    std::lock_guard<std::mutex> lock(mqtt_mutex_);
    mqtt_pending_reconnect_ |= conn_differs(*mqtt_cfg_, *s);
    *mqtt_cfg_ = *s;
    if (s->enabled) mqtt_pending_ = std::move(run);
    else mqtt_pending_.reset();
    mqtt_pending_set_ = true;
  }
  this->defer([this]() { this->log_event_(::gplug_log::EV_CONFIG, 4); });
  this->defer([this, saved]() {
    if (!this->nvs_save_("mqtt", saved)) ESP_LOGE(TAG, "nvs_save_(mqtt) failed -- applied live but won't survive a reboot");
  });
  send_json_(req, 200, "{\"ok\":true}");
}

void GplugSmi::json_mqtt_status_(std::string &s) {
  static const char *const STATE[] = {"off", "connecting", "connected"};
  static const char *const CONN_ERR[] = {"", "tcp", "refused", "auth"};
  const char *tpl = mqtt_tpl_err_.load();
  uint8_t st = mqtt_state_.load(), ce = mqtt_conn_err_.load();
  uint32_t pub = mqtt_pub_ms_.load();
  s += ",\"mqtt\":{\"state\":\"";
  s += tpl ? "error" : STATE[st < 3 ? st : 0];
  s += "\",\"error\":\"";
  s += tpl ? tpl : CONN_ERR[ce < 4 ? ce : 0];
  s += "\",\"sent\":" + std::to_string(mqtt_sent_.load());
  s += ",\"dropped\":" + std::to_string(mqtt_dropped_.load());
  s += ",\"skipped\":" + std::to_string(mqtt_skipped_.load());
  s += ",\"last_ago\":" + (pub ? std::to_string((millis() - pub) / 1000) : std::string("null")) + "}";
}

// ---------------------------------------------------------------- client

void GplugSmi::mqtt_event_(void *arg, const char *base, int32_t id, void *data) {
  auto *self = static_cast<GplugSmi *>(arg);
  auto *ev = static_cast<esp_mqtt_event_handle_t>(data);
  // A client being torn down on destroy_task may still report; only the current one sets state.
  if (ev->client != self->mqtt_client_.load()) return;
  switch ((esp_mqtt_event_id_t) id) {
    case MQTT_EVENT_BEFORE_CONNECT:
      self->mqtt_state_ = MQ_CONNECTING;
      break;
    case MQTT_EVENT_CONNECTED:
      self->mqtt_state_ = MQ_CONNECTED;
      self->mqtt_conn_err_ = MQE_NONE;
      break;
    case MQTT_EVENT_DISCONNECTED:
      self->mqtt_state_ = MQ_CONNECTING;   // esp-mqtt reconnects on its own
      break;
    case MQTT_EVENT_ERROR:
      if (ev->error_handle == nullptr) break;
      if (ev->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
        auto rc = ev->error_handle->connect_return_code;
        self->mqtt_conn_err_ = rc == MQTT_CONNECTION_REFUSE_BAD_USERNAME || rc == MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED
                                   ? MQE_AUTH : MQE_REFUSED;
      } else if (ev->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
        self->mqtt_conn_err_ = MQE_TCP;
      }
      break;
    default:
      break;
  }
  // Only the task itself can cheaply read its own high-water mark (like stack_loop in mem_service_).
  self->mqtt_stack_free_ = uxTaskGetStackHighWaterMark(nullptr);
}

void GplugSmi::mqtt_start_() {
  const MqttSettings &c = mqtt_run_->cfg;
  esp_mqtt_client_config_t mc{};
  mc.broker.address.hostname = c.host;
  mc.broker.address.port = c.port;
  mc.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
  mc.credentials.client_id = c.client_id[0] ? c.client_id : App.get_name().c_str();
  if (c.user[0]) mc.credentials.username = c.user;
  if (c.password[0]) mc.credentials.authentication.password = c.password;
  mc.session.keepalive = 60;
  mc.network.reconnect_timeout_ms = 10000;
  // Bounds every socket write the esp-mqtt task makes while holding the client lock, which
  // esp_mqtt_client_enqueue() on the loop task waits for. A LAN broker answers in milliseconds.
  mc.network.timeout_ms = 2000;
  // Nothing large ever comes in (no subscriptions: CONNACK, PUBACK, PINGRESP); a payload longer
  // than the out buffer is sent in fragments from the outbox copy.
  mc.buffer.size = 512;
  mc.buffer.out_size = 1024;
  mc.task.stack_size = 4096;
  mc.outbox.limit = OUTBOX_LIMIT;
  // esp_mqtt_client_init() copies every string, so the settings may change underneath it.
  esp_mqtt_client_handle_t cl = esp_mqtt_client_init(&mc);
  if (cl == nullptr) {
    ESP_LOGE(TAG, "client init failed");
    mqtt_retry_ms_ = millis();
    return;
  }
  esp_mqtt_client_register_event(cl, MQTT_EVENT_ANY, &GplugSmi::mqtt_event_, this);
  mqtt_conn_err_ = MQE_NONE;
  mqtt_state_ = MQ_CONNECTING;
  mqtt_client_ = cl;
  if (esp_mqtt_client_start(cl) != ESP_OK) {
    // Never started, so destroy() does not wait for a task.
    ESP_LOGE(TAG, "client start failed");
    mqtt_client_ = nullptr;
    esp_mqtt_client_destroy(cl);
    mqtt_state_ = MQ_OFF;
    mqtt_retry_ms_ = millis();
    return;
  }
  ESP_LOGI(TAG, "connecting to %s:%u", c.host, c.port);
}

void GplugSmi::mqtt_stop_() {
  esp_mqtt_client_handle_t cl = mqtt_client_.exchange(nullptr);
  if (cl == nullptr) return;
  // See destroy_task. If the task cannot be created, blocking here is still better than leaking a
  // running client.
  if (xTaskCreate(destroy_task, "mqtt_stop", 3072, cl, tskIDLE_PRIORITY + 1, nullptr) != pdPASS)
    esp_mqtt_client_destroy(cl);
  mqtt_state_ = MQ_OFF;
  mqtt_conn_err_ = MQE_NONE;
}

void GplugSmi::mqtt_service_() {
  bool reconnect = false;
  {
    std::lock_guard<std::mutex> lock(mqtt_mutex_);
    if (mqtt_pending_set_) {
      mqtt_run_ = std::move(mqtt_pending_);
      reconnect = mqtt_pending_reconnect_;
      mqtt_pending_set_ = mqtt_pending_reconnect_ = false;
      mqtt_tpl_err_ = nullptr;
    }
  }
  if (reconnect) this->mqtt_stop_();
  if (!mqtt_run_) {
    this->mqtt_stop_();
    return;
  }
  MqttRun &r = *mqtt_run_;
  uint32_t now = millis();
  if (mqtt_client_.load() == nullptr) {
    if (!wifi::global_wifi_component->is_connected()) return;
    if (mqtt_retry_ms_ && now - mqtt_retry_ms_ < START_RETRY_MS) return;
    mqtt_retry_ms_ = 0;
    this->mqtt_start_();
    mqtt_last_ms_ = now;
    return;
  }
  // Checked on every pass, not per period: with a long period /api/status would otherwise report a
  // template that no longer fits the new profile only up to an hour later.
  uint32_t gen;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    gen = desc_gen_;
  }
  if (gen != r.gen) {
    // The meter profile changed since the templates were compiled. Register indices and the worst
    // case depend on it, so compile again; on failure publishing pauses until the user edits the
    // template or the profile changes again.
    MqttErr e;
    if (this->mqtt_compile_(r, e)) {
      mqtt_tpl_err_ = nullptr;
    } else {
      ESP_LOGW(TAG, "%s template no longer compiles after the profile change: %s at %u", e.field, e.code, e.pos);
      mqtt_tpl_err_ = e.code;
    }
  }
  if (!r.ok) return;
  if (now - mqtt_last_ms_ < (uint32_t) r.cfg.period * 1000) return;
  mqtt_last_ms_ = now;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    // Stale values are not published: a meter that stopped sending would otherwise keep reporting
    // its last power reading as current.
    bool meter_ok = last_frame_ms_ != 0 && (now - last_frame_ms_) < 30000;
    if (!meter_ok || desc_gen_ != r.gen) return;
    memcpy(r.v, values_, sizeof(float) * r.n);
    memcpy(r.have, have_, sizeof(bool) * r.n);
    memcpy(r.smid, smid_, sizeof r.smid);
  }
  this->mqtt_publish_(r);
}

void GplugSmi::mqtt_publish_(MqttRun &r) {
  using namespace ::gplug_mqtt;
  uint32_t ep = (uint32_t) ::time(nullptr);
  if ((r.topic.uses_time || r.payload.uses_time) && ep <= EPOCH_SANE) {
    mqtt_skipped_++;
    return;
  }
  Fields f{r.np, r.op, r.up, r.prec, r.is_string, r.n, App.get_name().c_str(), r.mac};
  Snapshot snap{r.v, r.have, r.smid, ep};
  bool connected = mqtt_state_ == MQ_CONNECTED;
  auto send = [&](int item) {
    size_t tl = 0, pl = 0;
    if (!render(r.cfg.topic, r.topic, true, f, snap, item, r.topic_buf, sizeof r.topic_buf, tl) ||
        !render(r.cfg.payload, r.payload, false, f, snap, item, r.payload_buf, sizeof r.payload_buf, pl) ||
        tl == 0 || pl == 0) {   // e.g. a topic of just {meter} while the meter sends no ID
      mqtt_dropped_++;
      return;
    }
    // Live values go stale anyway, so nothing is queued while disconnected; retain covers a
    // subscriber that joins later. `sent` counts messages handed to the outbox.
    if (!connected ||
        esp_mqtt_client_enqueue(mqtt_client_.load(), r.topic_buf, r.payload_buf, (int) pl, r.cfg.qos, r.cfg.retain, true) < 0) {
      mqtt_dropped_++;
      return;
    }
    mqtt_sent_++;
    mqtt_pub_ms_ = millis();
  };
  if (!r.cfg.each) {
    send(-1);
    return;
  }
  for (uint8_t i = 0; i < r.n; i++)
    if (item_ok(f, snap, i)) send(i);
}

}  // namespace gplug_smi
}  // namespace esphome
