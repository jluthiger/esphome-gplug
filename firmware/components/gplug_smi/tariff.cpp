// Dynamic tariffs (issue 51): the price per quarter hour from BKW (feed-in) or CKW (grid usage),
// fetched by the device and served under /api/tariff.
//
// The device fetches because the browser cannot: neither API sends CORS headers, so the SPA's own
// fetch() is refused (checked 2026-11-13). The parsing is in tariff_parser.h (host-tested); this file
// is the plumbing, with three owners like mqtt.cpp:
//   - loop: decides when a fetch is due, starts it, and merges a finished job into tariff_.
//   - the `tariff` task, one per fetch: HTTPS GET with the IDF certificate bundle, the body streamed
//     through the parser in 512 B pieces. It owns its TariffJob until it sets tariff_job_done_, and
//     touches nothing else -- a slow provider holds no lock anyone waits for.
//   - httpd: renders tariff_ and replaces it when the provider changes.
//
// When: only while a provider is chosen (off by default), Wi-Fi is up and the clock is set. Each
// local day of the window is asked for until it is complete: CKW per day, so past days are
// backfilled once per boot; BKW has one URL for today, so only today and tomorrow are asked for.
// Tomorrow only from TOMORROW_FROM_H local time on (CKW publishes ~10:20). An incomplete day is
// asked again after RETRY_MS, a failed attempt after ERROR_RETRY_MS -- one TLS session an hour at
// most while a provider is late, nothing once the window is complete.
//
// Cost: the TLS session is the update check's (~44 kB heap at the peak, firmware/MEMORY.md), plus
// this task's stack. A fetch does not start while the update check or install runs, or when the
// largest free block is below MIN_BLOCK. No flash writes except the provider choice, on save.
#include "gplug_smi.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/components/wifi/wifi_component.h"

#include <esp_crt_bundle.h>
#include <esp_heap_caps.h>
#include <esp_http_client.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <ctime>
#include <new>

