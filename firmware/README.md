# gPlug ESPHome firmware

Status: **PoC**. One image for gPlugD / D-E / K / M (ESP32-C3, 4 MB). Pins and meter descriptor are
runtime data written by the SPA setup wizard and stored in NVS.

```
esphome config gplug.yaml            # validate
esphome compile gplug.yaml           # build (needs ../spa/dist/index.html.gz → run `npm run build` in ../spa first)
./sizes.sh                           # build base + gplug, print flash/RAM
cd test && for t in dsmr aes dlms replay raw structure capturelist; do clang++ -std=c++17 -I../components/gplug_smi test_$t.cpp -o test_$t && ./test_$t; done
```

**Flashing**: `esphome run gplug.yaml` / `esphome upload gplug.yaml` fail with `Detected overlap at
address: 0x10000` on this project's custom partition layout. ESPHome's `upload_using_esptool`
(`esphome/__main__.py`) hardcodes the ESP32 app image offset to `0x10000` for every ESP32 target
regardless of the actual partition table; this project's app0 starts at `0x20000` (`partitions.csv`),
so the upload step writes to the wrong address. Confirmed a real ESPHome bug, not a mistake here: the
bootloader's own partition-table dump on boot matches `partitions.csv` exactly, and the `flash_args`
file the same build step generates lists the correct `0x20000` offset.

Workaround — flash each real partition file at its own offset from `flasher_args.json`, **not**
`firmware.factory.bin`. That merged image is a single contiguous blob from `0x0` through the end of
`app0`; it has no entry for `nvs` (in the middle of that span) because nvs isn't a flashable file, so
the merge silently pads the gap with erased bytes. Writing it as one blob therefore **wipes the nvs
partition — the saved WiFi credentials and the SPA-configured hw/meter JSON — on every single flash**,
even a plain firmware update with no `partitions.csv` change. Found 2026-09-10 after it silently erased
a real device's just-completed onboarding on what should have been a routine reflash; the earlier
version of this doc recommended `firmware.factory.bin` directly and was wrong.

```
cd .esphome/build/gplug/.pioenvs/gplug
esptool --chip esp32c3 --port /dev/cu.usbmodemXXXX --baud 460800 write-flash \
  --flash_mode dio --flash_freq 80m --flash_size 4MB \
  0x0 bootloader.bin \
  0x8000 partitions.bin \
  0x19000 ota_data_initial.bin \
  0x20000 firmware.bin
esphome logs gplug.yaml --device /dev/cu.usbmodemXXXX   # watch boot
```

Verified 2026-09-10: clean boot on a real gPlugK, correct partition table, `gplug_smi` initializes,
and — unlike the merged-image method — nvs contents survive the flash.

**The other "config vanished" cause on 2026-09-10 was the AP button itself.** Its first
implementation called `nvs_flash_erase()`, which wipes the **entire** nvs partition -- every namespace,
so `gplug_smi`'s `hw`/`meter` JSON went with the WiFi credentials on every press. While the LED work was
being tested by holding that button, that read as "config keeps disappearing for no reason". An earlier
revision of this paragraph blamed ESPHome's `ESP32Preferences::open()` self-erase (it does call
`nvs_flash_erase()` if `nvs_open("esphome")` fails) and grew `nvs` from 24 kB to 64 kB for headroom; that
theory was wrong for these incidents. The larger partition is kept -- it costs nothing, using the gap
that was already unused between `phy_init` and `app0`, and `app0`/`app1`/`data` offsets are unchanged.

**Reset into AP mode**: hold the hardware AP button >= 3 s (wired to GND, `hw.pins.button` in the
stored hw config) — erases ESPHome's `esphome` NVS namespace (where `WiFiComponent` keeps its saved STA
credentials; `esp32/preferences.cpp`) and reboots. The `gplug` namespace -- hw pins, meter descriptor --
survives, so the device comes back up broadcasting its setup AP **with the LED still driven** (blue
blinking). Equivalent manual fallback (e.g. no button wired, or NVS is corrupt) -- note this one wipes
everything, hw/meter included:

```
esptool --chip esp32c3 --port /dev/cu.usbmodemXXXX erase_region 0x9000 0x10000   # whole nvs partition
```

