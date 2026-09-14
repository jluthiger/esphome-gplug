# Changelog

What changed for someone running a gPlug, per release. Versions follow [Semantic Versioning](https://semver.org/);
while the project is a proof of concept (0.x), a minor release may break things, and says so under
**Upgrade notes**. How releases are made: [README.md](README.md#cutting-a-release).

Every change a user would notice gets a line under **Unreleased** in the same commit. Upgrade notes
answer three questions whenever the answer is not "nothing to do": does it need a USB reflash
(partition table), are stored settings and history kept, do Home Assistant entity IDs change.

## [Unreleased]

## [0.4.0] – 2026-09-14

### Added
- The browser tab title names the device and the current screen (for example "gplugesp · Verlauf")
  in the chosen language, instead of "gPlug Setup" everywhere, so several open gPlugs and bookmarks
  can be told apart.
- Charts show the value under the pointer: hover with the mouse, drag a finger across the chart, or
  focus it and use the arrow keys. A line above the chart gives the time and the value -- power with
  direction on Live (marked when it is a stored 15-minute average), energy or power per interval on
  History, free heap and largest block on the memory trend -- and "no data" for a gap.
- Heap monitoring: the Setup tab shows free heap, its minimum since start, the largest free block and
  stack reserves, with a 24 h trend, so a memory leak shows as a falling line before the device
  restarts. Home Assistant gets "Free heap" and "Largest heap block" as diagnostic entities, the event
  log records low memory, and a restart entry shows the free heap at start.
- Setup tab: restart the device from the firmware card, after a confirmation. The card waits until the
  device is back (or says that it is not), and the event log records that the restart was requested
  from the web app.

### Changed
- The firmware update card is now "Firmware and restart".
- Setup tab: the key, firmware update, memory, event log and appearance cards fold to one line with a
  short summary; connection and language stay open. The choice is remembered in the browser, and a
  closed memory or event log card no longer queries the device.

### Upgrade notes
- No USB reflash: the partition table is unchanged. Wi-Fi, meter settings, history and event log
  are stored as before; OTA from the 0.3.0 release is checked on the release candidate.
- Home Assistant: two new diagnostic entities, "Free heap" and "Largest heap block"; nothing is
  renamed or removed.

## [0.3.0] – 2026-09-14

### Added
- Home Assistant entities: power (net, import, export), energy import/export with tariffs T1/T2,
  voltage, current and power per phase, and as diagnostics meter status, meter ID, seconds since the
  last frame and Wi-Fi signal. Filled every 10 s from whatever the meter profile reads; values the
  profile does not have stay unknown.
- Wide screens (1024 px and more) get their own layout: side rail navigation, power beside the hour
  chart, history chart beside export and registers, Data Stream as list and detail pane, Setup in two
  columns with the event log as a table. Phones and tablets are unchanged.
- The home-screen shortcut has a name and the gPlug icon.

### Fixed
- Data Stream showed the text of an older frame under a newer frame's header after new frames had
  arrived (frames are addressed by position in the device's buffer).

### Changed
- Adopting a gPlug in an ESPHome Device Builder now follows the `stable` branch, which moves to each
  release, instead of `main`.
- `gplug.yaml` fetches the component on every build (`refresh: 0s`), so it can never be older than the
  `gplug.yaml` that uses it. Builds need GitHub reachable.

### Upgrade notes
- No USB reflash: the partition table is unchanged. OTA kept Wi-Fi, meter settings, history and
  event log on the gPlugK when updating between development builds (2026-09-14); OTA from the 0.2.0
  release itself is checked on the release candidate.
- Home Assistant: the entities are new, nothing is renamed. A config adopted before this release pulls
  `gplug.yaml@main`; change the package to `@stable` to stay on releases.

## [0.2.0] – 2026-09-12

### Changed
- Wider screens (800 px and more) get the app in a wider column with the tab bar on top, instead of
  a phone-sized strip.
- The load-profile export has one format: the full record with counters. The two CKW-portal layouts
  are gone.

### Upgrade notes
- OTA from 0.1.0; nothing to do.

## [0.1.0] – 2026-09-12

First release: ESPHome firmware for gPlugD, gPlugD-E, gPlugK and gPlugM replacing Tasmota, one image
for all variants.

### Added
- Meter decoding: DSMR/P1, DLMS/COSEM over HDLC, AES-128-GCM encrypted meters, the gPlugM capture-list
  format; protocol detected from the line.
- Setup: captive portal for Wi-Fi, then a wizard in the device's own app (variant and pins, meter
  profile from the former Tasmota scripts, key), with a diagnosis and the way back to the step that
  fixes a failed setup.
- App with Live, History (day/week/month/year from 15-minute records stored on the device for about a
  year, load-profile CSV export), Data Stream (raw frames) and Setup (Wi-Fi, key status, firmware
  update, event log, language, light/dark). German, French, Italian and English.
- Firmware updates from the app and over ESPHome OTA, with an optional shared password.
- RGB status LED, AP button to re-enter Wi-Fi setup.
- Releases on GitHub and a browser installer for a first flash over USB; migration guide from Tasmota.
