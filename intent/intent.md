# Intent: Smart Meter Interface in ESPHome

| Field | Value |
|---|---|
| ID | INT-000 |
| Status | draft |
| Owner | juerg.luthiger@fhnw.ch |
| Created | 2026-09-09 |
| Updated | 2026-09-11 |

## Goal

Replace the Tasmota-based firmware on [gPlug](https://gplug.ch/) smart-meter adapters with an
[ESPHome](https://esphome.io/) firmware that offers the same HAN decoding capabilities as the
[Tasmota Smart Meter Interface](https://tasmota.github.io/docs/Smart-Meter-Interface/) for the meters gPlug
supports today, but is usable by non-technical people.

A gPlug must work **standalone**: it hosts a webservice and serves a Single-Page Application (SPA) from the device
that shows live and historical consumption. Integration into [Home Assistant](https://www.home-assistant.io/) is
optional and uses ESPHome's native API.

## Context

- Tasmota is not user-friendly for non-technical people. There is no guided workflow to bring a gPlug into the
  local WLAN, enter the meter key and pick the right meter profile. Today each installation gets a hand-edited
  Tasmota script (see `gplug/<variant>/<provider>/script.txt`), which drifts (label typos already exist in
  `gplugm/romande-energie`) and cannot be maintained by end users.
- ESPHome is actively maintained and integrates natively with Home Assistant.
- Every installation differs by utility (provider): protocol, baud rate, HDLC options, encryption key and
  OBIS set vary. The firmware must therefore be identical everywhere and the meter specifics must be data.
- Memory is limited (ESP32-C3, no PSRAM). Code size and runtime RAM must stay small.

## Scope

### In scope

- Firmware for the gPlug products currently sold on https://gplug.ch/produkte/: **gPlugD, gPlugD-E, gPlugK, gPlugM**.
  All based on ESP32-C3.
- **One firmware image** for all variants. Hardware variant (pins, LEDs) is selected during setup; defaults per variant
  come from the Tasmota scripts.
- Protocol decoders, exactly those used by the existing gPlug scripts:
  - DSMR / P1 ASCII (OBIS text telegrams, Tasmota mode `o`)
  - DLMS/COSEM over HDLC, raw push frames with OBIS lookup (Tasmota modes `r`, `rE1`)
  - AES-GCM decryption of DLMS frames with a GUEK (Tasmota `so4`)
- Meter descriptor (UART params, protocol mode, HDLC options, OBIS → sensor mapping) is **data, not code**:
  loaded at runtime from device storage, editable in the SPA, survives reboot and firmware update.
- Presets: existing Tasmota scripts per variant / provider ship as selectable presets in the SPA
  (gPlugD P1-DSMR, gPlugD P1-HDLC/DLMS, gPlugD-E P1-DSMR, gPlugD-E P1-HDLC/DLMS,
  gPlugK Kamstrup DLMS push, gPlugM Romande Energie, gPlugM universal).
- Webservice on the device: serves the SPA, the live data stream, the persisted history and the configuration API.
- Persistence of 15-min values on flash (see *Data flow and storage*).
- WiFi onboarding: captive portal access point. (Improv BLE dropped: 425 kB flash.)
- Optional MQTT publishing (off by default). Home Assistant via ESPHome native API.
- LED status behaviour on **all** variants: green pulse on frame received, blue blink when no data,
  red blink when no WiFi or decode/key error, short RGB light show at boot.

### Out of scope / Non-goals

- SML, MODBUS, EBus, binary M-Bus and other Tasmota SMI decoders not used by any gPlug variant.
- Re-flashing firmware to change meter/provider or pins. Provider differences are descriptor config only.
- OTA migration from Tasmota to ESPHome. Existing devices are flashed via USB.
- gPlugE (no longer sold) and any hardware not listed on the product page. The L&G E450 gPlug announced as
  "in development" is added once its hardware is final.
- Compatibility with legacy Tasmota installations: no Tasmota MQTT topic scheme, no Tasmota HTTP API, no
  Tasmota partition layout. Firmware updates use ESPHome's own OTA only.
- Cloud services, remote access, VPN.
- Tariff / price calculation. Only raw HT/NT counters are stored and shown.
- More than one meter per device.
- Authentication on the webservice in v1 (trusted LAN only).
- Upstreaming to the ESPHome project. Delivered as an external component in its own repository.

## Constraints

### Hardware (all gPlug variants)

- MCU: **ESP32-C3 v0.4** on every variant. Single-core RISC-V @160 MHz, ~400 kB SRAM, no PSRAM, 2 hardware UARTs.
- Flash: **4096 kB** (DIO), confirmed on gPlugK running Tasmota 15.5.0.
- Reference numbers from Tasmota 15.5.0 on gPlugK (2026-09-09), the baseline the ESPHome build must beat or match:

| Item | Tasmota value |
|---|---|
| Program size | 2128 kB |
| Partitions | safeboot 832 kB, app0 2880 kB, fs 320 kB |
| Free heap at runtime (script + MQTT + web) | 73.5 kB, 29 % fragmented |
| Flash write cycles after 2 days uptime | 252 |

- Skeleton ESPHome 2026.6.5 build (esp-idf, `firmware/`), measured 2026-09-09, app slot 0x1A0000 = 1664 kB:

| Config | Flash | Static RAM | Headroom in slot |
|---|---|---|---|
| base: wifi + AP + captive portal + api + ota + sntp + uart + status LED + web_server v3 | 923 kB (55 %) | 42 kB | 741 kB |
| base + esp32_ble_server + esp32_improv | 1348 kB (81 %) | 55 kB | 316 kB |

| gplug.yaml: base minus web_server v3, plus `gplug_smi` (DSMR decoder, HTTP API, embedded SPA 11.7 kB + presets) | 884 kB (53 %) | 51 kB | 780 kB |

  Static RAM is .data + .bss only; runtime heap (WiFi buffers, web server) must be measured on a device.
- HAN input on UART1 on the configured RX pin. UART0 stays free for logging.
- Variants differ only in pin assignment and HAN electrical interface. Defaults derived from the Tasmota scripts:

| Variant | HAN RX pin | Baud | Red | Green | Blue | Button | Interface |
|---|---|---|---|---|---|---|---|
| gPlugD | 4 | 115200 | 6 | 5 | 7 | 9 | P1 (DSMR ASCII or HDLC/DLMS) |
| gPlugD-E | 4 | 115200 | 5 | 6 | 7 | 9 | P1 (DSMR ASCII or HDLC/DLMS) |
| gPlugK | 4 | 2400 | 5 | 6 | 7 | 9 | Kamstrup DLMS push |
| gPlugM | 7 | 2400 | 1 | 4 | 3 | 9 | CII / M-Bus HDLC/DLMS |

- Pin assignment is a **runtime setting** with per-variant defaults, changeable during setup in the SPA.
- Memory: SPA assets, descriptor parser, 15-min store and web server must fit in ~400 kB SRAM alongside WiFi
  stack, ESPHome core and HA native API. Hard budget numbers TBD.
- Power: Tasmota scripts reduce CPU to 80 MHz, dynamic WiFi TX power, sleep 100. Same power envelope applies
  (device may be powered from the P1 port).

### Technical

- Platform: ESPHome, delivered as an **external component** (`external_components:`) in a dedicated repository.
- Descriptor: JSON, schema-versioned. Fixed maximum number of OBIS entries: 48 (largest preset, gPlugM universal, has 33).
  No unbounded heap use.
- SPA: Preact 10 + htm 3 (no JSX, no framework build), bundled by esbuild into one `index.html`, gzip-embedded in the
  firmware and served at `/`. Setup wizard measured at 11.7 kB gzipped. Budget for the full SPA (live + history):
  ≤ 64 kB gzipped. Source: `spa/`.
- UI: the whole application is **mobile-first** -- both the SPA (`spa/`) and the captive portal
  (`firmware/components/captive_portal/captive.html`). Primary device is a phone, not a desktop browser;
  layout, touch targets and viewport meta must be designed for that first, desktop is secondary.
- Time: NTP is required for timestamps of persisted slots. Slots completed before the first NTP sync are dropped.

### Operational

- Security: no authentication in v1. GUEK is entered once in the SPA, stored in NVS, never displayed again.
- Privacy: no data leaves the device unless MQTT or HA is enabled by the user.
- Reliability: flash wear must allow > 5 years of 96 appends/day.

### Process

- Phase: **proof of concept**. No deadline. Scope may be cut to what proves feasibility on real hardware
  (decoding, storage budget, SPA on device); polish comes after.
- Rollout: new units shipped with ESPHome; existing units re-flashed via USB on request.

### Prior art discovered 2026-09-10

- **github.com/jluthiger/esphome-gplugk** — the user's own published, MIT-licensed, hardware-proven
  ESPHome component for gPlugK (Kamstrup). Real HA dashboard, real device logs. Local checkout:
  `/Users/juerg.luthiger/projects-dev/gplug/esphome.root/dlms-gplugk/gplugk`. This is now the reference
  for gPlugK's crypto/decode approach (see decision log). Compile-time YAML sensor declarations, not a
  runtime descriptor.
- **github.com/jluthiger/esphome-dlms-meter** (also local, `dlms_meter/` + `esphome-dlms-meter/`) — a
  fork of a public multi-provider DLMS component, now merged upstream into ESPHome core itself
  (`esphome/components/dlms_meter`, ESPHome 2026.6.5+) delegating decode to a separate managed IDF
  component `esphome/dlms_parser`. Worth knowing about for future work; not adopted this round.
- Neither existing component (esphome-gplugk, ESPHome's own stock `dsmr`/`dlms_meter`) handles the
  gPlugM/L+G "capture list" push format -- this project's own decoder (see 2026-09-10 SOLVED finding
  below) is the only one of the four that does.
- **github.com/haribert/gplug-esphome** — a community ESPHome config for gPlugE (P1-DSMR) using
  ESPHome's own built-in `dsmr:` component, which delegates parsing/CRC/encryption to a separate,
  actively maintained library `esphome/dsmr_parser` (MIT, header-only, C++20; fetched and cross-checked
  2026-09-10, see decision log). gPlugE is Ethernet-based (WT32-ETH01), not WiFi like D/D-E/K/M.

## Acceptance criteria

Setup
- [ ] Given a factory-fresh gPlug, when powered on, then it opens a captive-portal AP and the user
      can join it to the home WLAN from a phone without any tool installation.
- [ ] Given the SPA setup wizard, when the user selects the hardware variant, then the pin defaults from the table
      above are applied and can be overridden.
- [ ] Given the SPA setup wizard, when the user selects a provider preset and (if required) enters the GUEK,
      then live values appear within 60 s without reboot or reflash.
- [ ] Given a stored configuration, when the device reboots or receives a firmware update, then descriptor,
      pins and key are retained.

Decoding
- [ ] Given a gPlugD on a DSMR P1 meter, when frames arrive, then SM-ID, Pi, Po, P1i–P3i, P1o–P3o, V1–V3,
      I1–I3, Ei, Ei1, Ei2, Eo, Eo1, Eo2 match the values shown by the current Tasmota firmware.
- [ ] Given a gPlugD/D-E on an encrypted HDLC/DLMS P1 meter with a valid GUEK, when frames arrive, then the same
      value set as above is decoded.
- [ ] Given a gPlugK on a Kamstrup push meter at 2400 baud, when frames arrive, then Pi, Po, per-phase P/V/I,
      reactive power/energy and Ei/Eo are decoded.
- [ ] Given a gPlugM on a CII/M-Bus HDLC meter (Romande Energie preset and universal preset), when frames arrive,
      then Pi, Po, per-phase P/V/I, Ei/Eo incl. HT/NT and reactive energy are decoded.
- [ ] Given a wrong GUEK, when frames arrive, then the SPA shows a clear "key invalid" state instead of silence.
- [ ] Given no frames for > 60 s, when the SPA is open, then the freshness badge turns stale and the blue LED blinks.

Live screen
- [ ] Given the SPA is opened, when the page loads, then the last 60 min of Pi/Po/L1–L3 at 10 s resolution are
      shown immediately from the RAM ring buffer, then updated at 0.1 Hz.
- [ ] The live screen shows: device name, integration badges, SM-ID, data age, current net power gauge,
      Bezug and Einspeisung counters, 60-min chart with toggleable Bezug/Einspeisung/L1/L2/L3.

History
- [ ] Every 15 min a record with Ei, Eo, Ei1, Ei2, Eo1, Eo2 and average Pi, Po for the slot is appended to flash.
- [ ] Given 1 year of operation, when the SPA history screen is opened, then a day view (96 slots) and a month
      view (daily aggregates) are available for any day in the last 365 days, aggregation done in the SPA.
- [ ] Given a full ring, when a new slot is written, then the oldest slot is overwritten and no whole-file rewrite
      occurs (measured by flash write count).
- [ ] Given a reboot at any time, then no more than the current open 15-min slot is lost.

Integration
- [ ] Given Home Assistant on the same LAN, when the ESPHome native API is enabled, then the device is
      auto-discovered and exposes all decoded values as sensors.
- [ ] Given MQTT enabled in the SPA, when frames arrive, then values are published at the configured period.

Resources
- [ ] Free heap after 24 h of operation with SPA open ≥ 80 kB, fragmentation < 20 % (Tasmota baseline: 73.5 kB / 29 %).
- [ ] Firmware + SPA fit the chosen partition layout with room for OTA.

## Proposed approach

- Single ESPHome external component `gplug_smi` (name TBD) containing: UART frame reader, DSMR ASCII parser,
  HDLC/DLMS parser with AES-GCM, OBIS mapper driven by the descriptor, 15-min store, web endpoints.
- Descriptor schema covers at least what the Tasmota scripts use today: RX pin/baud/protocol (`o`, `r`, `rE1`),
  serial line flags (`so2`: invert RX, no pullup), frame buffer size (`so3`), GUEK (`so4`), list of {OBIS, scale, name, unit, precision}.
- Hardware profile (variant, pins) and meter descriptor are two separate documents, both in NVS/LittleFS.
- Hot-reload of the descriptor from the SPA without reboot if feasible.

### Data flow and storage

- Meter data arrives at ~0.1–0.2 Hz. Firmware keeps in RAM only:
  - ring buffer of the last **K = 360** live samples at 10 s resolution (60 min), not persisted;
    fields Pi, Po, P1, P2, P3 only (no V/I) ≈ 7 kB
  - the counter values of the last completed 15-min slot for all sources
  - a few bytes of metadata describing the persisted structure on flash
- Every 15 min a small record is **appended** to flash (and possibly a few old records dropped).
  The on-flash structure must make this cheap: proposal is k rotating bucket files, k = 2…10,
  managed round-robin so that appends are sequential and deletes are whole-file.
- Retention: 1 year at 15-min resolution, ring overwrite.
  - Naive record: 8 values × 4 B = 32 B/slot ≈ 3 kB/day ≈ 1.1 MB/year. **Does not fit** a standard ESPHome
    dual-app OTA layout on 4 MB (2 × ~1.8 MB app leaves ~300 kB).
  - Compact record: energy as uint16 Wh deltas per slot (6 × 2 B) + avg Pi/Po as int16 W (2 × 2 B) + 4 B
    timestamp per bucket header = 16 B/slot ≈ 1.5 kB/day ≈ 560 kB/year. Fits if app slots are ≤ 1.7 MB each.
  - Partition layout (decided, `firmware/partitions.csv`): ESPHome standard dual-app OTA. nvs 24 kB, otadata 8 kB,
    phy 4 kB, app0 1664 kB, app1 1664 kB, data (LittleFS) 640 kB. Skeleton without BLE uses 923 kB, leaving
    741 kB for meter decoding, AES, history store and the SPA (~12 kB gz for the wizard).
- Browser/SPA on load reads the persisted history **once** from flash via the webservice,
  then subscribes to live data at f = 0.1 Hz. No repeated history reads.

### Webservice API (sketch)

| Endpoint | Purpose |
|---|---|
| `GET /` | SPA |
| `GET /api/live` (SSE or WebSocket) | 0.1 Hz push: Pi, Po, P1–P3, V1–V3, I1–I3, Ei, Eo, HT/NT, SM-ID, frame age |
| `GET /api/ring` | last 360 live samples |
| `GET /api/history?from&to` | persisted 15-min records |
| `GET/POST /api/config/hardware` | variant, pins |
| `GET/POST /api/config/meter` | descriptor incl. GUEK (write-only) |
| `GET /api/presets` | `{variants, presets}` – bundled presets, generated from the Tasmota scripts by `spa/tools/scripts2presets.py` |
| `GET /api/wifi/scan` | `[{ssid, rssi, secure}]` |
| `POST /api/config/wifi` | `{ssid, psk}`; device joins STA while keeping AP up, SPA polls `/api/status` |
| `GET /api/status` | WiFi, uptime, RSSI, heap, integrations |

### SPA screens

1. **Setup wizard** (implemented in `spa/`, see `spa/README.md`): Welcome → Gerät (variant + pins) →
   Smart Meter (preset + GUEK) → WLAN → Abschluss. WiFi is last so all other settings are saved while the phone
   is still on the captive-portal AP; the final screen shows the new address and first live values.
2. **Live** (reference: existing gPlugM "IoT-Adapter" UI, dark theme, mobile portrait):
   header with device name and integration badges; "Smartmeter <SM-ID>" with freshness badge;
   gauge with current net power (kW, one value: import positive, export negative); Bezug / Einspeisung counters (kWh); chart "Verlauf" last 60 min at 10 s,
   W on y-axis (negative = export), series Bezug (blue), Einspeisung (yellow), L1/L2/L3 toggleable; legend.
3. **History**: day view (96 × 15 min bars) and month view (daily bars), navigation by date, aggregation in SPA.
4. **Settings**: hardware/pins, descriptor (preset picker; raw OBIS table editor under "Advanced"), integrations
   (HA API, MQTT), diagnostics (RSSI, heap, last raw frame hex for support).

## Open questions

- [ ] Storage backend: LittleFS partition vs raw partition with own ring layout.
- [ ] Descriptor validation in SPA vs on device. Error feedback to non-technical user when meter sends nothing?
- [ ] Get a similarly real capture for at least one HDLC/DLMS meter with an authentication key set (so5) to test the tag-verification branch, and for a DSMR/P1 meter to validate that path against real bytes too.

## Risks & mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| Firmware outgrows the 1664 kB app slot | No OTA headroom, history partition must shrink | Skeleton measured at 923 kB (no BLE) / 1348 kB (BLE). Drop BLE; compact 16 B records; replace `web_server` with own minimal HTTP if needed |
| ESP32-C3 RAM exhausted by WiFi + HA API + web server + SPA (Tasmota baseline: 73.5 kB free, 29 % fragmented) | Crashes, reboots | Measure heap early with a skeleton build; SSE instead of WebSocket; compress SPA; cap OBIS entries |
| DLMS/HDLC + AES-GCM parser bugs on real meters | Wrong or no values | Record raw frames from every bench meter; replay tests in CI; compare against Tasmota output |
| Encrypted meters need utility-issued GUEK | User stuck in setup | Wizard explains where to get key; "key invalid" state; link to provider instructions |
| Flash wear from 15-min appends | Device dies after years | Rotating bucket files, sequential appends, LittleFS wear levelling; compute expected cycles |
| Preset drift like the current scripts | Wrong labels/values per provider | Single source of truth for presets, generated, with tests |
| ESPHome upgrades break external component | Maintenance burden | Pin ESPHome version; CI build against pinned and latest |
| No authentication on LAN | Anyone on WLAN reads/changes config | Document; optional auth in v2 |

## Decision log

| Date | Decision | Rationale |
|---|---|---|
| 2026-09-09 | Port only DSMR-ASCII, DLMS/HDLC raw + AES-GCM decoders | Only modes used across all gPlug scripts. Generic Tasmota SMI descriptor engine not needed. |
| 2026-09-09 | Meter descriptor is runtime data, editable in SPA; not compiled in | Every installation differs by provider. Users must not reflash. Existing per-provider scripts become presets. |
| 2026-09-09 | Target MCU is ESP32-C3 only | All variants use it. Memory constraints (no PSRAM, ~400 kB SRAM) fixed and known. |
| 2026-09-09 | Pins runtime-configurable with per-variant defaults; single firmware image for all variants | Setup adapts to board without reflash; one image simplifies OTA and support. |
| 2026-09-09 | RAM holds only live ring buffer (K=360) + last 15-min counters; flash gets small appends every 15 min in rotating buckets; SPA loads history once then live at 0.1 Hz | Minimise RAM and flash wear. |
| 2026-09-09 | Persist Ei, Eo, Ei1, Ei2, Eo1, Eo2, avg Pi, avg Po per 15-min slot; retention 1 year, ring overwrite | Covers counters and load profile; ≈1.1 MB/year fits 4 MB flash. |
| 2026-09-09 | Live screen layout follows existing gPlugM UI | Proven with users; keeps migration familiar. |
| 2026-09-09 | History: day view (96 slots) + month view, aggregation in SPA | Keeps firmware simple; device serves raw slots only. |
| 2026-09-09 | GUEK entered in SPA, stored plaintext in NVS, never shown again | Simplest for non-technical users; LAN trust model in v1. |
| 2026-09-09 | No auth in v1; MQTT optional off by default; HA via native API | Keep v1 small. |
| 2026-09-09 | WiFi onboarding via captive portal AP only; Improv BLE dropped | BLE measured at +425 kB flash / +13 kB static RAM; AP works from any phone. |
| 2026-09-09 | Delivery as ESPHome external component in own repo; no upstream PR; no OTA from Tasmota (USB flash) | Control over release cadence; avoids Tasmota partition compatibility work. |
| 2026-09-09 | SPA stack: Preact + htm, esbuild single-file bundle, no JSX | Tiny runtime (~4 kB), no build-time transform, fits flash/RAM budget. |
| 2026-09-09 | Setup wizard order: device → meter → WiFi → done | Saving config before WiFi switch avoids losing the session when the AP address changes. |
| 2026-09-09 | SPA sends the full descriptor (~3 kB JSON) on `POST /api/config/meter`, not just a preset id | Device stores what it runs; firmware needs no preset knowledge beyond serving the bundled list. |
| 2026-09-09 | Firmware updates via ESPHome's standard OTA (dual app partitions); no safeboot/single-app layout | Zero custom OTA code; ESPHome dashboard and HA update flow work out of the box. History budget must fit the remaining ~620 kB. |
| 2026-09-09 | No compatibility with legacy Tasmota installations (MQTT topics, HTTP API, partitions) | Clean break; existing devices are reflashed via USB anyway. |
| 2026-09-09 | 15-min records stored compact (16 B: Wh deltas + W averages as 16-bit) | 32 B records (1.1 MB/year) do not fit 4 MB flash next to dual-app OTA. |
| 2026-09-09 | Project phase is proof of concept, no deadline | Feasibility first: decoding, storage budget, SPA on device. |
| 2026-09-09 | LEDs on all variants signal data received / error | User-visible health without opening the SPA. |
| 2026-09-09 | Live ring buffer holds P only (Pi, Po, L1–L3); one net-power gauge; presets bundled in firmware only; pre-NTP slots dropped | Keep RAM, UI and firmware simple for PoC. |
| 2026-09-09 | Support only products listed on gplug.ch/produkte: gPlugD, gPlugD-E, gPlugK, gPlugM. gPlugE dropped. | No point maintaining hardware that is not sold. Supersedes the earlier gPlugE preset decision. |
| 2026-09-09 | Skeleton firmware built and measured; dual-app layout with 640 kB data partition confirmed feasible | 923 kB without BLE leaves 741 kB headroom. BLE decision pending. |
| 2026-09-09 | First real firmware (`firmware/gplug.yaml`, component `gplug_smi`) compiles at 884 kB: NVS config, runtime UART reconfig, DSMR parser (host-tested), 360-sample ring, HTTP API, SPA served as captive page. DLMS/AES, LEDs, history store still open. | Own minimal HTTP handler replaces ESPHome `web_server` v3 (smaller and serves the SPA at `/`). |
| 2026-09-09 | HTTP writes use POST, not PUT | ESPHome's ESP-IDF HTTP shim registers only GET and POST. |
| 2026-09-09 | DLMS/HDLC + AES-128-GCM decoder implemented in `gplug_smi` (own header-only AES-GCM, Tasmota-compatible `pm()` lookup); host tests pass (NIST GCM vectors, synthetic HDLC/GCM frames incl. split APDUs). Firmware 890 kB. | Unblocks gPlugK, gPlugM and encrypted P1 presets. Real-meter validation pending. |
| 2026-09-09 | Tasmota `so2` is a serial-line flag (0x0C = invert RX + no pullup on gPlugM), not an HDLC control byte; modelled as `serial_flags` + parity from mode `rE1` | Earlier descriptor sketch was wrong. |
| 2026-09-10 | Fixed: DLMS decoder only stored SM-ID when the OBIS value was an octet string; Kamstrup sends it as a plain uint32. Found via real gPlugK capture (SM-ID 32942200 matched exactly once fixed). | Would have shown a blank SM-ID on gPlugK/Kamstrup-style meters. |
| 2026-09-10 | Real Kamstrup DLMS plaintext capture (post-decryption) added as a replay test (`test_replay.cpp`); SM-ID, voltages, and two energy counters match the matching MQTT payload exactly, others within single-digit-Wh drift | First validation against actual device bytes, not just synthetic frames. AES-GCM/HDLC framing still unverified against a real raw (pre-decryption) capture — none available. |
| 2026-09-10 | Full DLMS pipeline (HDLC + AES-128-GCM + OBIS decode) validated end-to-end against 3 real raw gPlugK captures with the real device key (`test_raw.cpp`): frame counters increment correctly, decoded values match the corresponding MQTT payloads, and a wrong-key negative control is correctly rejected. | Closes the gap noted 2026-09-10: previously only synthetic frames and post-decryption plaintext were tested. This is the strongest evidence yet that the decoder is correct. |
| 2026-09-10 | Fixed: GBT (General-Block-Transfer) segment header was implemented from a guessed variable-length-prefix layout; the real header is fixed 7 bytes (flag, control, sequence u16, ack u16, size u8), confirmed against the AMS parser library source and a real 3-segment gPlugM capture. Also fixed: unencrypted GBT-reassembled payloads never reached a success state (code only handled the encrypted/0xDB branch). | Both were live bugs in code already shipped in the compiled firmware, caught only once a real multi-segment capture was available. |
| 2026-09-10 | **Open finding, not yet resolved — now with two real captures and two falsified hypotheses**: gPlugM/L+G-style meters send a DLMS "capture list" push: a `02 N 01 N` structure whose first element is an array of N `{class, obis(6B), attribute-id, dummy}` descriptor items, followed by further outer-structure elements that are the real, *untagged* readings. Generic OBIS-substring search (the Tasmota-`pm()`-equivalent this project is built on) only ever finds the descriptor's dummy value, never the real one. Two index-mapping hypotheses were tried against real captured bytes and instrumented dumps (`firmware/test/dbg_structure.cpp`, `dump_gplugm.cpp`, `dump_gplugm2.cpp`): (1) skip the attribute-id byte before the value — produced plausible-looking wrong zeros; (2) value index = descriptor index with exact-duplicate descriptors collapsed to one slot — fit the first (14-descriptor) capture perfectly by hand, but a second (18-descriptor) capture proved it wrong: the OBIS 18.2.1 reading (independently confirmed by the user at value 431192) sits at a different index than that rule predicts. Both reverted; current code is back to the flat, hardware-validated search only. | Two independently-derived mapping rules from real data both failed to generalize. Continuing to guess a third from byte inspection alone is not reliable enough for a value shown to a user as their energy reading. Needs either the real "GEAG" decoder's source/logic, or DLMS/COSEM documentation for this exact push-with-capture-list encoding, or many more real samples to triangulate from. |
| 2026-09-10 | **SOLVED**, with two more real captures (`24.Jun.2026.txt`, `23.Jun.2026.txt`). The dedup-rank hypothesis above (2026-09-10, hypothesis 2) was correct all along — its "disproof" was a bug in *my checking*, not in the theory: I used the raw descriptor loop index as the value-array index instead of a proper dedup rank (a counter that only advances on a *newly seen* 6-byte OBIS; repeats of an already-seen OBIS reuse its existing rank). Re-verified against 12 independent data points across two capture shapes (14-element: 11 fields incl. SM-ID string, all exact; 18-element: the water-meter reading, exact value *and* exact byte offset match to the device's own "GEAG pattern matched at pos N" debug log). Implemented as `DlmsDecoder::find_capture_list()`, tried first inside `find()`; wired into `GplugSmi::on_dlms_apdu_` as the fallback when the flat structural walk (`decode_structure()`) finds nothing. New test `test_capturelist.cpp` pins both captures. Firmware compiles clean at 985 kB. | gPlugM's *entire* official meter compatibility (L+G E450/E570, confirmed against gplug.ch/produkte) now decodes correctly, not just frame reassembly. The lesson: when a hypothesis is disproved by evidence, re-derive the evidence with tooling before concluding the hypothesis is wrong — a hand-counting mistake looked exactly like a falsified theory. |
| 2026-09-10 | Adopted github.com/jluthiger/esphome-gplugk as the reference implementation for gPlugK's DLMS decode, per the user's decision. Production crypto now uses ESP-IDF's mbedtls_gcm_* (matching that component exactly), replacing the hand-rolled AES-GCM for the firmware build; a host-only fallback of the same hand-rolled, NIST/hardware-validated AES-GCM remains for `firmware/test/*.cpp` since linking ESP-IDF's real mbedtls standalone on a dev machine is impractical. Value decode switched from Tasmota-style OBIS-substring search to a single-pass structural walk (`decode_structure()`) matching esphome-gplugk's `decode_cosem_`: strip the 18-byte data-notification header, then walk STRUCTURE -> optional name -> N x (obis, value) pairs once. Both changes verified: all 6 host test suites pass, including a new one exercising the actual production algorithm (`test_structure.cpp`) against the real gPlugK capture; firmware compiles and links clean at 961 kB / 51 kB RAM (up from 890 kB, the cost of a real crypto library over a hand-rolled one). | User has a mature, published, hardware-proven ESPHome component for gPlugK; duplicating its protocol work from scratch was the wrong direction once discovered. Scope for this round was narrowed to swapping the crypto/decode approach only, not migrating the whole SPA/webservice project into that repo. |
| 2026-09-10 | SPA's role narrows to WiFi onboarding, live view, and history -- not runtime meter-protocol selection. Provider/meter choice happens by picking the right compile-time YAML config (ESPHome's normal model, as esphome-gplugk already does), not via a descriptor the SPA edits at runtime. | Reconciles the original "no reflash to change meter" goal with esphome-gplugk's proven compile-time-sensor architecture, per the user's decision. The multi-variant/multi-preset runtime descriptor system built in `firmware/components/gplug_smi` this session remains as a prototype/reference, not the adopted direction. |
| 2026-09-10 | Cross-checked `dsmr_parser.h` against github.com/haribert/gplug-esphome (a real gPlugE P1-DSMR config using ESPHome's stock `dsmr:` component) and the actual library behind it, `esphome/dsmr_parser` (fetched from the PlatformIO registry, header-only, C++20, MIT). CRC16/ARC implementation is bit-for-bit identical to the reference library's. Found one real gap: the reference library handles a value spanning multiple physical telegram lines (used by fields like the power-failure event log, `1-0:99.97.0`); `dsmr_parser.h` does not -- a continuation line silently produces no match today rather than corrupting data, since none of the active gPlugD/D-E presets use such a field. Not fixed this round: the gap is inactive, and per the earlier decision that gplug_smi's runtime decode is a prototype (real direction is compile-time YAML, e.g. stock `dsmr:`), investing further in its DSMR internals has low return. | Also confirms haribert's gPlugE (WT32-ETH01, Ethernet, no WiFi) is real hardware still in community use for P1-DSMR, even though it is not on the current gplug.ch/produkte listing. |
| 2026-09-10 | Confirmed: Ethernet-based hardware (gPlugE / WT32-ETH01) stays out of scope, explicitly, not just by omission from the product page. Not pursued even though it is real, community-used hardware for P1-DSMR (haribert/gplug-esphome). | User's direct call: "Ethernet hardware is a no-brainer. don't follow it." No WiFi-vs-Ethernet abstraction needed anywhere in gplug_smi or the SPA. |
| 2026-09-10 | Descriptor UX: preset picker for normal users, raw OBIS table editor under "Advanced" | Non-technical default, power-user escape hatch (vse-aes case). |
| 2026-09-10 | **First real-hardware boot.** Flashed a gPlug over USB via `esptool write-flash 0x0 firmware.factory.bin` and confirmed clean boot over serial: ESP-IDF bootloader reads back the exact partition table from `partitions.csv` (nvs@0x9000, otadata@0xf000, phy@0x11000, app0@0x20000, app1@0x1c0000, data@0x360000), `gplug_smi` initializes ("ready, protocol=0 baud=115200 obis=0" -- no descriptor stored yet, expected on a fresh device), `setup()` completes without error. Found and worked around a real ESPHome bug: `esphome upload`/`esphome run` hardcode the ESP32 app image offset to `0x10000` (`esphome/__main__.py`, `upload_using_esptool`) regardless of the actual partition table, so they fail with "Detected overlap at address: 0x10000" against this project's custom layout (app0 at `0x20000`, to leave room for a bigger nvs/otadata/phy region). Workaround: flash `firmware.factory.bin` directly with `esptool` (ESPHome's own merge step already places every piece at its correct offset from the real partition table; that step is correct, only the multi-file `upload_using_esptool` path is not). | Custom partition layouts are apparently not something ESPHome's own upload path handles for ESP-IDF builds -- this looks like a real upstream bug, not a mistake in `partitions.csv` (verified: the bootloader's own partition-table dump matches the CSV exactly, and `flash_args` generated by the same build lists the correct `0x20000` offset for `gplug.bin`). Not filed upstream this round; worth doing if this keeps coming up. |
| 2026-09-10 | **First real WiFi onboarding, end to end.** User connected a phone to the captive-portal AP "gPlug-Setup" and entered home WiFi credentials through ESPHome's built-in captive portal. Device joined "CBOX24" and the whole webservice came up: `GET /` served the SPA (200, ~12 kB), `GET /api/status` reported live, correct state (`wifi.connected: true`, IP `192.168.0.162`, RSSI -51, 197 kB free heap -- well above the ≥80 kB target). Found via serial log that `esphome logs` at `level: INFO` never shows the assigned IP: ESPHome logs it via `ESP_LOGCONFIG`, one level above `INFO`, so it's silently suppressed at this build's log level. Fastest way to find the device's address in practice: mDNS (`ping gplug.local` / `dscacheutil -q host -a name gplug.local`), which resolved immediately. | First proof the SPA/webservice/WiFi-onboarding chain (not just protocol decode) works on real hardware. Correction to this row's original text: on reflection it likely was NOT ESPHome's stock captive portal the user went through -- see the next entry, which traces the actual registration order in code and finds this SPA's handler wins unconditionally, registering at `setup()` while `captive_portal`'s own handler only registers later, event-driven, when WiFi actually falls back to AP mode. |
| 2026-09-10 | **Defer to ESPHome's stock `captive_portal` while AP-fallback is active.** Traced in `esphome/components/wifi/wifi_component.cpp`: `captive_portal::global_captive_portal->start()` is called from WiFi's *loop*, not `setup()`, only when it actually falls back to AP-only. `gplug_smi`'s handler, registered at `setup()` and claiming every GET unconditionally, therefore always won the race, meaning phones connecting to the fresh-device AP got this SPA -- a client-rendered Preact app -- inside the OS's restricted captive-portal webview (iOS Captive Network Assistant / Android's equivalent), which is known to render JS-heavy pages unreliably. Fixed: `GplugSmi::canHandle()` now returns `false` for every request while `captive_portal::global_captive_portal->is_active()`, letting ESPHome's own simple, framework-free, million-times-proven WiFi picker own that phase; this SPA resumes owning `GET /` once the device actually leaves AP-fallback (real WiFi joined, no restricted webview involved). Added `captive_portal` as an explicit `DEPENDENCIES` entry. Compiles clean at 962 kB / 51 kB RAM; **not yet re-tested on the device** -- it's currently joined to "CBOX24" with saved credentials, so re-entering AP-fallback to verify needs clearing that first. | The user's own request ("improve the captive portal for smartphones") is best served by *not* trying to make a full SPA behave inside a hostile embedded webview, but by handing that narrow job to the tool already built and tested for it, and keeping the rich experience for where it works reliably. |
| 2026-09-10 | **Verified on real hardware, fully automated (no phone needed).** Flashed the updated firmware, confirmed the nvs partition (WiFi + gplug credentials) is wiped as a side effect of the normal `write-flash` erase range, so the device came up fresh with no saved WiFi and entered AP-fallback on its own. Joined "gPlug-Setup" from this Mac (`networksetup -setairportnetwork`), got a lease on the standard ESPHome AP subnet (192.168.4.100), and fetched `http://192.168.4.1/`: 1462 bytes gzipped, decompresses to ESPHome's actual stock captive-portal HTML (network picker posting to `/wifisave`, proper mobile viewport meta tag, ~2.8 kB, no JS framework) -- not this project's SPA. Confirms the 2026-09-10 fix works exactly as intended. | Closes the loop on today's captive-portal work with a real, reproducible, scriptable test (join AP, curl gateway, check response size/content) rather than relying on a phone each time -- worth keeping as a repeatable check. |
| 2026-09-10 | Added a hardware AP-reset button (`hw.pins.button`, active low, internal pull-up): held >= 3 s, `gplug_smi` erases NVS and reboots, same effect as the documented manual esptool workaround but without a cable. Compiled clean (985 kB) and flashed to the real gPlugK. | Physical fallback path to AP-fallback mode without USB/esptool access, for field use. |
| 2026-09-10 | **Bug found and fixed on real hardware**: the AP button's first implementation called `wifi::save_wifi_sta("", "")` to "clear" WiFi credentials, but that call doesn't clear anything -- it *saves* an empty-SSID STA entry, which `WiFiComponent` persists in its own flash preference and reloads on every subsequent boot too, producing an endless "No matching network found" / "Restarting adapter" retry loop that starved the AP instead of leaving it stable (confirmed via serial log on the real device: AP was reported present at `192.168.0.x` but neither `gplug.local` nor its IP responded, no `gPlug-Setup` visible). Fixed by using `nvs_flash_erase()` + `esp_restart()` instead -- the same mechanism as the manual workaround, verified reliable earlier today. Re-flashed and re-verified: AP came up cleanly, served the correct captive page. | `save_wifi_sta()`'s name is misleading for a "clear" use case; ESPHome has no dedicated clear-credentials API. Root-caused from a live serial log, not guessed. |
| 2026-09-10 | White-labelled the captive portal (the page phones see during AP-fallback) to match the SPA's branding, since ESPHome's built-in `captive_portal` has no config-level styling hook (`captive_index.h` is a vendored, pre-gzipped byte array; `CONFIG_SCHEMA` only exposes `compression`). Forked the whole component into `firmware/components/captive_portal/` via the same `external_components` mechanism `gplug_smi` already uses (confirmed by reading ESPHome's `loader.py`: it fully shadows a built-in component of the same name, no partial-merge caveat). The fork's only functional change: the page byte array is loaded from a real, editable `captive.html` at codegen time (same `_gz_bytes()`/`static_const_array` pattern `gplug_smi/__init__.py` already uses for the SPA) instead of the vendored array; `/config.json`, `/wifisave`, `/update`, and all JS/markup behavior are byte-identical to upstream, only the `<style>` block and viewport/color-scheme meta changed to the SPA's tokens (`--bg:#1f1f1f`, `--accent:#ffd400`, matching radii/font). Compiled clean, confirmed the fork (not stock) was actually built (`captive_index.h` absent, `index_gz_` symbol present in generated source), flashed to the real gPlugK, and verified via the same no-phone automated test used earlier: joined `gPlug-Setup`, fetched `http://192.168.4.1/`, confirmed the branded markup/CSS serves instead of stock ESPHome styling. | The WiFi-setup page is the first thing a user sees when unboxing a device; it looked like generic ESPHome before this, undermining the "defer to stock captive_portal for phones" decision's spirit even though it was the right call functionally. Forking a core component carries real upstream-drift risk (flagged in `firmware/README.md`'s new `captive_portal` section): re-diff against the newly-installed esphome package before every version bump. |
| 2026-09-10 | **RGB status LED, per the user's spec**: AP-fallback = blue blinking, setup (WiFi joined, no meter committed) = blue steady, running = green steady, error = red steady. Pins come from the stored hw JSON (`pins.red/green/blue`, gplugK: 5/6/7), same runtime pattern as `button`. Error = wrong DLMS key (`key_invalid_`) or no frame for 60 s after a meter is configured ("no smart meter connected"). Mode is derived every loop from live state, committed only after 300 ms stable, and GPIOs are driven from the committed mode only. Refines the 2026-09-09 "LEDs signal data received / error" decision. | Every state transition verified on the real gPlugK via the `led mode -> …` serial log line, not just by compile. |
| 2026-09-10 | **Every real meter-config POST crashed the device** (Guru Meditation, stack-protection fault in the httpd task): `apply_meter_json_()` built the ~3 kB `Descriptor` (48 OBIS slots) as a stack local on the httpd task's ~4.3 kB stack. Heap-allocated it. This -- not anything in the SPA -- was why the wizard's meter step never completed on hardware; the device rebooted mid-request, the SPA saw a dropped connection. Also caught: `apply_meter_json_` didn't reset `last_frame_ms_` on reconfigure. | Only found by watching the serial log *during* the POST. The SPA's symptom ("signal is aborted without reason") pointed at the network, the real fault was 200 lines away in C++. |
| 2026-09-10 | SPA error handling centralised in `api.js`: browser `AbortError` on timeout, network failure and HTTP 4xx/5xx now surface as three German messages (device not responding / not reachable / device error + the backend's own `{"error":…}` text) instead of raw `"signal is aborted without reason"` or `"POST /api/x: HTTP 400"`. | The device-side crash above was masked for a while by exactly this: the friendly timeout message looked like the whole story. |
| 2026-09-10 | **Flashing `firmware.factory.bin` wipes nvs on every flash.** The merged image spans `0x0`…end-of-app0 as one blob; nvs (`0x9000`–`0xf000`) has no file in it, so the gap is padded with erased bytes and every flash took WiFi + hw/meter with it (row 326 noticed this as a "side effect" without drawing the conclusion; row 323's workaround was therefore wrong). Now flash the four real partition files at their own offsets from `flasher_args.json`; verified with full erase-range output that none touches nvs, and config survives repeated flashes. | The ESPHome `upload` bug (row 323) is still real; the fix for it just mustn't be a bigger bug. |
| 2026-09-10 | `status_led: pin: GPIO7` in `gplug.yaml` was ESPHome's built-in health-blink component driving gplugK's *blue* pin, uncoordinated with `gplug_smi`'s LED state machine on the same pin -- two writers on one GPIO, seen by the user as the LED flickering between colours during a mode transition. Removed. | Predated the RGB feature; a single-colour placeholder that became a conflict the moment the real LED logic arrived. |
| 2026-09-10 | Grew nvs 24 kB -> 64 kB (`partitions.csv`, into the gap already unused before app0; app0/app1/data offsets unchanged). **The theory behind it was wrong**: I attributed recurring "config vanished" incidents to ESPHome's `ESP32Preferences::open()` calling `nvs_flash_erase()` on a failed `nvs_open` (which it does do), assuming the small partition was tripping it. The real cause was the next row. Kept the larger partition as free headroom; README corrected. | Recorded so the partition change isn't later read as evidence for a mechanism that was never demonstrated. |
| 2026-09-10 | **The AP button was the "config keeps vanishing" cause.** Its `nvs_flash_erase()` (rows 327/328) wipes every namespace, so hw pins and the meter descriptor went with the WiFi credentials on each press -- and the LED work was being tested by pressing it. After the reboot the firmware had no LED pins to drive, so the LED just kept whatever state GPIO left it in (red), which is what "red stays / red-blue flashing in AP mode" was. Now the button erases only ESPHome's `esphome` NVS namespace (where `WiFiComponent`'s saved credentials live, `esp32/preferences.cpp`) and reboots; `gplug` survives. Verified: button -> blue blinking, hw/meter intact afterwards. Supersedes rows 327/328. | Hours of chasing flash tooling for a wipe I was triggering myself with the test procedure. The one piece of code that *deliberately* erased NVS should have been the first suspect. |
| 2026-09-10 | Removed the WiFi step from the onboarding wizard (`welcome -> hardware -> meter -> done`). This SPA is only reachable after the device has joined WiFi (the stock portal does the first join, row 325), so "connect gPlug to your home network" there was always a no-op re-confirmation -- the user hit it at `gplug.local`, already connected, and asked why it existed. The reconnect capability stays, as "WLAN ändern" on the Done page, reusing the same component. | A wizard step that can never do what its heading says is worse than no step. |
| 2026-09-10 | Leaving AP-fallback now resets the LED's error state (clears `key_invalid_`, restarts the 60 s no-data timer -- it started at boot and had run out while the user was still on the portal, so the LED went red the instant WiFi came up) and holds the device in setup (blue steady) until the wizard commits the meter or a first frame arrives. AP also now takes priority over error in mode selection. | Verified by the user on hardware: button -> blue blink -> portal -> blue steady -> meter commit -> green -> red after 60 s with nothing on the UART. |
| 2026-09-10 | `/api/live` now carries `no_data` -- the same verdict that turns the LED red -- and the Done page badge uses it. Before, the badge derived state from `age` alone, which stays `null` forever when no frame has ever arrived, so it read "Warte auf Daten vom Zähler…" indefinitely while the LED was red. Badge is a short pill in the row; the actionable hint (check connection / profile / key) is its own line below -- a full sentence in the pill overflowed the phone screen. | Two consumers deriving "is there data" independently will drift; the firmware decides once, the UI displays it. |
| 2026-09-10 | Everything from today pushed to github.com/jluthiger/esphome-gplug (`main`), including the forked `captive_portal` component, which had been untracked -- a fresh clone didn't build without it. | First state of the repo that builds from a clean checkout. |
| 2026-09-10 | **Partition layout moved back to the standard ESP-IDF/ESPHome one** (`nvs` 0x9000/0x5000, `otadata` 0xE000, `app0` at `0x10000`, `app1` 0x1B0000, `data` 0x350000/0xB0000 = 704 kB; `phy_init` dropped, `CONFIG_ESP_PHY_INIT_DATA_IN_PARTITION` is unset so it was never read). `esphome run` / `esphome upload` now work unmodified; the hand-written esptool command in the README is gone. Root cause of the original failure re-verified in ESPHome 2026.6.5 source: `upload_using_esptool` (`esphome/__main__.py:888`) hardcodes `firmware_offset = "0x10000"` for every ESP32 target although PlatformIO's idedata already carries the real `application_offset` (`"0x20000"` for the old layout); upstream `dev` still has the same line. Second, independent trap found on the way: ESPHome regenerates `.esphome/idedata/<name>.json` only when `platformio.ini` changes, so a `partitions.csv` edit leaves the cached bootloader/partition-table/otadata offsets stale -- the cache here still said otadata at `0xf000`, inside the 64 kB nvs, i.e. even a patched ESPHome would have corrupted nvs on the next upload. Alternatives weighed: one-line patch of the installed ESPHome (lost on `brew upgrade`, plus the stale-idedata trap), or a repo `flash.sh` reading `flasher_args.json` (works, but one more thing to maintain and still bypasses the dashboard). Standard layout wins because nothing built so far depends on the old one: `data` is unused, nvs holds three blobs (WiFi creds, `hw`, `meter`) well under 4 kB, no code hardcodes offsets. Cost: one USB reflash of each dev unit with config re-entered. **Verified on the real gPlugK**: `esphome run gplug.yaml` flashes and boots on the new layout, no esptool by hand. Supersedes rows 323 (workaround) and 335 (nvs 24->64 kB); row 333's finding (merged `factory.bin` wipes nvs) is layout-independent and still true. | The 64 kB nvs existed only to support a theory row 335 already retracted, and it was the sole reason for the non-standard app offset. Tooling that "just works" beats a workaround kept alive by a partition choice that no longer has a purpose. Worth filing upstream: use `idedata.raw["extra"]["application_offset"]`, and invalidate idedata on `partitions.csv` mtime too. |
| 2026-09-10 | **"Live-Ansicht öffnen" now actually opens a live view.** It was a dead button: `href="/"` reloaded the SPA, which always started at the wizard's first step, so it silently restarted setup. Added `spa/src/steps/live.js` (current power, import/export counters, per-phase power from `/api/ring`'s latest sample -- `/api/live` itself carries no per-phase field, only the meter-descriptor-dependent `values` map -- hidden on single-phase installs, a 1 h sparkline from the same endpoint, reachable-address + "WLAN ändern" panel moved here from the Done step) and a `#live`/`#setup` hash route in `src/main.js`: first load picks live view if the device already has hardware+meter configured and is on WiFi, wizard otherwise; the wizard's Done step and the live view's "Einrichtung" button cross-link without a page reload. `api.js` gained `ring()`; mock server gained `/api/ring` and the missing `no_data` field on `/api/live` (both endpoints had drifted from the real firmware shape, which spa/README.md's API table also had wrong -- fixed). | Closes the gap intent row 319 described (SPA's role: WiFi onboarding, live view, history) -- live view existed nowhere before this, only a same-page poll on the wizard's last screen that vanished as soon as you left it. |
| 2026-09-10 | **Why the Device Builder never listed the gPlug, and the fix.** Not a network problem: from a Mac on the same WLAN, `dns-sd -B _esphomelib._tcp` showed `gplug` and `gplug.local` resolved. ESPHome's dashboard discovery (`zeroconf.py`, `DashboardImportDiscovery._process_service_info`) is the *adoption* flow and lists a device only if its mDNS TXT carries `package_import_url`, `project_name` and `project_version`; everything else is used just for the online dot of configs the dashboard already owns. Added `esphome: project:` + `dashboard_import:` to `gplug.yaml`. That forced the package to be self-contained, because a remote package's relative paths resolve against the *adopter's* config dir: components now come from `github://jluthiger/esphome-gplug` (git source with `path: firmware/components`), the SPA gz and presets are bundled and committed inside `components/gplug_smi/` (`npm run build` / `npm run presets` write there; `_bundled()` validator defaults to them), and the partition table is inline via ESPHome's `esp32: partitions:` list (its standard IDF layout -- app0 @ 0x10000, app1, nvs 448 kB -- plus our `data` 704 kB appended; app slots 1408 kB, firmware 987 kB). `partitions.csv` deleted. New `dev.yaml` = `packages: !include gplug.yaml` + a local `external_components` entry; verified in source that sources are processed in order and each installs its finder at `sys.meta_path[0]`, so the local one shadows the git clone. Verified: `esphome config dev.yaml` merges both sources in that order; `esphome config gplug.yaml` alone fails on the *old* component from GitHub (`'presets' is a required option`) until this is pushed -- which is the intended proof that the git path is really used. | Discovery was gated on metadata, not connectivity -- reading the discovery code beat guessing at multicast/VLAN issues. The self-contained-package constraint is the real cost of adoption and shapes the repo layout (assets live with the component, not with the SPA); worth it because it makes "flash once, adopt in your own dashboard" work for others without cloning. Adoption end-to-end (the Adopt click) can only be tested after push. |
| 2026-09-10 | **AP button "stopped working" after the first Device Builder adoption -- it hadn't; the adopted firmware had compiled-in credentials.** The builder writes `wifi: ssid/password: !secret` into every adopted YAML, so the button's erase-saved-credentials-and-reboot did exactly its job and the device was back on the compiled-in network 3 s later, AP never shown. Storage mechanics were unchanged between ESPHome 2026.6.5 and the builder's 2026.8.0 (checked `esp32/preferences.cpp`, `wifi_component.cpp` in both). Fix in `gplug_smi`: the button sets a persistent `ignore_sta` flag in the `gplug` NVS namespace; `setup()` (which runs `AFTER_WIFI`, i.e. after WiFi already started with the compiled-in list) then calls `WiFiComponent::clear_sta()` and `disable()`+`enable()` -- the public pair that re-runs `start()`, which with no STA networks goes straight to the fallback AP + captive portal. The flag is never cleared, on purpose: `start()` derives the saved-credentials preference key from `has_sta()`, so portal-saved credentials are only reachable on boots that drop the compiled-in networks too. Verified on the real gPlugK with a temporary build that compiled in one network: button -> AP < 1 s (serial), portal onboarding to a different network, reboot via `/api/reboot` -> reconnects to the portal network. Alternative considered: tell adopters to delete the `wifi:` block -- rejected, the builder re-adds it on every adoption and nobody will remember. | The button is the product's only cable-free way back into setup, so it must win over whatever a foreign dashboard compiles in. The AFTER_WIFI ordering and the has_sta()-dependent preference key were the two things that made the obvious "clear_sta() in setup" wrong on its own; both came out of reading `wifi_component.cpp`, not from the symptom. |
| 2026-09-10 | **Live view became a four-tab app (Live / Verlauf / Datenstrom / Setup)**, merged from an agreed Claude Design canvas ("gPlug OBIS Monitor", variant 1a: dark-green, schutzraumtauglich). `spa/src/steps/live.js` split into `spa/src/live/{index,tabbar,live-tab,hist-tab,stream-tab,setup-tab}.js`; the live route now draws its own chrome (mono status bar, serif screen title, data-freshness pill, fixed bottom nav) and the wizard's brand/title header is wizard-only. Two deliberate deviations from the mockup: its Google Fonts (Caprasimo/Figtree/JetBrains Mono) were dropped in favour of the fallback stacks the mockup itself lists, because the SPA is a self-contained gzipped blob served by a device that often has no WAN during on-site setup and an external `<link>` would silently fail; and the mockup's fabricated 27-register OBIS grid was cut down to the registers a preset actually exposes. German number formatting (`spa/src/fmt.js`: decimal comma, thin-space thousands) added -- the SPA had none. Bundle 13 kB -> 20 kB gz. | The design was already agreed with the user, so the work was translation, not invention; the two deviations are the places where the mockup assumed things the hardware does not provide (a font CDN, registers the meter never sends). Verified by walking all four tabs in a browser against the mock server, not just by compiling. |
| 2026-09-10 | **Datenstrom tab is backed by real captured frames, not mock data** -- the user's explicit call when told the mockup's raw-frame view had no firmware behind it. New `firmware/components/gplug_smi/frame_log.h`: a fixed ring of the last 5 raw DLMS HDLC frames (ciphertext capped at 768 B plus the decrypted plaintext when available, ~7.7 kB static RAM), fed from `loop()` and exposed as `GET /api/frames` + `/api/frames/<i>/raw\|plain` (uppercase hex, 16 B/line, one endpoint serving preview, clipboard copy and download alike). Required one change in `dlms_decoder.h`: `on_frame_()` previously conflated "HDLC FCS/HCS valid" with "decrypted successfully" in a single bool, so a wrong-key frame was indistinguishable from a corrupted one; it now tracks the CRC verdict separately, which is what lets the UI show `CRC OK` on a frame that never decrypted. DLMS-only by construction -- a DSMR device gets an explanatory empty state rather than invented frames. | A debug view that shows fabricated bytes is worse than no debug view: its entire purpose is diagnosing a meter that is not decoding. The CRC-vs-decrypt split is also the honest answer to "is my key wrong or is my wiring wrong", which is the actual question a user has at that moment. |
| 2026-09-11 | **15-minute history now persists to flash**, per the user's storage proposal (RAM holds only the non-persisted 10 s values, the last quarter-hour's meter readings for all sources, and a little metadata; flash gets a few bytes per quarter hour in rotating buckets). The 704 kB `data` partition -- declared since the layout rework and never used -- is now written raw via `esp_partition_*` (no filesystem, no partition-table change), as 176 rotating 4 kB sector buckets of 16 B header + 204 x 20 B records = ~374 days. `history_store.h` is header-only with an injected flash backend, so `test_history.cpp` exercises the shipping code against a fake that models 4 kB erase granularity, 1->0-only programming and mid-byte power cuts; `partition_flash.h` is the only file including `esp_partition.h`. Three flash realities drove the format, each of which a first sketch got wrong: programming only clears bits, so "time unknown" is all-ones rather than zero **and lives in its own word outside the record CRC**, which is what allows records written before NTP synced to be back-dated by a second write with no erase; a record needs its own CRC or a torn write is indistinguishable from data; and erased flash reads `0xFFFFFFFF`, the numeric maximum, so electing the newest sector by sequence alone would crown an interrupted erase and overwrite live history. Mean power is stored, not derived from the counter delta: `values_[]` is `float`, and at ~19 MWh one ULP is 2 Wh, i.e. +-32 W of noise over a quarter hour. Reboots, gaps, meter swaps and dead intervals are flag bits on the regular record, never separate marker records, because the timeline reconstruction depends on exactly one record per interval. `GET /api/history?range=day\|week\|month\|year` downsamples server-side to <=365 points (deltas, not absolutes; ~14 kB worst case) and the Verlauf tab gained Tag/Woche/Monat/Jahr next to the ring's 60 Min, fetched once per range rather than on the 10 s poll. Cost: +656 B static RAM, +16 kB flash. | The user asked for a specific storage shape and it survived scrutiny; what did not survive was my own first record layout, which had no integrity field, encoded "unknown" in a way flash can never fill in later, and derived average power from a float counter delta. Also recorded so the one behaviour change is not a surprise: a sector erase runs with the flash cache disabled and `CONFIG_UART_ISR_IN_IRAM` is off, so nothing drains the UART FIFO for 30-50 ms (it overflows after ~11 ms at 115200 baud) -- the recycle is therefore pre-armed and performed only in the quiet stretch after a frame, once per ~2.6 days. Device-level verification (real rotation, power-cut recovery, a 24 h run) still outstanding; everything above is host tests plus a mock-server browser walkthrough. |
| 2026-09-11 | **Datenstrom now shows DSMR telegrams too** -- the DLMS-only scoping from the row above was wrong, and the user said so: "why [the empty state]? Show the raw data here too." For DSMR the raw data is the P1 telegram itself, ASCII, arguably *more* useful than a DLMS hex dump because it is readable without a key. `DsmrParser` already buffers the whole telegram; it just had no way out, because `parse_()` rewrites that buffer in place (NULs over the separators) and `len_` resets before `feed()` returns. Added `set_raw_callback()`, fired at telegram end -- before parsing, so a capture costs no extra buffer -- and on a bad CRC too, since a failing telegram is exactly the one worth looking at. The two view modes now mean different things per protocol, which the firmware signals with a new `encoding` field: DLMS keeps ciphertext vs decrypted APDU (both hex), DSMR gets hex vs the telegram verbatim, served from the same stored bytes so nothing is kept twice. `FrameLog` gained separate raw/plain caps (1280/768) because the two halves are different problems -- a telegram or a full-size HDLC frame vs. a decrypted APDU, and DSMR has nothing to decrypt; this also fixed DLMS raw frames silently truncating at 768 B on presets whose buffer ceiling is 1280. +2.6 kB static RAM. | I scoped the feature to DLMS because that is what the design mockup showed, and wrote an empty state explaining the absence as if it were a property of the protocol rather than of my implementation. It read as a reasoned limitation, which made it worse than an obvious gap. The parser already had the data one function call away. |