namespace esphome {
namespace gplug_smi {

static const char *const TAG = "gplug_smi.tariff";
static constexpr uint32_t EPOCH_SANE = 1600000000u;   // same as gplug_smi.cpp
static_assert(::gplug_tariff::QH_EPOCH == ::gplug_hist::HIST_EPOCH, "tariff and history slots must share one index");

static constexpr uint32_t CHECK_MS = 5000;               // how often the loop looks for a due fetch
static constexpr uint32_t RETRY_MS = 3600000;            // a day still incomplete: ask again in an hour
static constexpr uint32_t ERROR_RETRY_MS = 900000;       // network / HTTP / format error: 15 min
static constexpr int TOMORROW_FROM_H = 11;               // local hour from which tomorrow is asked for
static constexpr uint32_t MIN_BLOCK = 40 * 1024;         // TLS record buffers + handshake, with margin
// ESPHome's own update_task, which runs the same TLS client, has 8 kB; the high-water mark is
// reported in /api/tariff (`stack_free`) so a device reading can tell whether that is too generous.
static constexpr uint32_t TASK_STACK = 8192;
static constexpr size_t BODY_MAX = 64 * 1024;            // BKW's day is ~21 kB, CKW's ~12 kB

// TE_AUTH: the hub refused the key (401/403). Nothing is fetched again until the settings change:
// retrying a wrong key would only spend the account's 100 requests a day.
enum TariffErr : uint8_t { TE_NONE, TE_NET, TE_HTTP, TE_FORMAT, TE_EMPTY, TE_MEMORY, TE_AUTH };
static const char *const TE_NAME[] = {"", "net", "http", "format", "empty", "memory", "auth"};

struct TariffJob {
  explicit TariffJob(::gplug_tariff::Provider p) : prov(p), parser(p) {}
  ::gplug_tariff::Provider prov;
  uint32_t day_qh{0};          // the local day this fetch was started for (scheduler bookkeeping)
  char url[160]{};
  char key[::gplug_tariff::STH_KEY_MAX + 1]{};   // X-API-Key, Swiss Tariff Hub only
  std::atomic<bool> *done{nullptr};
  // Results, written by the task before *done.
  uint8_t error{TE_NONE};
  int16_t http{0};
  uint32_t stack_free{0};
  ::gplug_tariff::TariffParser parser;
  char buf[512];
};

static void tariff_task(void *arg) {
  auto *j = static_cast<TariffJob *>(arg);
  esp_http_client_config_t cfg = {};
  cfg.url = j->url;
  cfg.timeout_ms = 10000;
  cfg.crt_bundle_attach = esp_crt_bundle_attach;
  cfg.buffer_size = 1024;
  cfg.buffer_size_tx = 512;
  cfg.user_agent = "gPlug";
  cfg.keep_alive_enable = false;
  esp_http_client_handle_t c = esp_http_client_init(&cfg);
  if (c != nullptr && j->key[0]) esp_http_client_set_header(c, "X-API-Key", j->key);
  if (c == nullptr) {
    j->error = TE_MEMORY;
  } else {
    esp_err_t err = esp_http_client_open(c, 0);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "connect failed: %s", esp_err_to_name(err));
      j->error = TE_NET;
    } else {
      esp_http_client_fetch_headers(c);
      j->http = (int16_t) esp_http_client_get_status_code(c);
      if (j->http != 200) {
        j->error = (j->http == 401 || j->http == 403) && j->key[0] ? TE_AUTH : TE_HTTP;
      } else {
        size_t total = 0;
        for (;;) {
          int n = esp_http_client_read(c, j->buf, sizeof j->buf);
          if (n < 0) { j->error = TE_NET; break; }
          if (n == 0) {
            if (!esp_http_client_is_complete_data_received(c)) j->error = TE_NET;
            break;
          }
          total += (size_t) n;
          if (total > BODY_MAX) { j->error = TE_FORMAT; break; }
          j->parser.feed(j->buf, (size_t) n);
        }
        if (j->error == TE_NONE) {
          j->parser.finish();
          auto r = j->parser.result();
          if (r == ::gplug_tariff::TariffParser::E_EMPTY) j->error = TE_EMPTY;
          else if (r != ::gplug_tariff::TariffParser::OK) j->error = TE_FORMAT;
        }
      }
    }
    esp_http_client_cleanup(c);
  }
  j->stack_free = (uint32_t) uxTaskGetStackHighWaterMark(nullptr);
  j->done->store(true);
  vTaskDelete(nullptr);
}

// NVS key "tariff": the provider id, and for the hub "sth <tariff_id> <key>" -- three tokens that
// token_ok() allows no space in, so splitting at spaces is unambiguous.
static bool tariff_parse_nvs(const std::string &s, TariffState &st) {
  char buf[8 + ::gplug_tariff::STH_ID_MAX + ::gplug_tariff::STH_KEY_MAX + 4];
  if (s.size() >= sizeof buf) return false;
  memcpy(buf, s.c_str(), s.size() + 1);
  char *id = strchr(buf, ' '), *key = nullptr;
  if (id) { *id++ = 0; key = strchr(id, ' '); if (key) *key++ = 0; }
  if (!::gplug_tariff::provider_from_id(buf, st.provider)) return false;
  if (st.provider != ::gplug_tariff::P_STH) return true;
  if (!id || !key || !::gplug_tariff::token_ok(id, ::gplug_tariff::STH_ID_MAX) ||
      !::gplug_tariff::token_ok(key, ::gplug_tariff::STH_KEY_MAX))
    return false;
  strcpy(st.sth_id, id);
  strcpy(st.sth_key, key);
  return true;
}

