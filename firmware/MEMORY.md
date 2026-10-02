# Flash and RAM usage

What fits, what is left, and what to do when a change moves either. The summary is the part worth
reading before a change; the collapsed sections below hold the tables it is drawn from and the
history of how the image got here.

Snapshot of the `dev.yaml` build from 2026-09-30 (ESPHome 2026.6.5, ESP-IDF 5.5.4, ESP32-C3,
4 MB flash), with the Home Assistant entities, the update-from-release components and MQTT. Sizes
in kB = 1024 bytes. Numbers come from the linker map, not from a running device, except where noted.

<!-- size-baseline image=1249850 dram=130510 gplug_smi_obj=25304 -->

## The four budgets

| Budget | Used | Limit | Left |
|---|---|---|---|
| App image in the `app0` slot | 1221 kB (86.7 %) | 1408 kB | 187 kB |
| Static RAM (IRAM + DRAM at link time) | 127.5 kB (40.6 %) | 314 kB of SRAM | 186.3 kB for the heap at boot |
| Free heap on a device | 197 kB idle, 83-101 kB at the worst minimum | >= 80 kB after 24 h (`DESIGN.md`) | the TLS update check is the peak, ~44 kB |
| SPA, gzipped | 58.5 kB | 64 kB | 5.5 kB |

The history partition (704 kB) holds ~374 days of 15-min records; every other partition is sized by
ESPHome, see the partition table below.

## What costs what

Of the 1221 kB image, 74 % is platform that no gPlug change touches (Wi-Fi 298 kB, ESP-IDF 242 kB,
crypto 210 kB, networking 140 kB). gPlug's own share is about 142 kB: 72.9 kB `gplug_smi` code and
69.3 kB embedded web files, of which the SPA alone is 58.5 kB. The remaining 96 kB of string
literals belong to all of the above together, which the by-owner section explains under
"Misleading attribution".

Static RAM is dominated by IRAM code (59.6 kB, platform) and the one `GplugSmi` object (24.7 kB),
half of which is the Datenstrom frame log.

## If memory gets tight

- **Image**: the SPA and the HTTP layer are the only large parts that are ours to shrink -- the SPA
  58.5 kB, the HTTP API 21.1 kB and the ArduinoJson it pulls in for the POST bodies 17.8 kB.
  Meter decoding, the reason the firmware exists, is 5.4 kB. Switching a whole feature off in the
  YAML (MQTT, update-from-release) frees far more than tuning code: the release install cost 128 kB.
- **Static RAM**: `FRAME_LOG_LEN` and `FRAME_LOG_RAW_CAP` in `gplug_smi.h` are the first dials; the
  frame log is 10.0 kB of the 24.7 kB object.
- **Heap**: the peak is an update check over TLS (~44 kB with `CONFIG_MBEDTLS_DYNAMIC_BUFFER`,
  ~62 kB without), not steady-state operation.

## Keeping this file current

The three tables marked *generated* are the output of `tools/size_report.py`, which reads the linker
map and the ELF symbols of the last build. Paste them, with the baseline line above, after a build
that changes the image; `tools/size_report.py --check` fails once the build has drifted more than
1 kB (image) or 0.5 kB (static RAM, `GplugSmi` object) from that line, and a Claude Code hook runs
it after every `esphome compile`. Snapshots up to 2026-09-12 were grouped by hand from
`esp_idf_size` output, so their row values are not comparable with the generated ones; the image
and static-RAM totals are.

```
tools/size_report.py                     # tables + baseline line for this file
tools/size_report.py --check             # does this file still match the build?
B=.esphome/build/gplug/.pioenvs/gplug
~/.platformio/penv/bin/python -m esp_idf_size --files $B/firmware.map      # per object file, for digging
python <esp-idf>/components/partition_table/gen_esp32part.py $B/partitions.bin   # real partition table
```

## Details

<details>
<summary><b>Flash: partition table</b></summary>

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
| nvs | `0x2D0000` | 448 kB | Wi-Fi credentials, `hw`, `meter` and `mqtt` JSON, ESPHome preferences; mostly empty |
| data | `0x340000` | 704 kB | 15-min history: 176 sectors x 204 records x 20 B, ~374 days |
| (unused) | `0x3F0000` | 64 kB | |

</details>

<details>
<summary><b>Flash: the app image, by owner (*generated*)</b></summary>

By section: 891 kB code run from flash, 258 kB read-only data, 60 kB IRAM code and 11 kB `.data` initial values
(those two are stored in flash *and* occupy RAM).

