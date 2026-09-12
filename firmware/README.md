# gPlug ESPHome firmware

Status: **PoC**. One image for gPlugD / D-E / K / M (ESP32-C3, 4 MB). Pins and meter descriptor are
runtime data written by the SPA setup wizard and stored in NVS.

```
esphome config dev.yaml              # validate
esphome compile dev.yaml             # build (embeds components/gplug_smi/spa.html.gz → run `npm run build` in ../spa after SPA changes)
./sizes.sh                           # build base + dev, print flash/RAM
cd test && for t in dsmr aes dlms replay raw structure capturelist framelog history csv eventlog sniff; do clang++ -std=c++17 -I../components/gplug_smi test_$t.cpp -o test_$t && ./test_$t; done
```

**Flashing**: `esphome run dev.yaml` (or `esphome upload dev.yaml --device /dev/cu.usbmodemXXXX`).
Stock tooling, nothing custom. That works because the partition table is ESPHome's own standard
ESP-IDF layout with `app0` at `0x10000` (only the `data` partition is added, inline in `gplug.yaml`): ESPHome's `upload_using_esptool` (`esphome/__main__.py`) hardcodes
that ESP32 app offset for every target instead of reading the `application_offset` PlatformIO already
puts in `.esphome/idedata/<name>.json` (still hardcoded in upstream `dev` as of 2026-09-10). Until
2026-09-10 this project had `app0` at `0x20000` to leave room for a 64 kB nvs, so `esphome run` failed
with `Detected overlap at address: 0x10000` and every flash went through a hand-written esptool
command. The big nvs turned out to be unnecessary (see the AP-button paragraph below), so the layout
was moved back to standard and the workaround dropped. Cost: one USB reflash of each dev unit with
WiFi/hw/meter re-entered. Later the same day the hand-written `partitions.csv` went too, in favour of
ESPHome's generated table plus an inline `data` entry (needed for the self-contained package, see
below): app slots 1408 kB instead of 1664 kB, nvs 448 kB (ESPHome's IDF default), `data` 704 kB.

Two flashing traps that are independent of the layout, both hit on 2026-09-10:

- **Never write `firmware.factory.bin` to `0x0`** (dashboard "factory" download, ESP Web Tools, or
  `esptool write-flash 0x0 firmware.factory.bin`) on a device whose config you want to keep. The merged
  image is one contiguous blob from `0x0` through the end of `app0`; nvs sits in the middle of that span
  with no file of its own, so the merge pads it with erased bytes and **every such flash wipes the saved
  WiFi credentials and the SPA-configured hw/meter JSON**, even a plain firmware update. `esphome run`
  flashes bootloader / partition table / otadata / app as separate files at their own offsets and
  leaves nvs alone. Verified 2026-09-10 on a real gPlugK: config survives repeated `esphome run`.
- **After any partition-table change, delete `.esphome/idedata/gplug.json` before flashing.** ESPHome
  takes the bootloader/partition-table/otadata offsets from that cached idedata and only regenerates it
  when `platformio.ini` changes, which a partition edit doesn't trigger. With a stale cache the otadata
  image is written at its *old* offset -- on 2026-09-10 that was `0xf000`, i.e. straight into the then
  64 kB nvs partition.

```
esphome run dev.yaml --device /dev/cu.usbmodemXXXX            # compile + flash + tail log (never exits: Ctrl+C)
esphome run dev.yaml --device /dev/cu.usbmodemXXXX --no-logs  # same, returns after the flash
esphome upload dev.yaml --device /dev/cu.usbmodemXXXX         # flash only, no compile
esphome logs dev.yaml --device /dev/cu.usbmodemXXXX           # log only
```

**OTA updates** (verified on hardware 2026-09-11, device on LAN). Two paths, both into the
inactive app slot (`app0`/`app1`, 1408 kB each; image ~985 kB), nvs and `data` untouched so WiFi,
hw/meter config and history survive:

```
esphome upload dev.yaml --device <ip|gplug.local>             # native ESPHome OTA, port 3232 (~6 s)
curl -F update=@.esphome/build/gplug/.pioenvs/gplug/firmware.ota.bin http://<ip>/update   # what the SPA's firmware card does (~12 s)
```

For end users the entry point is the SPA: Setup tab → **firmware update** (`spa/src/live/firmware-card.js`).
It checks the file's image header in the browser (ESP32-C3 app image; a `firmware.factory.bin` is
refused, it starts with the bootloader), POSTs it to `/update` (ESPHome's `ota.web_server`, which
`gplug_smi` auto-loads), then polls `/api/status` until a fresh uptime appears and compares its
`build` (compile time) with the one before, so a boot-loop rollback to the old image is reported
rather than shown as success. Measured on the gPlugK: 13 s upload, confirmed new build at 19 s.

