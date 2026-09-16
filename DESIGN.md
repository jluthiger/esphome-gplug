# Design: Smart Meter Interface in ESPHome

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
  for gPlugK's crypto/decode approach (see [`DECISIONS.md`](DECISIONS.md)). Compile-time YAML sensor declarations, not a
  runtime descriptor.
- **github.com/jluthiger/esphome-dlms-meter** (also local, `dlms_meter/` + `esphome-dlms-meter/`) — a
  fork of a public multi-provider DLMS component, now merged upstream into ESPHome core itself
  (`esphome/components/dlms_meter`, ESPHome 2026.6.5+) delegating decode to a separate managed IDF
  component `esphome/dlms_parser`. Worth knowing about for future work; not adopted this round.
- Neither existing component (esphome-gplugk, ESPHome's own stock `dsmr`/`dlms_meter`) handles the
  gPlugM/L+G "capture list" push format -- this project's own decoder (see the 2026-09-10 SOLVED finding
  in [`DECISIONS.md`](DECISIONS.md)) is the only one of the four that does.
- **github.com/haribert/gplug-esphome** — a community ESPHome config for gPlugE (P1-DSMR) using
  ESPHome's own built-in `dsmr:` component, which delegates parsing/CRC/encryption to a separate,
  actively maintained library `esphome/dsmr_parser` (MIT, header-only, C++20; fetched and cross-checked
  2026-09-10, see [`DECISIONS.md`](DECISIONS.md)). gPlugE is Ethernet-based (WT32-ETH01), not WiFi like D/D-E/K/M.

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
      auto-discovered and exposes all decoded values as sensors. *Partly (2026-09-14): a fixed set of
      power, energy, per-phase and diagnostic entities, not every register a profile reads; see
      [`DECISIONS.md`](DECISIONS.md).*
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
