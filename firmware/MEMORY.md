# Flash and RAM usage

Snapshot of the `dev.yaml` build from 2026-09-25 (ESPHome 2026.6.5, ESP-IDF 5.5.4, ESP32-C3,
4 MB flash), with the Home Assistant entities and the update-from-release components. Sizes in kB = 1024 bytes. Numbers come from the
linker map, not from a running device, except where noted.

The two tables marked *generated* are the output of `tools/size_report.py`, which reads the linker
map and the ELF symbols of the last build. Paste them, with the baseline line below, after a build
that changes the image; `tools/size_report.py --check` fails once the build has drifted more than
1 kB (image) or 0.5 kB (static RAM, `GplugSmi` object) from that line, and a Claude Code hook runs
it after every `esphome compile`. Snapshots up to 2026-09-12 were grouped by hand from
`esp_idf_size` output, so their row values are not comparable with the generated ones; the image
and static-RAM totals are.

<!-- size-baseline image=1200850 dram=129820 gplug_smi_obj=25176 -->

```
tools/size_report.py                     # tables + baseline line for this file
tools/size_report.py --check             # does this file still match the build?
B=.esphome/build/gplug/.pioenvs/gplug
~/.platformio/penv/bin/python -m esp_idf_size --files $B/firmware.map      # per object file, for digging
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

Image 1,200,850 B = 1173 kB in a 1408 kB slot: **83.3 % full, 235 kB headroom** (2026-09-25, with
install from the release (issue #12): +128 kB. Of that, ~126 kB is ESPHome's `http_request`,
`update` and `ota: http_request` platforms with HTTPS -- mbedTLS TLS + X.509 +70 kB (the common-CA
bundle alone 18 kB of read-only data), `esp_http_client` + `esp-tls` in the ESP-IDF row ~14 kB, the
ESPHome components +17.5 kB, strings +12 kB -- and 3 kB `gplug_smi` code plus 1.8 kB SPA. Measured
first with the YAML alone: 1,195,608 B. 1,069,558 B = 74.2 % before, 2026-09-25, with
the stored-key fingerprint and `/api/key/check` (issue #11): +1.3 kB, of which 0.7 kB SPA check
and strings and 0.5 kB `gplug_smi` code for the SHA-256 call and the route; 1,068,292 B before, 2026-09-16, with
the captive page's copyable address, copy button and screenshot/router hints in four languages:
+1.3 kB, all of it the gzipped captive page; the MAC-suffixed name before it added 0.5 kB that was
not recorded here; 1,066,460 B before, 2026-09-14, with chart pointer values, the restart button and the per-screen tab title: +2.5 kB, all of it SPA
(44.3 -> 46.7 kB) and its strings; 1,063,994 B before, with collapsible Setup cards: +1.4 kB, of which 0.8 kB SPA and 0.5 kB `gplug_smi` code from moving the
event-log write and the URL buffer off the httpd task; 1,062,602 B before, with heap monitoring: +3.3 kB, of which 1.3 kB SPA Memory card and strings, 1.5 kB `gplug_smi` code for
the sampler, `/api/heap` and two more HA entities; 1,059,332 B before, with the Home Assistant
entities: +8.7 kB for the sensor and text_sensor cores, 21 entities and their publishing; 1,050,616 B
before that, with the wide-screen SPA; 1,041,930 B = 72.3 % on 2026-09-12).

By section: 858 kB code run from flash, 244 kB read-only data, 59 kB IRAM code and 11 kB `.data` initial values
(those two are stored in flash *and* occupy RAM).

By owner (*generated*):

| Part | Size | Share of image |
|---|---|---|
| Wi-Fi driver, WPA supplicant, PHY (`libnet80211`, `libpp`, `libwpa_supplicant`, `libphy`) | 298.2 kB | 25.4 % |
| ESP-IDF system (FreeRTOS, libc/printf, HAL, flash + NVS drivers, heap, UART, OTA, HTTP client + esp-tls) | 228.0 kB | 19.4 % |
| Crypto (mbedTLS: AES-GCM, TLS + X.509 + CA bundle for the update check; Noise/Ed25519 for the encrypted API) | 206.5 kB | 17.6 % |
| Networking (lwIP, ESP-IDF HTTP server + parser, mDNS) | 140.3 kB | 12.0 % |
| String literals from all code | 91.9 kB | 7.8 % |
| ESPHome core and components (incl. the captive_portal fork) | 84.9 kB | 7.2 % |
| `gplug_smi` code | 55.4 kB | 4.7 % |
| Embedded web files, gzipped except the PNG: SPA 49.2 kB, captive page 4.7 kB, icon 3.6 kB, presets 1.6 kB, manifest 0.2 kB | 59.9 kB | 5.1 % |
| ESPHome-generated `main.cpp` setup code | 6.0 kB | 0.5 % |
| Linker alignment padding (no owning object) | 1.6 kB | 0.1 % |

The platform (Wi-Fi, ESP-IDF, crypto, networking) is 74 % of the image; gPlug's own code and web
files are about 10 %.

**Misleading attribution:** `esp_idf_size` reports ~80 kB of `.rodata` in `api_connection.cpp.o`,
which the script books as "String literals". That is the linker's merged string-literal pool (`.rodata.*.str1.4`): string literals from every
object are deduplicated into one section, and the whole section is credited to the first object
that contributed. It is not API code.

## RAM: static

The C3 has 314 kB (321,296 B) of SRAM usable by the app; IRAM and DRAM share it. Table *generated*:

| Part | Size | Share of SRAM |
|---|---|---|
| IRAM code (interrupts, flash driver, scheduler, Wi-Fi) | 59.3 kB | 18.9 % |
| `GplugSmi` object (`gplug_smi__gplug_smi_gplugsmi_id__pstorage`) | 24.6 kB | 7.8 % |
| Wi-Fi globals (connection manager, power management, WPA state) | 14.8 kB | 4.7 % |
| ESP-IDF globals (scheduler lists, ISR stack, stdio, driver state) | 11.4 kB | 3.6 % |
| ESPHome loop task stack (`esphome::loop_task_stack`) | 8.0 kB | 2.5 % |
| lwIP and mDNS (DNS table, mDNS task stack) | 4.6 kB | 1.5 % |
| Other component objects (logger, remaining ESPHome components) | 4.2 kB | 1.3 % |
| **Static total** | **126.8 kB** | **40.4 %** |
| **Left for the heap at boot** | **187.0 kB** | **59.6 %** |


The 2026-09-12 table had a separate 0.4 kB row for the event log blob and its mutex. No static symbol
of that name is in the 2026-09-13 ELF, so it is counted wherever the linker put it (possibly inside
the `GplugSmi` object); not traced further.

### Inside the 24.6 kB `GplugSmi` object

Hand-itemised from the source on 2026-09-11, when the object was 19.9 kB; the 0.6 kB since then
is not broken down (0.1 kB of it, 2026-09-14, the Home Assistant entity pointers and sent-state flags).
The 3.4 kB added on 2026-09-14 for heap monitoring is itemised below, as is the 0.5 kB URL buffer
that moved off the httpd task stack the same day.

| Member | Size |
|---|---|
| `frames_`: Datenstrom frame log, `FRAME_LOG_LEN` 5 x (`FRAME_LOG_RAW_CAP` 1280 + `FRAME_LOG_PLAIN_CAP` 768) | 10.0 kB |
| `ring_`: 360 `Sample`s x 10 B, 1 h at 10 s | 3.5 kB |
| `mem_ring_`: 288 `gplug_mem::Sample`s x 12 B, 24 h of heap figures at 5 min | 3.4 kB |
| `desc_`: meter descriptor, 48 OBIS slots | ~3.0 kB |
| `dsmr_`: DSMR telegram buffer | 2.0 kB |
| `values_`, `have_`, `qh_values_`, `smid_` | 0.5 kB |
| `url_buf_`: request URL copy, 513 B, a member because httpd serves from one task | 0.5 kB |
| decoders, history store, remaining state | 0.9 kB |

`upd_snap_`, the release-check copy for the httpd task (fixed char buffers, 2026-09-25) adds 0.2 kB.

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
| TLS session, only during an update check or download: `MBEDTLS_SSL_IN_CONTENT_LEN` 16 kB + `OUT` 4 kB, plus handshake and certificate parsing | ~30-40 kB; after an HTTPS manifest check the minimum since boot was 106 kB with 159 kB free (gPlugK 2026-09-25), so at most ~53 kB in use at the peak, boot and Wi-Fi join included |
| `update_task` stack, only while a manifest check runs (ESPHome's `xTaskCreate(..., 8192, ...)`) | 8 kB |
| Manifest body during a check (`content_length`, ~0.5 kB) | <1 kB |

Last device reading: **197 kB free heap** with Wi-Fi joined and the SPA loaded (2026-09-10, older
962 kB build, via `/api/status`). That is more than the 193 kB the linker leaves (188.0 kB on 2026-09-14) because the runtime
heap also gets RAM the bootloader releases after startup, so the two figures don't subtract. Target
in `DESIGN.md`: >= 80 kB free after 24 h. A current reading still has to be taken on a device.
During an update check over TLS (gPlugK 2026-09-25): 159 kB free, minimum since boot 106 kB,
largest block 112 kB, httpd stack 1328 B unused (unchanged). A download over TLS is still to be
read on a device; it runs on the loop task, whose stack mark during a download is not measured yet.
Since 2026-09-14 the device keeps its own 24 h trend of free heap, minimum and largest block
(`/api/heap`, Memory card on the Setup tab), so that reading is a screenshot rather than a polling
session.