By owner (*generated*):

| Part | Size | Share of image |
|---|---|---|
| Wi-Fi driver, WPA supplicant, PHY (`libnet80211`, `libpp`, `libwpa_supplicant`, `libphy`) | 298.2 kB | 24.4 % |
| ESP-IDF system (FreeRTOS, libc/printf, HAL, flash + NVS drivers, heap, UART, OTA, HTTP client + esp-tls, MQTT client) | 241.9 kB | 19.8 % |
| Crypto (mbedTLS: AES-GCM, TLS + X.509 + CA bundle for the update check; Noise/Ed25519 for the encrypted API) | 209.5 kB | 17.2 % |
| Networking (lwIP, ESP-IDF HTTP server + parser, mDNS) | 140.3 kB | 11.5 % |
| String literals from all code | 95.9 kB | 7.9 % |
| ESPHome core and components (incl. the captive_portal fork) | 84.8 kB | 6.9 % |
| `gplug_smi` code | 72.9 kB | 6.0 % |
| Embedded web files, gzipped except the PNG: SPA 58.5 kB, captive page 4.9 kB, icon 3.6 kB, presets 1.6 kB, manifest 0.2 kB | 69.3 kB | 5.7 % |
| ESPHome-generated `main.cpp` setup code | 6.1 kB | 0.5 % |
| Linker alignment padding (no owning object) | 1.7 kB | 0.1 % |

The platform (Wi-Fi, ESP-IDF, crypto, networking) is 74 % of the image; gPlug's own code and web
files, `gplug_smi` and the embedded web files together, about 12 %.

### Inside the `gplug_smi` row (*generated*)

`esp_idf_size` stops at the object file, and the component is two of them (`gplug_smi.cpp`, with
every header-only decoder, store and route handler inlined into it, and `mqtt.cpp`), so the row
above is one lump. This table splits it by the source file each function was compiled from, taken
from the linker map's input sections and the DWARF line table; the rules are `GPLUG_PARTS` in
`tools/size_report.py`, and a section the rules do not name lands in "Everything else".

Inside `gplug_smi`: 80,800 B = 78.9 kB, of which 6.7 kB string literals that the image table counts in its "String literals" row, not in "`gplug_smi` code" (72.9 kB).

| Part of `gplug_smi` | Size | Of it literals | Share |
|---|---|---|---|
| HTTP API: route dispatch, the JSON bodies and the CSV export (`handleRequest`, `json_*`, `handle_*`) | 21.1 kB | 3.1 kB | 26.8 % |
| Everything else: lifecycle, config apply and NVS, Home Assistant entities, LED and button, update check, heap sampler, and the `std::string`/`std::vector` code inlined into them | 18.2 kB | 2.0 kB | 23.1 % |
| ArduinoJson, inlined where it parses the POST bodies (`/api/meter`, `/api/hw`, `/api/mqtt`, `/api/key/check`, `/api/wifi/scan`) and re-serializes the stored meter and MQTT settings | 17.8 kB | 0.0 kB | 22.6 % |
| MQTT: client, template compiler and renderer, status payload (`mqtt.cpp`, `mqtt_template.h`, `mqtt_status.h`) | 9.4 kB | 1.3 kB | 12.0 % |
| History and event store: 15-min records on the `data` partition, CSV rows, event log in NVS | 6.9 kB | 0.3 kB | 8.7 % |
| Meter decoding: HDLC + DLMS/COSEM, DSMR/P1, AES-GCM, protocol sniffing, frame log | 5.4 kB | 0.0 kB | 6.8 % |

Reading it: the HTTP surface is the biggest part of the component, more than three times the meter
decoding it exists for, and ArduinoJson -- pulled in only to parse the POST bodies and to
re-serialize the stored settings -- is as large as all of MQTT. The "Of it literals" column is the
part of each row that sits in the merged pool, i.e. field names and fixed JSON scaffolding.

**Misleading attribution:** `esp_idf_size` reports ~80 kB of `.rodata` in `api_connection.cpp.o`,
which the script books as "String literals". That is the linker's merged string-literal pool (`.rodata.*.str1.4`): string literals from every
object are deduplicated into one section, and the whole section is credited to the first object
that contributed. It is not API code.

</details>

<details>
<summary><b>Flash: how the image grew (one entry per change)</b></summary>

Newest first; each entry gives the image size it replaced and where the delta went.