| File | Purpose |
|---|---|
| `partitions.csv` | nvs 64 kB, otadata 8 kB, phy 4 kB, app0/app1 1664 kB each (OTA), data 640 kB (history, unused yet) |
| `base.yaml` | skeleton without the component, for size reference |
| `gplug.yaml` | real config: wifi AP + captive portal, api, ota, sntp, uart, status LED, `gplug_smi` |
| `components/gplug_smi/` | external component (see below) |
| `components/captive_portal/` | forked+re-styled external component, shadows ESPHome's built-in one (see below) |
| `test/test_dsmr.cpp` | host unit test for the DSMR parser |
| `test/test_structure.cpp` | tests `decode_structure()` (the production DLMS decode path) against a real capture |
| `test/test_capturelist.cpp` | tests `find_capture_list()` (gPlugM/L+G capture-list decode) against two real captures |

## Component `gplug_smi`

- `__init__.py` embeds `../spa/dist/index.html.gz` and a gzipped, minified `presets.json` as const arrays.
- `dsmr_parser.h` – DSMR/P1 ASCII telegram parser with CRC16, no ESPHome deps (host-testable). CRC16
  cross-checked bit-for-bit against `esphome/dsmr_parser` (the real library behind ESPHome's stock
  `dsmr:` component, used by [haribert/gplug-esphome](https://github.com/haribert/gplug-esphome) for
  gPlugE): identical. Known gap vs. that library: no support for a value spanning multiple physical
  telegram lines (e.g. the power-failure log, `1-0:99.97.0`) — inactive today, no current preset uses
  such a field; see the 2026-09-10 finding in `intent/intent.md`.
- `aes_gcm.h` – AES-128-GCM. On the firmware (`ESP_PLATFORM`) this is a thin wrapper around ESP-IDF's
  `mbedtls_gcm_*`, matching [esphome-gplugk](https://github.com/jluthiger/esphome-gplugk)'s approach exactly
  (its `gplugk.cpp` was used as the reference). On host (running `test/*.cpp` on a dev machine) it falls
  back to a small from-scratch AES-128-GCM, since linking ESP-IDF's real mbedtls standalone is impractical;
  that fallback is verified against NIST vectors (`test/test_aes.cpp`) and real device output
  (`test/test_raw.cpp`), not a guess, but it is not what ships.
- `CMakeLists.txt.in` – copied to `<build>/src/CMakeLists.txt` by `__init__.py`
  (`esp32.add_extra_build_file`). Without this, ESPHome's generated project on this
  platformio-espressif32 (pioarduino) build puts every component into one flat "src" pseudo-component
  with no `REQUIRES`, and `mbedtls_gcm_*` comes back as an undefined reference at link time even though
  ESP-IDF already builds `libmbedcrypto.a` for WiFi/TLS elsewhere. Confirmed via `nm` that the symbols
  exist in the already-built archive; this was purely a missing dependency declaration, not a config or
  compile problem. Fix has no effect on the separate bootloader sub-build (verified).
- `dlms_decoder.h` – HDLC (length-driven, FCS/HCS) → LLC → GBT segments / split APDUs → general-glo-ciphering
  decrypt → plaintext data-notification. Two decode paths:
  - `decode_structure()` — what actually ships (`gplug_smi.cpp`'s `on_dlms_apdu_`): strips the fixed
    18-byte data-notification header, then walks the COSEM STRUCTURE once (optional name, then N pairs
    of `(obis(6B), typed value)`), matching esphome-gplugk's `decode_cosem_` algorithm exactly. Single
    pass, no risk of a byte pattern matching somewhere it shouldn't.
  - `find()` — the earlier Tasmota-compatible `pm(C.D.E)` substring search, kept as a fallback for
    buffers `decode_structure()` doesn't recognise (used directly by `test_dlms.cpp`/`test_replay.cpp`/
    `test_raw.cpp`; `test_structure.cpp` exercises `decode_structure()` instead, against the real
    capture, since that's what production actually runs).
  Tag verified only when an authentication key is configured; otherwise plaintext sanity check (first
  byte 0x0F) drives `key_invalid`.
- `gplug_smi.{h,cpp}` – component:
  - loads `hw` and `meter` JSON blobs from NVS namespace `gplug` at boot, applies baud + RX pin to the UART
  - drives the hardware `button` pin (from `hw.pins.button`, active low, internal pull-up): held >= 3 s
    erases ESPHome's `esphome` NVS namespace (`nvs_erase_all` on it -- that's where `WiFiComponent`'s
    saved credentials live) and hard-reboots (`esp_restart()`); the `gplug` namespace with hw/meter is
    kept, so the LED pins are still known after the reboot (see "Reset into AP mode" below). Not
    `nvs_flash_erase()`: that took hw/meter with it, leaving the LED undriven and stuck on whatever
    GPIO state it was in. Deliberately
    not `wifi::save_wifi_sta("", "")`: that call doesn't clear credentials, it *saves* an empty-SSID
    STA entry, which `WiFiComponent` reloads and retries forever on every subsequent boot too
    (persisted in its own flash preference, independent of the `gplug` NVS namespace), producing an
    endless "No matching network found" / "Restarting adapter" loop that starves the AP instead of
    leaving it stable. Found and fixed 2026-09-10 after a real device got stuck in exactly that loop.
  - decodes DSMR telegrams and DLMS push APDUs into the descriptor's named values
  - applies Tasmota serial flags (`so2`: bit 2 invert RX, bit 3 no pullup) and even parity for mode `rE1`
  - keeps a 360-sample ring (10 s, W: Pi, Po, L1–L3) in RAM
  - registers an `AsyncWebHandler` on the shared web server. Defers to ESPHome's own stock
    `captive_portal` while it's active (AP-fallback / fresh device with no saved WiFi): phones then get
    that simple, framework-free, proven WiFi picker inside the OS's restricted captive-portal webview
    (iOS Captive Network Assistant / Android's equivalent), which is known to handle a full JS SPA
    poorly. Once real WiFi is joined, this SPA resumes owning `GET /` for hardware/meter setup and
    live/history viewing, in a normal, unrestricted mobile browser tab. See the 2026-09-10 finding in
    `intent/intent.md`.

## Component `captive_portal` (fork)

ESPHome's built-in `captive_portal` component (the page phones actually see during AP-fallback,
per the deferral above) has no config-level way to customize its appearance — its page
(`captive_index.h`) is a vendored, pre-gzipped byte array, and its `CONFIG_SCHEMA` only exposes
`compression: gzip|br`. So this project forks the whole component under `components/captive_portal/`
(same `external_components` mechanism as `gplug_smi`, which fully shadows the built-in one — ESPHome
picks up an external component of the same name in preference to its own) to give that page gPlug's
branding, based on esphome 2026.6.5:

- `dns_server_esp32_idf.{h,cpp}` – copied byte-for-byte from upstream, untouched.
- `__init__.py`, `captive_portal.{h,cpp}` – forked with one functional change: the page byte
  array is no longer the vendored `captive_index.h`, it's loaded from a real, editable
  `captive.html` at codegen time via the same `_gz_bytes()`/`static_const_array()`/setter pattern
  `gplug_smi/__init__.py` already uses for the SPA (`set_index()` on `CaptivePortal`, mirroring
  `set_spa()` on `GplugSmi`). Everything else — namespace, class name, `global_captive_portal`,
  `is_active()`, the `/config.json` and `/wifisave` handlers `gplug_smi` and phones both depend
  on — is byte-identical to upstream. `compression: br` is no longer accepted (the runtime source
  is always gzip via `_gz_bytes()`); this is a deliberate narrowing, not an oversight.
- `captive.html` – the actual branding: same markup/JS/form-field contract as upstream's page
  (dynamic title/MAC/network-list from `/config.json`, `#ssid`/`#psk` fields posting to
  `/wifisave`, `/update` OTA form), only the `<style>` block and viewport/color-scheme meta
  changed, using the SPA's tokens (`spa/src/style.css`): `--bg:#1f1f1f`, `--panel:#2a2a2a`,
  `--accent:#ffd400` buttons, same border-radius scale (8/10/12px) and font stack. This is the
  file to edit for future branding tweaks — no rebuild-time script needed, ESPHome
  gzip-compresses it automatically via `to_code()`.

Since this is a fork of an actively-evolving core component (not a frozen vendored copy), re-diff
`captive_portal.{h,cpp}`/`dns_server_esp32_idf.{h,cpp}` against the newly-installed esphome
package before every ESPHome version bump, to pick up any upstream fixes (the diff today is small
by design: one setter, one call-site swap).

### HTTP API

| Method | Path | Notes |
|---|---|---|
| GET | `/` and any non-`/api` path | SPA (gzip) |
| GET | `/api/status` | version, hostname, uptime, heap, wifi, hardware, meter counters |
| GET | `/api/live` | `{age, key_invalid, smid, p (kW net), pi, po (W), ei, eo (kWh), values{name:value}}` |
| GET | `/api/ring` | `{period:10, samples:[[pi,po,p1,p2,p3],…]}` |
| GET | `/api/wifi/scan` | last scan results kept by the wifi component (no active scan trigger yet) |
| GET | `/api/presets` | embedded presets (gzip) |
| GET/POST | `/api/config/hardware` | `{variant, pins:{rx,red,green,blue,button}}` |
| POST | `/api/config/meter` | `{preset, key?, auth_key?, descriptor:{protocol, mode, baud, rx, serial_flags?, buffer?, obis[]}}` |
| POST | `/api/config/wifi` | `{ssid, psk}` → `save_wifi_sta` |
| POST | `/api/reboot` | |

Only GET and POST exist: ESPHome's ESP-IDF HTTP shim registers no PUT. JSON bodies are read from the socket by
the handler itself because the shim only pre-reads `x-www-form-urlencoded`.

### Reference: github.com/jluthiger/esphome-gplugk

This project's crypto and DLMS-value-decode logic (see `dlms_decoder.h` above) was rewritten to match
the user's own published, MIT-licensed, hardware-proven ESPHome component for gPlugK, once discovered
partway through this project (local checkout:
`/Users/juerg.luthiger/projects-dev/gplug/esphome.root/dlms-gplugk/gplugk`). That component uses
compile-time YAML sensor declarations (standard ESPHome model), not this project's runtime JSON
descriptor + SPA wizard. Per the user's decision (2026-09-10), the runtime-descriptor/SPA approach here
remains a prototype; a future pass should reconcile the two rather than duplicate protocol work, with
the SPA's role narrowing to WiFi onboarding, live view and history rather than meter-protocol selection.

### Known gaps (PoC)

- Firmware is 962 kB flash / 51 kB static RAM (up from 890 kB before adopting mbedtls — a real crypto
  library is bigger than the hand-rolled one it replaced; still well under the 1664 kB app-slot budget).
- Full pipeline (HDLC framing, AES-128-GCM decrypt, OBIS decode) validated end-to-end against **real**
  gPlugK captures (`test/captures/`, gitignored, not committed — raw frames + device key):
  `test_raw.cpp` decrypts 3 consecutive live frames with the real key, checks frame-counter continuity,
  and cross-checks SM-ID/voltages/energy counters against the matching MQTT payloads (exact on static
  values, single-digit-Wh drift on the slowly increasing ones). A negative control (same frame, corrupted
  key) confirms the auth/sanity check actually rejects a wrong key. `test_replay.cpp` additionally checks
  the plaintext-decode step alone against an earlier already-decrypted capture.
- **gPlugM/L+G "capture-list" push format: SOLVED.** This is gPlugM's entire official meter
  compatibility (L+G E450/E570), so this closes the biggest real gap in the project, not an edge case.
  `DlmsDecoder::find_capture_list()` locates the `02 N 01 N` descriptor-array anchor, ranks each
  descriptor by first-occurrence of its 6-byte OBIS (duplicates reuse the earlier rank), and reads
  that rank's entry from the trailing untagged value array. Verified against two independent real
  captures — 12 cross-checked fields total, including exact byte-offset matches to the device's own
  "GEAG pattern matched at pos N" debug log (`test_capturelist.cpp`). Wired into
  `GplugSmi::on_dlms_apdu_` as the fallback when the flat structural walk (`decode_structure()`) finds
  nothing. Two earlier hand-derived hypotheses were tried, looked disproved, and were reverted — the
  real bug was in *how I was checking* (raw loop index instead of a proper dedup rank), not in the
  underlying theory; see the two 2026-09-10 entries in `intent/intent.md` for the full trail.
  `dbg_structure.cpp <hex files...>` remains for dumping any future capture's descriptors/values with
  byte offsets.
- LEDs: only ESPHome `status_led` on GPIO7; RGB behaviour from the descriptor not wired yet.
- No 15-min history store yet (`data` partition unused).
- WiFi scan endpoint returns whatever the wifi component last scanned; may be empty right after boot.
- Config writes are applied immediately from the HTTP task; UART reconfiguration is not yet deferred to the main loop.