**The check compares image identity, not build timestamps** (changed 2026-09-12). `build` is
`App.get_build_time()`, i.e. ESPHome's `ESPHOME_BUILD_TIME`, and ESPHome caches that value in
`.esphome/build/gplug/build_info.json` keyed by the *config hash*. An update that changes only
bundled assets -- the SPA, the presets, the captive page -- leaves the YAML config untouched, so
the timestamp is reused and the new image reports the same `build` as the old one. The card used
to read that as "unchanged build, possibly rolled back" for an update that had in fact succeeded,
which is exactly what flashing the four-language SPA produced: the device served the new 35 kB
bundle and still reported the previous build time. The timestamp is also wrong in the other
direction -- it moves on a rebuild even when the device ends up back on the old image.

`/api/status` therefore also reports `app`, the first 16 hex digits of the running image's
`esp_app_desc_t.app_elf_sha256` (read straight from `esp_app_get_description()`, not through
`esp_app_get_elf_sha256()`, which truncates to `CONFIG_APP_RETRIEVE_LEN_ELF_SHA` -- 9 characters
by default). The same 32 bytes sit at offset `0xB0` of an OTA file, so the firmware card reads
the identity of the file it is about to upload and, after the reboot, compares the two: equal
means this exact image is running. Comparing instead against the identity from before the upload
catches the bootloader rollback. Only a firmware too old to report `app` falls back to the
timestamp. Both verdicts were walked through the real card against the mock
(`MOCK_OTA_ROLLBACK=1` makes it accept an upload and come back on the old image), and the hash a
real gPlugK reports matches the one read out of the flashed `firmware.bin` at `0xB0`.
There is no OTA on the captive-portal page any more (removed 2026-09-11): that page is onboarding
only. A bad image is rejected before the slot
switch (`esp_ota_ops: OTA image has invalid magic byte`); an image that boots but crash-loops is
rolled back by the bootloader (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`, `safe_mode` marks the app
valid after 60 s) -- config-verified only, not provoked on hardware.

**OTA password is optional**: substitution `ota_password` in `gplug.yaml`, default `""` = no auth,
anyone on the LAN can flash. Non-empty protects both paths with the same password: native OTA
(SHA256 challenge) and `/update` (HTTP Basic, user `admin`, via `gplug_smi: ota_password` →
`web_server_base` credentials; the SPA and captive portal register with
`add_handler_without_auth()` and stay open). Set it in the adopting config's `substitutions:` or
per build with `esphome -s ota_password <pw> compile dev.yaml`. `/api/status` reports
`ota_auth`, so the firmware card only asks for a password when one is set. Gotchas, all seen on
hardware:

- Plain `esphome upload` (no `-s`) reuses the validated config of the *last compile*
  (`.esphome/storage/dev.yaml.validated.yaml`), password included -- after a `-s` compile, pass the
  same `-s` to `upload` or it silently authenticates with the cached value.
- `password: ""` still runs the native OTA challenge, just with the empty password, so
  `esphome -s ota_password <new> upload` against an unprotected device fails with
  "Authentication invalid". To *set* a password, push the protected build through `/update`
  (open while no password is set) or the firmware card; to change it, authenticate with the old one.
- ESPHome's Basic-auth middleware rejects an unauthenticated `/update` once **per body chunk**: a
  1 MB upload without (or with wrong) credentials crawls for minutes, and the device's web server
  stays unreachable until the connection times out (~3 min). Browsers trigger this by themselves
  when credentials are given via `XMLHttpRequest.open(user, pass)` -- they send the body without
  them first and only retry after the 401. The firmware card therefore checks the password with an
  empty POST first (instant 401, or `Update Failed!` without touching flash) and then uploads with
  a preemptive `Authorization` header (see `spa/src/api.js`).

**Two configs, one firmware.** `gplug.yaml` is the *package*: self-contained, no path relative to
any config directory, because it is what a foreign ESPHome Device Builder pulls in when a gPlug is
adopted. Components come from `github://jluthiger/esphome-gplug` (`firmware/components`), the SPA
and presets are bundled *inside* the component (`components/gplug_smi/spa.html.gz`, `presets.json`,
both committed, regenerated by `npm run build` / `npm run presets` in `../spa`), and the partition
table is inline. `dev.yaml` includes it as a package and appends a local `external_components`
source; ESPHome processes sources in order and the last one wins (each installs its meta-path
finder at the front), so the working tree shadows the git clone. **Always build/flash `dev.yaml`
while developing** -- `gplug.yaml` alone compiles whatever is on GitHub `main`, not your edits.

**Device Builder discovery ("Discovered" list).** An ESPHome device is only listed there if its
mDNS TXT records carry `package_import_url`, `project_name` and `project_version`
(`esphome/zeroconf.py`, `DashboardImportDiscovery`); anything else is silently used just for the
online dot of configs the dashboard already has. That is why the gPlug was invisible to the
Device Builder on the same Wi-Fi network before 2026-09-10 although `dns-sd -B _esphomelib._tcp` on a Mac
showed it fine. `esphome: project:` plus `dashboard_import:` in `gplug.yaml` provide the three
records; adopting fetches the import URL and writes a small config with `packages:` pointing at it,
a fresh API key and the adopter's WiFi. Every build of `gplug.yaml` (via `dev.yaml` too) advertises
the URL, so discovery works from the first flash; the adopt step itself only works once the
bundled-asset version of the component is on GitHub `main`.

**The other "config vanished" cause on 2026-09-10 was the AP button itself.** Its first
implementation called `nvs_flash_erase()`, which wipes the **entire** nvs partition -- every namespace,
so `gplug_smi`'s `hw`/`meter` JSON went with the WiFi credentials on every press. While the LED work was
being tested by holding that button, that read as "config keeps disappearing for no reason". An earlier
revision of this paragraph blamed ESPHome's `ESP32Preferences::open()` self-erase (it does call
`nvs_flash_erase()` if `nvs_open("esphome")` fails) and grew `nvs` from 24 kB to 64 kB for headroom; that
theory was wrong for these incidents. The custom partition table was later dropped altogether
(2026-09-10, same day): the only thing it bought was a non-standard `app0` offset that broke
`esphome run`, and nvs holds three small blobs (WiFi credentials, `hw` JSON, `meter` JSON).

