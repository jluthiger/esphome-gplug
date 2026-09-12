# Flash and RAM usage

Snapshot of the `dev.yaml` build from 2026-09-11 19:50 (ESPHome 2026.6.5, ESP-IDF 5.5.4, ESP32-C3,
4 MB flash). Sizes in kB = 1024 bytes. Numbers come from the linker map, not from a running device,
except where noted.

To regenerate after a build:

```
B=.esphome/build/gplug/.pioenvs/gplug
~/.platformio/penv/bin/python -m esp_idf_size $B/firmware.map              # summary
~/.platformio/penv/bin/python -m esp_idf_size --archives $B/firmware.map   # per library
~/.platformio/penv/bin/python -m esp_idf_size --files $B/firmware.map      # per object file
python <esp-idf>/components/partition_table/gen_esp32part.py $B/partitions.bin   # real partition table
```

## Flash: partition table

Decoded from the flashed `partitions.bin`. `gplug.yaml` only adds `data`; the rest is ESPHome's
standard ESP-IDF layout.

| Partition | Offset | Size | Holds |
|---|---|---|---|
| bootloader | `0x000000` | 32 kB | second-stage bootloader, 20.6 kB used |
| partition table | `0x008000` | 4 kB | |
| otadata | `0x009000` | 8 kB | which app slot boots next |
| phy_init | `0x00B000` | 4 kB | RF calibration |
| (gap) | `0x00C000` | 16 kB | padding, app slots start on a 64 kB boundary |
| app0 | `0x010000` | 1408 kB | firmware image; `esphome run` hardcodes this offset |
| app1 | `0x170000` | 1408 kB | OTA target, roles swap after an update |
| nvs | `0x2D0000` | 448 kB | Wi-Fi credentials, `hw` and `meter` JSON, ESPHome preferences; mostly empty |
| data | `0x340000` | 704 kB | 15-min history: 176 sectors x 204 records x 20 B, ~374 days |
| (unused) | `0x3F0000` | 64 kB | |

## Flash: the app image

Image 1,036,230 B = 1012 kB in a 1408 kB slot: **71.9 % full, 396 kB headroom** (2026-09-12, after
the load-profile CSV export and the four-language SPA).

By section: 747 kB code run from flash, 176 kB read-only data, 59 kB IRAM code and 11 kB `.data`
initial values (those two are stored in flash *and* occupy RAM).

By owner:

| Part | Size | Share of image |
|---|---|---|
| Wi-Fi driver, WPA supplicant, PHY (`libnet80211`, `libpp`, `libwpa_supplicant`, `libphy`) | 305.8 kB | 30.8 % |
| ESP-IDF system (FreeRTOS, libc/printf, HAL, flash + NVS drivers, heap, UART, OTA) | 200.8 kB | 20.2 % |
| Crypto (mbedTLS for AES-GCM, Noise/Ed25519 for the encrypted API) | 138.9 kB | 14.0 % |
| Networking (lwIP, ESP-IDF HTTP server + parser, mDNS) | 136.7 kB | 13.8 % |
| String literals from all code | 82.5 kB | 8.3 % |
| ESPHome core and components | 57.6 kB | 5.8 % |
| `gplug_smi` code | 40.6 kB | 4.1 % |
| Embedded web files, gzipped: SPA 35.3 kB (four languages since 2026-09-12), presets 2.4 kB (from 26.4 kB JSON), captive page 3.1 kB | 40.8 kB | 4.0 % |
| ESPHome-generated `main.cpp` setup code | 3.0 kB | 0.3 % |

The platform (Wi-Fi, ESP-IDF, crypto, networking) is 79 % of the image; gPlug's own code and web
files are about 7 %.

**Misleading attribution:** `esp_idf_size` reports ~80 kB of `.rodata` in `api_connection.cpp.o`.
That is the linker's merged string-literal pool (`.rodata.*.str1.4`): string literals from every
object are deduplicated into one section, and the whole section is credited to the first object
that contributed. It is not API code.

## RAM: static

The C3 has 314 kB (321,296 B) of SRAM usable by the app; IRAM and DRAM share it.

| Part | Size | Share of SRAM |
|---|---|---|
| IRAM code (ESP-IDF 34.6 kB: interrupts, flash driver, scheduler; Wi-Fi 24.1 kB) | 59.3 kB | 18.9 % |
| `GplugSmi` object (`gplug_smi__gplug_smi_gplugsmi_id__pstorage`) | 19.9 kB | 6.3 % |
| Wi-Fi globals (connection manager, power management, WPA state) | 15.4 kB | 4.9 % |
| ESP-IDF globals (scheduler lists, 1.5 kB ISR stack, stdio, driver state) | 9.7 kB | 3.1 % |
| ESPHome loop task stack (8 kB static) + TCB | 8.4 kB | 2.7 % |
| lwIP and mDNS (DNS table, mDNS task stack) | 4.6 kB | 1.5 % |
| Other component objects (logger buffer 1.4 kB, remaining ESPHome components) | 3.0 kB | 1.0 % |
| **Static total** | **120.3 kB** | **38.4 %** |
| **Left for the heap at boot** | **193.4 kB** | **61.6 %** |

### Inside the 19.9 kB `GplugSmi` object

| Member | Size |
|---|---|
| `frames_`: Datenstrom frame log, `FRAME_LOG_LEN` 5 x (`FRAME_LOG_RAW_CAP` 1280 + `FRAME_LOG_PLAIN_CAP` 768) | 10.0 kB |
| `ring_`: 360 `Sample`s x 10 B, 1 h at 10 s | 3.5 kB |
| `desc_`: meter descriptor, 48 OBIS slots | ~3.0 kB |
| `dsmr_`: DSMR telegram buffer | 2.0 kB |
| `values_`, `have_`, `qh_values_`, `smid_` | 0.5 kB |
| decoders, history store, remaining state | 0.9 kB |

The frame log is half the object. If RAM gets tight, `FRAME_LOG_LEN` and `FRAME_LOG_RAW_CAP` in
`gplug_smi.h` are the first things to shrink. The DLMS decoder's frame/APDU buffers are
`std::vector`s and live on the heap, not in this object.

## RAM: heap at runtime

These are configured sizes from `sdkconfig.gplug` and the code, not measurements:

| Consumer | Size |
|---|---|
| Wi-Fi static RX buffers, 10 x ~1.6 kB | ~16 kB |
| Wi-Fi dynamic RX / TX buffers, up to 32 each | on demand |
| TCP send / receive window per socket (`CONFIG_LWIP_TCP_SND_BUF_DEFAULT` / `WND`) | 5.6 kB each |
| HAN UART RX buffer (`rx_buffer_size`) | 2 kB |
| DLMS frame and APDU vectors, <= 1280 B each | ~3-5 kB |
| HTTP server task stack | ~4.3 kB |
| Main task stack (`CONFIG_ESP_MAIN_TASK_STACK_SIZE`) | 3.5 kB |
| lwIP TCP/IP task stack | 3 kB |
| System event task stack | 2.3 kB |
| Meter descriptor copy, only during a config save (heap on purpose, too big for the httpd stack) | ~3 kB |

Last device reading: **197 kB free heap** with Wi-Fi joined and the SPA loaded (2026-09-10, older
962 kB build, via `/api/status`). That is more than the 193 kB the linker leaves because the runtime
heap also gets RAM the bootloader releases after startup, so the two figures don't subtract. Target
in `intent/intent.md`: >= 80 kB free after 24 h. A current reading still has to be taken on a device.
