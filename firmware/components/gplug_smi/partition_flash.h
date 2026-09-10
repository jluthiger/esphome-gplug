// Device flash backend for HistoryStore: the `data` partition, addressed raw. This is the only
// file that includes esp_partition.h -- history_store.h stays ESPHome- and IDF-free so the host
// tests exercise the same code the device runs.
//
// The partition is declared in gplug.yaml as type data / subtype spiffs, but nothing ever mounted a
// filesystem on it; we take the subtype purely as a name to find it by. Offset and size are read at
// runtime (and logged in dump_config) rather than hardcoded, because the partition table is
// generated and its offsets are derived, not fixed.
//
// Two traps worth knowing:
//   * esp_partition_write does NOT fail when the target bytes are not erased -- it silently ANDs
//     into them, producing garbage that reads back as plausible data. Every write must therefore
//     target virgin bytes, which HistoryStore's append-only, two-phase scheme guarantees.
//   * With flash encryption enabled, writes must be 16 B aligned and sized, which 20 B records
//     violate. This project enables neither flash encryption nor secure boot, so the plain
//     (non-_raw) API is correct here; that assumption is checked in begin().
#pragma once
#include <cstddef>
#include <cstdint>
#include <esp_partition.h>
#ifdef CONFIG_SECURE_FLASH_ENC_ENABLED
#error "history storage needs 16 B aligned writes when flash encryption is on -- see partition_flash.h"
#endif

namespace esphome {
namespace gplug_smi {

class PartitionFlash {
 public:
  bool begin() {
    part_ = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "data");
    return part_ != nullptr;
  }

  bool read(uint32_t off, void *dst, size_t n) {
    return part_ && esp_partition_read(part_, off, dst, n) == ESP_OK;
  }
  bool write(uint32_t off, const void *src, size_t n) {
    return part_ && esp_partition_write(part_, off, src, n) == ESP_OK;
  }
  bool erase(uint32_t off, size_t n) {
    return part_ && esp_partition_erase_range(part_, off, n) == ESP_OK;
  }

  uint32_t size() const { return part_ ? part_->size : 0; }
  uint32_t address() const { return part_ ? part_->address : 0; }

 private:
  const esp_partition_t *part_{nullptr};
};

}  // namespace gplug_smi
}  // namespace esphome