**Reset into AP mode**: hold the hardware AP button >= 3 s (wired to GND, `hw.pins.button` in the
stored hw config) — erases ESPHome's `esphome` NVS namespace (where `WiFiComponent` keeps its saved STA
credentials; `esp32/preferences.cpp`) and reboots. The `gplug` namespace -- hw pins, meter descriptor --
survives, so the device comes back up broadcasting its setup AP **with the LED still driven** (blue
blinking).

The button also works on firmware with **compiled-in WiFi credentials**, which is what every config
the ESPHome Device Builder generates on adoption has (it adds `wifi: ssid/password: !secret` to the
adopted YAML). Found 2026-09-10 right after the first adoption: the button erased the saved
credentials and rebooted as designed, and the device was back on the compiled-in network 3 s later,
so the AP never appeared. Fix: the button additionally sets a persistent `ignore_sta` flag in the
`gplug` namespace; on every later boot `gplug_smi::setup()` drops the compiled-in networks
(`WiFiComponent::clear_sta()`) and restarts WiFi (`disable()` + `enable()` re-runs `start()`, which
with no STA left goes straight to the fallback AP + captive portal -- this component sets up
`AFTER_WIFI`, so WiFi has already started by then). The flag is deliberately never cleared:
`WiFiComponent::start()` keys its saved-credentials preference on `has_sta()`, so credentials
saved through the portal while the compiled-in ones are dropped are only found again on boots where
they are dropped again. Verified on the real gPlugK with a build that compiled in one network:
button -> AP in < 1 s, portal onboarding to a *different* network, `/api/reboot` -> reconnects to
that portal network, not the compiled-in one. Harmless on builds without compiled-in credentials. Equivalent manual fallback (e.g. no button wired, or NVS is corrupt) -- note this one wipes
everything, hw/meter included:

```
esptool --chip esp32c3 --port /dev/cu.usbmodemXXXX erase_region 0x2D0000 0x70000    # whole nvs partition (448 kB)
```

The offsets come from the generated partition table, not from this file: check them before erasing
with `python gen_esp32part.py .esphome/build/gplug/.pioenvs/gplug/partitions.bin` (in ESP-IDF's
`components/partition_table/`). An old command with `0x9000` would now hit otadata and phy_init.

| File | Purpose |
|---|---|
| `gplug.yaml` | the package: wifi AP + captive portal, api, ota (optional `ota_password` substitution), sntp, uart, `gplug_smi`, project + dashboard_import, git component source, inline partitions (ESPHome's standard ESP-IDF layout: otadata, phy_init, app0 @ 0x10000 / app1 1408 kB each, nvs 448 kB, plus `data` 704 kB appended for the 15-min history) |
| `dev.yaml` | `gplug.yaml` + local component source; what you build and flash while developing |
| `base.yaml` | skeleton without the component, for size reference |
| `MEMORY.md` | flash and RAM breakdown: partition table, what the app image and static RAM are made of, heap consumers |
| `components/gplug_smi/` | external component (see below) |
| `components/captive_portal/` | forked+re-styled external component, shadows ESPHome's built-in one (see below) |
| `test/test_dsmr.cpp` | host unit test for the DSMR parser |
| `test/test_structure.cpp` | tests `decode_structure()` (the production DLMS decode path) against a real capture |
| `test/test_capturelist.cpp` | tests `find_capture_list()` (gPlugM/L+G capture-list decode) against two real captures |
| `test/test_eventlog.cpp` | tests `event_log.h`: ring order and wrap, folding of repeated events, the saturating repeat counter, back-dating after a clock sync, and the NVS blob round-trip including rejection of a truncated or corrupt one |
| `test/test_history.cpp` | tests `history_store.h`: append/rotate/wrap, crash recovery (torn record, half-written timestamp, interrupted erase), timestamp back-patching, and the bucket aggregation |
| `test/test_sniff.cpp` | tests `protocol_sniff.h`: DSMR ident line vs HDLC frame from the first bytes, ciphered/plain tag through LLC and GBT headers, a stray `/XYZ5` in ciphertext not flipping the verdict, reset |
| `test/test_framelog.cpp` | tests `frame_log.h`'s ring buffer and its wiring to `DlmsDecoder`'s capture hook (`last_frame()`/`last_frame_ok()`/`frame_seq()`), incl. the CRC-ok-but-wrong-key case the Data Stream view depends on |

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
- `frame_log.h` – fixed-size ring of the last 5 raw DLMS HDLC frames (ciphertext, capped 768 B, plus
  decrypted plaintext when available), for the SPA's Data Stream view (`/api/frames`,
  `/api/frames/<i>/raw|plain`). Header-only, no ESPHome deps, and a generic byte-blob ring with no
  notion of "key" — structurally incapable of exposing key material. Captures whatever the profile
  speaks: DLMS HDLC frames (raw ciphertext + decrypted APDU) or whole DSMR P1 telegrams, which the
  parser hands over through `set_raw_callback()` before it rewrites its buffer in place. `raw` is
  capped at 1280 B (a telegram or the descriptor's frame ceiling), `plain` at 768 B and unused on
  DSMR, which has nothing to decrypt.
- `history_store.h` – persistent 15-min history: an append-only log of 20 B records over the raw
  `data` partition, organised as rotating 4 kB sector buckets (16 B header + 204 records each).
  Appending costs one write; a sector is erased only when recycled, once per ~2.6 days, giving
  ~374 days of history on 176 sectors. Header-only, no ESPHome deps, host-testable
  (`test/test_history.cpp`) via an injected flash backend. Two flash facts shape the record layout:
  programming only clears bits (so "time unknown" is all-ones, never zero, and can be filled in
  later), and a record needs its own crc or a power cut mid-write is indistinguishable from data.
  Hence the two-phase write — payload+crc first, the timestamp as a separate word excluded from the
  crc, which is what lets records written before NTP synced be back-dated with no erase.
- `partition_flash.h` – the device backend for the above, and the only file that includes
  `esp_partition.h`. Note `esp_partition_write` does *not* fail on non-erased bytes, it silently
  ANDs into them; the append-only scheme is what guarantees virgin targets.
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
- `captive.html` – the actual branding **and the only screen that has to speak four languages
  without any stored preference**, since it is what a phone sees before the SPA exists. A flat
  six-string table in the page's own module script picks German, French, Italian or English from
  `navigator.languages` and rewrites the labels; the markup itself ships German, which is what
  stays on screen if the captive-portal webview blocks scripting. Deliberately not the SPA's i18n
  module: this page is framework-free on purpose. Cost: 1.5 kB gzipped. Everything else is the
  same markup/JS/form-field contract as upstream's page
  (dynamic title/MAC/network-list from `/config.json`, `#ssid`/`#psk` fields posting to
  `/wifisave`; the upstream `/update` OTA form was removed, updates live in the SPA), only the `<style>` block and viewport/color-scheme meta
  changed, using the SPA's tokens and font stack (`spa/src/style.css`, both themes: dark default,
  light via `prefers-color-scheme` since the captive webview has no toggle). Keep the token values
  in sync by hand when the SPA palette changes. This is the
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
| GET | `/api/status` | version, hostname, uptime, build, `app` (first 16 hex digits of the running image's ELF SHA-256, see the OTA section), ota_auth, heap, wifi, hardware, meter counters |
| GET | `/api/live` | `{age, no_data, key_invalid, diag, rx_bytes, rx_age, detect:{protocol, encrypted, hits, age}, smid, p (kW net), pi, po (W), ei, eo (kWh), values{name:value}}`. `detect` is the header sniffer's verdict (below), available before any profile is configured: `protocol` `"dsmr"`/`"dlms"`/null, `encrypted` true/false/null (null = DLMS tag not seen yet), `hits` = header hits behind the verdict, `age` = seconds since the last one |
| GET | `/api/ring` | `{period:10, samples:[[pi,po,p1,p2,p3],…]}` |
| GET | `/api/wifi/scan` | last scan results kept by the wifi component (no active scan trigger yet) |
| GET | `/api/presets` | embedded presets (gzip) |
| GET/POST | `/api/config/hardware` | `{variant, pins:{rx,red,green,blue,button}, baud?, parity?: "N"\|"E", serial_flags?}`. The line parameters are the variant's (from `variants` in `presets.json`, same for all its presets) and are applied to the UART at once, so the sniffer listens before a profile exists |
| POST | `/api/config/meter` | `{preset, key? \| keep_key?, auth_key?, descriptor:{protocol, mode, baud, rx, serial_flags?, buffer?, obis[]}}`. `keep_key: true` instead of `key` re-uses the GUEK/auth key of the stored config (400 `no stored key` if there is none); the body is rewritten with the key before it is applied and saved |
| POST | `/api/config/wifi` | `{ssid, psk}` → `save_wifi_sta` |
| POST | `/api/reboot` | |
| GET | `/api/log` | the persistent event log: `{now, uptime, cap, events:[{t, up, code, detail, value, repeat},…]}`, oldest first. `t` is 0 for a record written before the clock had ever synced, which is what `up` (uptime in seconds) is for. Codes and details are numbers, deliberately: the SPA renders them in the user's language (see below) |
| GET | `/api/history.csv?from=<qh>&to=<qh>&format=…` | Load-profile download: every stored 15-min record at native resolution, `text/csv`, `Content-Disposition: attachment`. `from` inclusive / `to` exclusive quarter-hour indices, none = everything including undated records. `format=full` (default) = all columns, field names are the implementation's own (German), see `history_csv.h`; two other `format` values (exact spelling in `handle_history_csv_()` below) mirror the CKW customer-portal export instead (tab separated, interval-start timestamp as `DD.MM.YY HH:MM`, one kWh column with 3 decimals), one per direction, for a line-by-line compare. Format and rules in `history_csv.h`; streamed sector by sector (below) |

**Setup diagnosis (`diag` in `/api/live`, `GplugSmi::diag_`)** says why there are no values, so
the SPA can send the user to the wizard step that fixes it. Three timestamps feed it: any byte on
the HAN UART (`last_rx_ms_`, plus the running `rx_bytes`), any decoded frame/telegram
(`last_frame_ms_`), and any value that matched a configured OBIS code (`last_match_ms_`). Same 60 s
window and grace (after boot or a meter config change) as the LED's no-data verdict:

| `diag` | meaning | SPA sends the user to |
|---|---|---|
| `ok` | a configured OBIS code matched in the last 60 s | – |
| `waiting` | grace period, nothing conclusive yet | – |
| `key` | frames arrive, the GUEK doesn't decrypt them | meter step, key field |
| `no_match` | frames decode, but none of the profile's OBIS codes is in them | meter step (profile) |
| `protocol` | the header sniffer has seen ≥ 2 frames/telegrams of the *other* protocol in the last 60 s; conclusive at once, no grace | meter step (profile; the detected one is proposed) |
| `garbled` | bytes arrive but never form a valid frame/telegram (baud, parity, protocol) | meter step, then hardware |
| `silent` | nothing on the line at all: pin/variant, cable, or the utility hasn't enabled the customer port | hardware step |
| `unconfigured` | no meter descriptor | – |

**Protocol sniffer (`protocol_sniff.h`, `detect` in `/api/live`).** Every HAN byte also runs
through a header-only detector, whether or not a profile is configured: a P1 telegram opens with
the IEC 62056-21 ident line `/` + 3 letters + baud digit (`/KFM5…`, `/LGF5…`), an HDLC type-3
frame with `7E A?`; for DLMS the tag after the LLC `E6 E7 00` says ciphered (`DB`) or plain
(`0F`), a GBT segment (`E0`, 7-byte header) is skipped to the same tag. The verdict is the protocol
with more header hits since the last hw/meter config (ties go to DLMS: `A0..AF` never occur in
DSMR ASCII, while ciphertext can contain a stray `/XYZ5`). It answers within the first frame and
needs no key, which is what the wizard's meter step needs to propose the profile right after the
hardware step -- the reason the hardware step now carries the line parameters. Host test:
`test/test_sniff.cpp`.

`no_match` and `protocol` also turn the LED red: before, frames without a single matching register kept it
green while the app showed nothing. Verified on the gPlugK with nothing on its HAN port
(`waiting` for 60 s, then `silent`, `rx_bytes` 0); `garbled`/`no_match` need bytes on GPIO4 and
are covered by the mock only.

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

### Event log (`event_log.h`, `/api/log`)

Thirty-two records in one NVS blob, answering the question a serial console cannot once the cable
is unplugged: **why did it restart**. `esp_reset_reason()` is read at boot and stored, so a panic,
a task or interrupt watchdog, and a brownout are told apart from a normal power-up or the
software restart that an OTA and a config save perform. Alongside that: Wi-Fi up and down with
the RSSI, the meter going quiet and coming back (with the `diag` verdict that says why), config
changes, history-store failures, and the AP button erasing the Wi-Fi credentials.

Four decisions worth knowing:

- **NVS, not the `data` partition.** The write volume is a handful of records a day, which is what
  NVS is for; it brings its own wear levelling and needs no partition-table change, so the feature
  ships by OTA. The whole blob is 388 B (`4 + 32 x 12`), rewritten on each append.
- **Codes, not sentences.** A record is 12 B of numbers. The wording lives in the SPA, in four
  languages, and can be reworded without touching firmware or invalidating stored records. The
  event and reset-reason numbers are therefore append-only: they are on flash, so never renumber.
- **Repeats fold.** A flapping Wi-Fi would otherwise push every other record out within minutes
  and write NVS each time, so an identical event within five minutes bumps a counter on the
  newest record instead of adding one. A new record is flushed to NVS immediately (the next event
  may be the crash); a folded one waits for the 60 s rate limit.
- **Entries written before the first clock sync are back-dated**, the same trick the history store
  uses: uptime is always known, so once SNTP lands, `epoch_now - (uptime_now - uptime_then)` dates
  this boot's records. Records from a previous boot with no clock stay undated and the SPA shows
  their uptime instead.

An update is logged without hooking ESPHome's OTA at all: the image identity (the same ELF hash
`/api/status` reports) is kept in NVS, and a boot that finds a different one logs "firmware
updated". Verified on the gPlugK -- the first boot after flashing this recorded a software
restart with 185 kB free heap, Wi-Fi at -39 dBm three seconds later, and meter data two seconds
after that. A crash, a watchdog and a brownout are covered by the host tests and the mock rather
than provoked on hardware; `test/test_eventlog.cpp` pins the ring, the folding, the back-dating
and the blob round-trip, including rejection of a corrupt blob.

