# Changelog

What changed for someone running a gPlug, per release. Versions follow [Semantic Versioning](https://semver.org/);
while the project is a proof of concept (0.x), a minor release may break things, and says so under
**Upgrade notes**. How releases are made: [README.md](README.md#cutting-a-release).

Every change a user would notice gets a line under **Unreleased** in the same commit. Upgrade notes
answer three questions whenever the answer is not "nothing to do": does it need a USB reflash
(partition table), are stored settings and history kept, do Home Assistant entity IDs change.

## [Unreleased]

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