void GplugSmi::tariff_setup_() {
  std::string s;
  if (!this->nvs_load_("tariff", s)) return;
  auto st = std::unique_ptr<TariffState>(new (std::nothrow) TariffState());
  if (!st) { ESP_LOGE(TAG, "no memory for the tariff table"); return; }
  if (!tariff_parse_nvs(s, *st)) { ESP_LOGW(TAG, "stored tariff setting invalid, tariffs off"); return; }
  if (st->provider == ::gplug_tariff::P_OFF) return;
  std::lock_guard<std::mutex> lock(tariff_mutex_);
  tariff_ = std::move(st);
}

// Loop task: a finished job's slots go into the table, provided the provider is still the one the
// job was started for (a save during the fetch replaced tariff_).
void GplugSmi::tariff_collect_() {
  TariffJob *j = tariff_job_;
  tariff_job_ = nullptr;
  tariff_job_done_ = false;
  {
    std::lock_guard<std::mutex> lock(tariff_mutex_);
    TariffState *st = tariff_.get();
    if (st != nullptr && st->provider == j->prov) {
      const auto &p = j->parser;
      size_t put = 0;
      if (j->error == TE_NONE) {
        for (size_t i = 0; i < p.count(); i++) put += st->table.put(p.slots()[i].qh, p.slots()[i].price);
        if (p.publication()[0]) snprintf(st->publication, sizeof st->publication, "%s", p.publication());
        st->ok_ms = millis() | 1;
      }
      st->fetching = false;
      st->error = j->error;
      st->http = j->http;
      st->last_count = (uint16_t) put;
      st->last_dropped = p.dropped();
      st->last_bad = p.bad();
      st->stack_free = j->stack_free;
      for (auto &d : st->days) {
        if (d.qh != j->day_qh) continue;
        d.done = j->error == TE_NONE || j->error == TE_EMPTY;
        d.failed = !d.done;
      }
      ESP_LOGI(TAG, "%s: %s, http %d, %u slots (%u dropped, %u bad), stack left %u B",
               ::gplug_tariff::provider_id(j->prov), j->error ? TE_NAME[j->error] : "ok", j->http,
               (unsigned) put, p.dropped(), p.bad(), (unsigned) j->stack_free);
    }
  }
  delete j;
}

void GplugSmi::tariff_start_(::gplug_tariff::Provider p, uint32_t day_qh, int64_t day_utc, const char *sth_id,
                             const char *sth_key) {
  auto *j = new (std::nothrow) TariffJob(p);
  if (j == nullptr) return;
  j->day_qh = day_qh;
  j->done = &tariff_job_done_;
  if (p == ::gplug_tariff::P_BKW) {
    snprintf(j->url, sizeof j->url, "https://api.bkw.ch/api/dyntariffs/v1/Tariffs/energyreturn");
  } else if (p == ::gplug_tariff::P_STH) {
    // start_time is the local midnight as a UTC instant; the hub answers the Swiss day from there.
    int y; unsigned m, d;
    ::gplug_tariff::civil_from_days(::gplug_tariff::floor_div(day_utc, 86400), y, m, d);
    int64_t sod = day_utc - ::gplug_tariff::floor_div(day_utc, 86400) * 86400;
    snprintf(j->url, sizeof j->url, "https://swisstariffhub.ch/api/v1/prices?tariff_id=%s&start_time=%04d-%02u-%02uT%02d:00:00Z",
             sth_id, y, m, d, (int) (sod / 3600));
    snprintf(j->key, sizeof j->key, "%s", sth_key);
  } else {
    int y; unsigned m, d;
    ::gplug_tariff::local_date(day_utc, y, m, d);
    snprintf(j->url, sizeof j->url,
             "https://www.ckw.ch/api/network/tariff?start=%04d-%02u-%02u&tariffType=grid_usage&tariffName=home_dynamic",
             y, m, d);
  }
  tariff_job_done_ = false;
  tariff_job_ = j;
  if (xTaskCreate(tariff_task, "tariff", TASK_STACK, j, tskIDLE_PRIORITY + 1, nullptr) != pdPASS) {
    ESP_LOGW(TAG, "no memory for the fetch task");
    tariff_job_ = nullptr;
    delete j;
    std::lock_guard<std::mutex> lock(tariff_mutex_);
    if (tariff_) { tariff_->error = TE_MEMORY; tariff_->fetching = false; }
    return;
  }
  std::lock_guard<std::mutex> lock(tariff_mutex_);
  if (tariff_) tariff_->fetching = true;
}