**Wiping the stored history.** The 15-min log lives in the `data` partition and survives every
`esphome run`. To clear it, erase that region — take the offset from the boot log
(`gplug_smi: history: partition 0x...`), never a hardcoded constant, since the generated partition
table derives it:

```
esptool --chip esp32c3 --port /dev/cu.usbmodemXXXX erase_region <addr> 0xB0000
```

The device reformats one sector on the next boot; NVS (WiFi, hw/meter config) is untouched.

**Load-profile export (`/api/history.csv`, `history_csv.h`, History tab → "export load profile").**
The stored records, one CSV row each, for checking the grid operator's bill and for ZEV/LEG
settlement. Two things make it settlement-grade rather than chart-grade:

- **Counters are exact.** `values_[]` is `float`; past ~10 MWh a counter only has ~10 Wh of
  resolution there, invisible on a gauge but exactly the jitter a per-interval energy must not
  carry. The decoders therefore also hand Ei/Eo to `note_energy_exact_()` as a `double` in Wh
  (`ei_wh_exact_`/`eo_wh_exact_`), and that is what goes into the record. Older records written by
  the float path keep whatever rounding they got.
- **A delta is printed only when it is one interval's energy.** The row's import-energy /
  export-energy columns come from the counter difference to the previous record, and stay empty
  when that record is not the directly preceding quarter hour (flagged as a gap), the meter config
  was replaced (flagged as a config change), a counter is absent, or the jump is implausible
  (> 30 kW average). The counters themselves are always printed, so nothing is lost -- the reader
  just cannot misattribute a multi-interval delta to one row. Column and flag names in the file
  itself are the implementation's own (German), see `history_csv.h`; `test/test_csv.cpp` pins
  every rule.