Image 1,249,850 B = 1221 kB in a 1408 kB slot: **86.7 % full, 187 kB headroom** (2026-09-30, with
mbedTLS dynamic buffers and the MPI lock created at boot (issue #35): +3.6 kB, of which 3.0 kB
mbedTLS (the per-record buffer paths and freeing the config data after the handshake), 0.3 kB
SPA already on `main` since the last snapshot, 0.2 kB strings and 0.1 kB `gplug_smi` code; static
RAM unchanged. The heap effect is under "RAM: heap at runtime". 1,246,132 B = 86.4 % before,
2026-09-30, with consistent panel widths on phones (issue #31): +1.4 kB, all embedded web files. The SPA grew
57.8 -> 58.2 kB with this change (panel tokens, the MQTT port width, the wrapping status pill) and
had already grown ~0.8 kB since the last snapshot with the collapsible History cards (issues #26,
#27); the captive page 4.7 -> 4.9 kB. No code or RAM change. 1,244,756 B = 86.3 % before, 2026-09-28, with
the MQTT device status, availability topic and last will (issue #17): +5.2 kB, of which 3.0 kB
`gplug_smi` code (mqtt_status.h, the fixed topics, the status sample in the GET), 0.4 kB strings,
0.4 kB ESP-IDF (esp-mqtt's blocking publish and last-will paths, now linked) and 1.1 kB SPA (55.9 -> 57.0 kB, the status section and 7 strings in four languages); static RAM
+0.1 kB (not traced; the object itself grew 16 B). 1,239,566 B = 86.0 % before, 2026-09-27, with
the socket-pool sizing, the open-socket count in the heap sample, `EV_SOCKETS` and `/api/sockets`:
+1.1 kB, of which 0.8 kB `gplug_smi` code, 0.2 kB SPA (55.7 -> 55.9 kB, the event's strings) and
0.2 kB static RAM for the 7 extra lwIP socket slots and 4 TCP control blocks; 1,238,424 B = 85.9 % before, 2026-09-26, with
MQTT publishing and templates (issue #14): +37.6 kB. esp-mqtt in the ESP-IDF row +13.3 kB with its
TLS and WebSocket transports switched off in sdkconfig (+18.3 kB with them, the component's
default), `gplug_smi` code +13.6 kB (mqtt.cpp, the template compiler and renderer, and the
ArduinoJson it instantiates), strings +3.2 kB, and the SPA's MQTT card with its template port and
53 strings in four languages +6.5 kB (49.2 -> 55.7 kB gzipped). 1,200,850 B = 83.3 % before, 2026-09-25, with
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

</details>

<details>
<summary><b>RAM: static, and inside the `GplugSmi` object (*generated*)</b></summary>

The C3 has 314 kB (321,296 B) of SRAM usable by the app; IRAM and DRAM share it. Table *generated*:

| Part | Size | Share of SRAM |
|---|---|---|
| IRAM code (interrupts, flash driver, scheduler, Wi-Fi) | 59.6 kB | 19.0 % |
| `GplugSmi` object (`gplug_smi__gplug_smi_gplugsmi_id__pstorage`) | 24.7 kB | 7.9 % |
| Wi-Fi globals (connection manager, power management, WPA state) | 14.8 kB | 4.7 % |
| ESP-IDF globals (scheduler lists, ISR stack, stdio, driver state) | 11.3 kB | 3.6 % |
| ESPHome loop task stack (`esphome::loop_task_stack`) | 8.0 kB | 2.5 % |
| lwIP and mDNS (DNS table, mDNS task stack) | 4.8 kB | 1.5 % |
| Other component objects (logger, remaining ESPHome components) | 4.2 kB | 1.3 % |
| **Static total** | **127.5 kB** | **40.6 %** |
| **Left for the heap at boot** | **186.3 kB** | **59.4 %** |


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

</details>

<details>
<summary><b>RAM: heap at runtime, and the device readings</b></summary>

These are configured sizes from `sdkconfig.gplug` and the code, not measurements:

| Consumer | Size |
|---|---|
| Wi-Fi static RX buffers, 10 x ~1.6 kB | ~16 kB |
| Wi-Fi dynamic RX / TX buffers, up to 32 each | on demand |
| TCP send / receive window per socket (`CONFIG_LWIP_TCP_SND_BUF_DEFAULT` / `WND`) | 5.6 kB each |
| lwIP socket table (`CONFIG_LWIP_MAX_SOCKETS` 19, was 12) and TCP control blocks (`CONFIG_LWIP_MAX_ACTIVE_TCP` 20, was 16), sized so the pool exceeds what the image can hold open (README "Socket pool", 2026-09-26): +224 B static, +8 B image | ~0.2 kB |
| HAN UART RX buffer (`rx_buffer_size`) | 2 kB |
| DLMS frame and APDU vectors, <= 1280 B each | ~3-5 kB |
| HTTP server task stack | ~4.3 kB |
| Main task stack (`CONFIG_ESP_MAIN_TASK_STACK_SIZE`) | 3.5 kB |
| lwIP TCP/IP task stack | 3 kB |
| System event task stack | 2.3 kB |
| Meter descriptor copy, only during a config save (heap on purpose, too big for the httpd stack) | ~3 kB |
| TLS session, only during an update check or download: `MBEDTLS_SSL_IN_CONTENT_LEN` 16 kB + `OUT` 4 kB, allocated per record with `CONFIG_MBEDTLS_DYNAMIC_BUFFER` (config data freed after the handshake), plus handshake, certificate parsing and the 8 kB `update_task` stack below | whole check ~44 kB at the peak with dynamic buffers, ~62 kB with static ones (gPlugK 2026-09-30, see below) |
| MQTT, only while enabled: esp-mqtt task stack 4 kB, in/out buffers 0.5 + 1 kB, outbox <= 4 kB (`OUTBOX_LIMIT`), `MqttRun` ~7.5 kB (templates compiled against a copy of the profile, 2 kB + 256 B render buffers, the two rendered fixed topics 2 x 256 B; the status message renders into the payload buffer), `MqttStopArg` 0.3 kB, TCP socket | ~17.5 kB + one socket; not measured on a device yet |
| `mqtt_stop` task, only while a client is torn down: 3 kB stack, now also for the blocking "offline" publish (bounded by the 2 s network timeout) | 3 kB, transient |
| `MqttRun` + `MqttSettings` during an MQTT config save (heap, validated on the httpd task) | ~8.5 kB |
| GET `/api/config/mqtt`: `MqttSettings` copy plus the status sample (`STATUS_WORST` + 1 B buffer and the Wi-Fi strings) | ~1.9 kB, transient |
| `mqtt_stop` task, only while an old client is torn down after a reconnecting save: 3 kB stack, and the old client's 4 kB task + buffers until it exits (up to ~10 s) | ~9 kB |
| `update_task` stack, only while a manifest check runs (ESPHome's `xTaskCreate(..., 8192, ...)`) | 8 kB |
| Manifest body during a check (`content_length`, ~0.5 kB) | <1 kB |

Last device reading: **197 kB free heap** with Wi-Fi joined and the SPA loaded (2026-09-10, older
962 kB build, via `/api/status`). That is more than the 193 kB the linker leaves (188.0 kB on 2026-09-14) because the runtime
heap also gets RAM the bootloader releases after startup, so the two figures don't subtract. Target
in `DESIGN.md`: >= 80 kB free after 24 h. A current reading still has to be taken on a device.
During an update check over TLS (gPlugK 2026-09-25): 159 kB free, minimum since boot 106 kB,
largest block 112 kB, httpd stack 1328 B unused (unchanged). A download over TLS worked on the
gPlugK (0.6.0-rc.3 → 0.6.0, 2026-09-25); its heap peak and the loop task's stack mark during it are
not measured yet (both are reset by the reboot that follows).
Update check, issue #35 (gPlugK 2026-09-30, one check ~5 min after boot, ~145 kB free before):
with static mbedTLS buffers the minimum since boot fell 117 -> 83 kB, ~62 kB in use at the peak;
with dynamic buffers 127 -> 101 kB (~44 kB), and three checks in a row kept it at 99-101 kB. The
first check also created ESP-IDF's MPI accelerator lock (an 88 B mutex, never freed) 12-28 kB
into the 116 kB heap region that holds the largest free block, which then read 100-104 kB instead
of 112 kB for the rest of the boot. `gplug_smi` now takes that lock once in `setup()`; after three
checks the largest block stays at 112 kB. "Largest block" is `tlsf_fit_size()`, rounded down to
TLSF's size class (4 kB steps between 64 and 128 kB), and 112 kB is that region's whole free
space, so it cannot read higher.
Since 2026-09-14 the device keeps its own 24 h trend of free heap, minimum and largest block
(`/api/heap`, Memory card on the Device tab), so that reading is a screenshot rather than a polling
session.

</details>