void GplugSmi::tariff_service_() {
  if (tariff_job_ != nullptr) {
    if (tariff_job_done_.load()) this->tariff_collect_();
    return;
  }
  uint32_t now = millis();
  if (now - tariff_check_ms_ < CHECK_MS) return;
  tariff_check_ms_ = now;
  if (!wifi::global_wifi_component->is_connected()) return;
  if (upd_checking_ || upd_installing_) return;   // one TLS session at a time
  time_t ep = ::time(nullptr);
  if ((uint32_t) ep < EPOCH_SANE) return;

  namespace gt = ::gplug_tariff;
  const int64_t mid = gt::local_midnight(ep);
  const int64_t local_now = (int64_t) ep + gt::zurich_offset(ep);
  const int hour = (int) ((local_now % 86400 + 86400) % 86400 / 3600);
  gt::Provider prov;
  char sth_id[gt::STH_ID_MAX + 1], sth_key[gt::STH_KEY_MAX + 1];
  uint32_t due_qh = 0;
  int64_t due_utc = 0;
  {
    std::lock_guard<std::mutex> lock(tariff_mutex_);
    TariffState *st = tariff_.get();
    if (st == nullptr) return;
    prov = st->provider;
    if (st->error == TE_AUTH) return;   // until the key or tariff is changed (a save resets the state)
    memcpy(sth_id, st->sth_id, sizeof sth_id);
    memcpy(sth_key, st->sth_key, sizeof sth_key);
    st->table.rebase(gt::qh_of(gt::add_local_days(mid, -TARIFF_DAYS_BACK)));
    for (int k = 0; k < TARIFF_DAYS_BACK + 2; k++) {
      const int off = k - TARIFF_DAYS_BACK;
      const int64_t d0 = gt::add_local_days(mid, off), d1 = gt::add_local_days(mid, off + 1);
      const uint32_t q0 = gt::qh_of(d0), q1 = gt::qh_of(d1);
      // The window moved since this entry was written: find the day's old entry or start afresh.
      if (st->days[k].qh != q0) {
        TariffState::Day keep{q0, 0, false, false};
        for (const auto &d : st->days) if (d.qh == q0) keep = d;
        st->days[k] = keep;
      }
    }
    for (int k = 0; k < TARIFF_DAYS_BACK + 2 && !due_qh; k++) {
      const int off = k - TARIFF_DAYS_BACK;
      auto &d = st->days[k];
      if (prov == gt::P_BKW && off < 0) continue;            // BKW serves today only
      if (off == 1 && hour < TOMORROW_FROM_H) continue;
      const int64_t d0 = gt::add_local_days(mid, off), d1 = gt::add_local_days(mid, off + 1);
      const uint32_t q0 = gt::qh_of(d0), q1 = gt::qh_of(d1);
      // CKW leaves the repeated autumn hour out (tariff_parser.h), so four missing slots still count
      // as complete.
      if (st->table.count(q0, q1) + 4 >= q1 - q0) continue;
      if (off < 0 && d.done) continue;                       // a past day is asked once per boot
      // The hub's free plan allows 100 requests a day, so a failing hub is asked hourly, not every
      // 15 min: five days of backfill plus at most one request an hour stays below that.
      const uint32_t gap = d.failed && prov != gt::P_STH ? ERROR_RETRY_MS : RETRY_MS;
      if (d.try_ms && now - d.try_ms < gap) continue;
      due_qh = q0;
      due_utc = d0;
    }
    if (!due_qh) return;
    size_t block = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    st->heap_before = (uint32_t) block;
    for (auto &d : st->days) if (d.qh == due_qh) { d.try_ms = now | 1; d.failed = false; }
    st->try_ms = now | 1;
    if (block < MIN_BLOCK) {
      ESP_LOGW(TAG, "largest free block %u B, fetch deferred", (unsigned) block);
      st->error = TE_MEMORY;
      for (auto &d : st->days) if (d.qh == due_qh) d.failed = true;
      return;
    }
  }
  this->tariff_start_(prov, due_qh, due_utc, sth_id, sth_key);
}