- **A reboot does not break the chain.** Until 2026-09-12 every boot re-applied the stored meter
  config through `apply_meter_json_()`, which flags the next record `HF_CONFIG_CHANGE`, so the first
  interval after any power cut or OTA lost its energy in the export. `setup()` now resets the
  pending flags to `HF_BOOT_BEFORE | HF_PARTIAL` after loading the stored config: the counters are
  the meter's and absolute, only a *replaced* config (other register mapped to "Ei", swapped meter)
  breaks the chain. Verified on the gPlugK: the first record after an OTA reboot carries the
  reboot and partial-interval flags and keeps its delta.
- **A second shape mirrors the grid operator's own export.** CKW's customer portal hands out a
  two-column export (interval period, then energy consumption in kWh) with `DD.MM.YY HH:MM`
  interval starts and kWh to three decimals (their values carry float32 noise in the 9th digit,
  e.g. `0.156000003`, so compare at 1 Wh). Two `format` values (one per direction, exact
  spelling in `handle_history_csv_()`) produce exactly those two columns, undated records left
  out, an unattributable interval kept as a row with an empty value so the two files stay aligned
  line by line. The feed-in column's header text is a guess at CKW's wording; check against a
  real feed-in export.

**The export's own column names stay German while the app speaks four languages.** The CKW-shaped
output has to: it mirrors the vendor's file byte for byte, and that file is German. The `full`
format is this project's own, so its column names are a real decision and an open one -- either
language-neutral English columns for everybody, or a `lang` parameter and four variants to test.
Nothing was changed here on the way to the four-language UI, because the format is what a
settlement is computed from and renaming its columns is not a documentation change.