// GET /api/tariff. Prices are integers in micro-CHF per kWh (`scale`), one per quarter hour from
// `first_qh` on, null where the provider sent none.
std::string GplugSmi::json_tariff_() {
  namespace gt = ::gplug_tariff;
  std::lock_guard<std::mutex> lock(tariff_mutex_);
  const TariffState *st = tariff_.get();
  if (st == nullptr) return "{\"provider\":\"off\"}";
  uint32_t now = millis();
  std::string s;
  s.reserve(TARIFF_SLOTS * 7 + 400);
  s = "{\"provider\":\"";
  s += gt::provider_id(st->provider);
  s += "\",\"kind\":\"";
  s += st->provider == gt::P_BKW ? "feed_in" : st->provider == gt::P_STH ? "total" : "grid";
  s += "\"";
  if (st->provider == gt::P_STH) {
    // The key itself is never sent back, only whether one is stored.
    s += ",\"tariff_id\":\"";
    s += st->sth_id;
    s += "\",\"key_set\":";
    s += st->sth_key[0] ? "true" : "false";
  }
  s += ",\"fetching\":";
  s += st->fetching ? "true" : "false";
  s += ",\"error\":";
  if (st->error) { s += "\""; s += TE_NAME[st->error]; s += "\""; } else s += "null";
  s += ",\"http\":" + std::to_string(st->http);
  s += ",\"fetched_ago\":" + (st->ok_ms ? std::to_string((now - st->ok_ms) / 1000) : std::string("null"));
  s += ",\"tried_ago\":" + (st->try_ms ? std::to_string((now - st->try_ms) / 1000) : std::string("null"));
  s += ",\"publication\":\"";
  for (const char *p = st->publication; *p; p++) if (*p != '"' && *p != '\\' && (uint8_t) *p >= 0x20) s += *p;
  s += "\",\"last\":{\"count\":" + std::to_string(st->last_count) + ",\"dropped\":" + std::to_string(st->last_dropped) +
       ",\"bad\":" + std::to_string(st->last_bad) + "}";
  s += ",\"stack_free\":" + std::to_string(st->stack_free);
  s += ",\"heap_before\":" + std::to_string(st->heap_before);
  s += ",\"scale\":1000000";
  uint32_t first, end;
  if (!st->table.span(first, end)) { s += ",\"first_qh\":null,\"prices\":[]}"; return s; }
  s += ",\"first_qh\":" + std::to_string(first) + ",\"prices\":[";
  for (uint32_t q = first; q < end; q++) {
    if (q != first) s += ',';
    int32_t v = st->table.get(q);
    if (v == gt::PRICE_NONE) s += "null"; else s += std::to_string(v);
  }
  s += "]}";
  return s;
}

// `,"tariff":{...}` for /api/live: the running quarter hour's price, until when it holds and the
// price after that, so the Live tab can show it without the 3.5 kB of /api/tariff every 10 s.
// Nothing when no provider is chosen or the running quarter hour has no price.
void GplugSmi::json_tariff_live_(std::string &s) {
  namespace gt = ::gplug_tariff;
  time_t ep = ::time(nullptr);
  if ((uint32_t) ep < EPOCH_SANE) return;
  const uint32_t q = gt::qh_of(ep);
  std::lock_guard<std::mutex> lock(tariff_mutex_);
  const TariffState *st = tariff_.get();
  if (st == nullptr) return;
  const int32_t now = st->table.get(q);
  if (now == gt::PRICE_NONE) return;
  uint32_t until = q + 1;
  while (st->table.get(until) == now) until++;
  const int32_t next = st->table.get(until);
  s += ",\"tariff\":{\"kind\":\"";
  s += st->provider == gt::P_BKW ? "feed_in" : st->provider == gt::P_STH ? "total" : "grid";
  s += "\",\"now\":" + std::to_string(now);
  s += ",\"until_qh\":" + std::to_string(until);
  s += ",\"next\":" + (next == gt::PRICE_NONE ? std::string("null") : std::to_string(next)) + "}";
}

// POST /api/config/tariff {"provider":"off"|"bkw"|"ckw"|"sth", "tariff_id"?, "key"?}. A different
// provider or tariff starts with an empty table (the old prices belong to another tariff); saving
// the same one keeps what is there. For the hub, `key` left out keeps the stored one, like the MQTT
// password; a new key clears an auth error so the next pass tries again.
void GplugSmi::handle_tariff_post_(AsyncWebServerRequest *req, const std::string &body) {
  namespace gt = ::gplug_tariff;
  gt::Provider p;
  if (!gt::provider_from_body(body.c_str(), p)) return send_json_(req, 400, "{\"error\":\"provider\"}");
  char id[gt::STH_ID_MAX + 1] = "", key[gt::STH_KEY_MAX + 1] = "";
  int has_id = 0, has_key = 0;
  if (p == gt::P_STH) {
    has_id = gt::json_str_field(body.c_str(), "tariff_id", id, sizeof id);
    has_key = gt::json_str_field(body.c_str(), "key", key, sizeof key);
    if (has_id != 1 || !gt::token_ok(id, gt::STH_ID_MAX)) return send_json_(req, 400, "{\"error\":\"tariff_id\"}");
    if (has_key < 0 || (has_key == 1 && !gt::token_ok(key, gt::STH_KEY_MAX))) return send_json_(req, 400, "{\"error\":\"key\"}");
  }
  bool changed = false;
  std::string saved;
  {
    std::lock_guard<std::mutex> lock(tariff_mutex_);
    TariffState *cur = tariff_.get();
    const gt::Provider was = cur ? cur->provider : gt::P_OFF;
    if (p == gt::P_STH && has_key == 0) {
      if (was != gt::P_STH || !cur->sth_key[0]) return send_json_(req, 400, "{\"error\":\"key\"}");
      strcpy(key, cur->sth_key);
    }
    const bool same_tariff = was == p && (p != gt::P_STH || !strcmp(cur->sth_id, id));
    if (same_tariff && p == gt::P_STH && strcmp(cur->sth_key, key)) {
      strcpy(cur->sth_key, key);
      cur->error = 0;
      for (auto &d : cur->days) d.try_ms = 0;
      changed = true;
    } else if (!same_tariff) {
      changed = true;
      if (p == gt::P_OFF) {
        tariff_.reset();
      } else {
        auto st = std::unique_ptr<TariffState>(new (std::nothrow) TariffState());
        if (!st) return send_json_(req, 503, "{\"error\":\"memory\"}");
        st->provider = p;
        strcpy(st->sth_id, id);
        strcpy(st->sth_key, key);
        tariff_ = std::move(st);
      }
    }
  }
  if (changed) {
    saved = gt::provider_id(p);
    if (p == gt::P_STH) { saved += ' '; saved += id; saved += ' '; saved += key; }
    this->defer([this]() { this->log_event_(::gplug_log::EV_CONFIG, 5); });
    this->defer([this, saved]() {
      if (!this->nvs_save_("tariff", saved)) ESP_LOGE(TAG, "nvs_save_(tariff) failed -- applied live but won't survive a reboot");
    });
  }
  send_json_(req, 200, "{\"ok\":true}");
}

}  // namespace gplug_smi
}  // namespace esphome