A full year is ~35k rows / ~2.5 MB, so the response is chunked, and the store's mutex is never
held across a socket write: the handler snapshots the sector range, then per sector locks, copies
4 kB, unlocks, formats and sends (`HistoryStore::read_sector`). The main loop's quarter-hour append
only `try_lock`s, so it is never stalled by an export. Local times come from ESPHome's time
component, whose timezone `gplug.yaml` pins to `Europe/Zurich` (the default would be whichever
machine compiled the image). Verified against the mock (`spa/mock`, `MOCK_HIST_DAYS`); not yet on
hardware.

**Erase timing.** A 4 kB sector erase runs with the flash cache disabled and
`CONFIG_UART_ISR_IN_IRAM` is off, so nothing drains the UART FIFO for 30-50 ms — at 115200 baud that
overflows after ~11 ms and clips one meter frame. The store therefore pre-arms the recycle and
`hist_service_()` performs it only in the quiet stretch after a frame completed (or while the meter
is silent anyway), which at a 1 s meter cadence leaves ~900 ms of headroom. It happens once per
~2.6 days. `CONFIG_SPI_FLASH_AUTO_SUSPEND=y` or `CONFIG_UART_ISR_IN_IRAM=y` would remove the hazard
outright; neither is enabled by default (each has its own caveats, and ESPHome's UART driver may not
pass `ESP_INTR_FLAG_IRAM` anyway).

### Known gaps (PoC)

- Firmware is 1007 kB flash / 60 kB static RAM of a 1408 kB app slot and 320 kB SRAM (890 kB before
  adopting mbedtls — a real crypto library is bigger than the hand-rolled one it replaced; +8 kB RAM
  for the raw-frame capture ring, +0.6 kB for the history store). Full breakdown in `MEMORY.md`.
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
- History is verified by host tests and a mock-server walkthrough only: real sector rotation, recovery
  from a power cut mid-append, and a 24 h run still need the device. A dev build with
  `HIST_INTERVAL_S = 60` exercises rotation in ~3.5 h.
- WiFi scan endpoint returns whatever the wifi component last scanned; may be empty right after boot.
- Config writes are applied immediately from the HTTP task; UART reconfiguration is not yet deferred to the main loop.
