# gPlug ESPHome firmware

Replace the Tasmota firmware on [gPlug](https://gplug.ch/) smart-meter adapters with an
[ESPHome](https://esphome.io/) firmware that keeps the Tasmota Smart Meter Interface's HAN
decoding capabilities, but is usable by non-technical people via a guided setup wizard hosted
on the device itself. See [`intent/intent.md`](intent/intent.md) for goal, scope and constraints.

## Layout

| Path | What |
|---|---|
| [`firmware/`](firmware/README.md) | ESPHome external component (`gplug_smi`): DSMR/P1, DLMS/COSEM + HDLC, AES-GCM decoding, 15-min history on flash, host-side test suite |
| [`spa/`](spa/README.md) | Setup wizard + live app (Preact, no build framework), bundled into the firmware image and served from the device |
| [`gplug/`](gplug) | Existing Tasmota scripts per variant/provider, source of the SPA's presets (`gPlugD`, `gPlugD-E`, `gPlugK`, `gPlugM`) |
| [`intent/`](intent) | Intent doc: goal, scope, constraints |
| [`install/`](install) | Web installer page (ESP Web Tools) + manifest, deployed to GitHub Pages by the release workflow |
| [`CHANGELOG.md`](CHANGELOG.md) | What changed per release, with upgrade notes |
| [`migration.md`](migration.md) | Replacing Tasmota on an existing gPlug: what to save first, software setup for macOS, Linux and Windows, erasing, flashing, and the way back |

## Ready-made firmware

Releases are built and published by GitHub Actions ([`.github/workflows/release.yml`](.github/workflows/release.yml)),
so nothing below is needed just to run the firmware:

| Where | What | For |
|---|---|---|
| <https://jluthiger.github.io/esphome-gplug/> | Web installer (ESP Web Tools) | A fresh gPlug, or one still on Tasmota: flash over USB from Chrome or Edge, no tools installed |
| [Latest release](https://github.com/jluthiger/esphome-gplug/releases/latest) | `gplug-<version>.ota.bin` | Updating a gPlug that already runs this firmware: *Setup → Firmware* in the device's own app |
| | `gplug-<version>.factory.bin` | Full flash from `0x0` with esptool |

### Versions and branches

[Semantic Versioning](https://semver.org/), 0.x while this is a proof of concept. What changed per
release, with upgrade notes: [`CHANGELOG.md`](CHANGELOG.md).

| Version | Means |
|---|---|
| `0.2.x` patch | fixes only: OTA-safe, stored settings, HTTP API and Home Assistant entity keys unchanged |
| `0.x.0` minor | features; in 0.x also breaking changes (settings or profile format, HTTP API, renamed entity keys), stated under *Upgrade notes*. A partition-table change is always minor and marked "USB reflash required" |
| `0.x.0-rc.N` | release candidate: GitHub pre-release, for the hardware test; the installer page and `stable` stay on the last release |
| `0.x.0-dev` | what `main` says between releases: the next version, never a release |

- **`main`** is development. Its `gplug.yaml` says `version: "<next>-dev"` and pulls the component from `main`.
- **`stable`** always points at the newest release commit, moved by the release workflow. A gPlug
  adopted in an ESPHome Device Builder follows it (`dashboard_import` in `gplug.yaml`), and the
  `gplug.yaml` there pins the component to its own tag -- so a Device Builder builds released code,
  from one commit. To try unreleased code on a Device Builder, point the adopted package at `@main`.
- The version lives in `gplug.yaml`, not in the tag: a tag has to name what its commit already says,
  or the workflow refuses it (`.github/release-check.sh`).

### Cutting a release

With Claude Code: `/release`. By hand:

1. `main` is green: `cd spa && npm run build` committed, `firmware/test/run.sh`, `esphome compile dev.yaml`, `firmware/MEMORY.md` current (`python3 firmware/tools/size_report.py --check`).
2. **Release commit** for `0.3.0-rc.1`: in `firmware/gplug.yaml` set `version: "0.3.0-rc.1"` and the
   component `ref: v0.3.0-rc.1`; in `CHANGELOG.md` rename *Unreleased* to `## [0.3.0] – <date>`
   and add an empty *Unreleased* above it. `.github/release-check.sh 0.3.0-rc.1` must pass. Commit, then
   `git tag v0.3.0-rc.1 && git push origin main v0.3.0-rc.1` -- a pre-release with both images.
3. **Hardware test** of the rc, results noted in the release: OTA from the previous *release* (not a
   development build) keeps Wi-Fi, meter settings, history count and event log; `diag` is `ok`; Home
   Assistant entities are present; the app loads on a phone and a desktop; the web installer on a
   spare unit when there is one. Variants not tested on hardware are listed as such.
4. **Release**: one commit on top of the tested rc that changes nothing but `version: "0.3.0"`,
   `ref: v0.3.0` and the date in the changelog;
   `.github/release-check.sh 0.3.0`; `git tag v0.3.0 && git push origin main v0.3.0`. The workflow
   publishes the release with the changelog section as notes, deploys the installer page and moves `stable`.
5. **Back to development**: `version: "0.4.0-dev"`, `ref: main`, commit and push.

The workflow rebuilds the SPA and refuses to release if the committed `spa.html.gz` is stale, checks
the release commit, compiles `dev.yaml` and attaches `gplug-<version>.ota.bin` and
`.factory.bin`. `workflow_dispatch` is a dry run: build artefacts only, no release, no deploy.

A released image is built from the config in this repository, so two secrets in it are public: the
OTA password is empty, meaning anyone on the LAN can reflash the device, and the API encryption key
is the placeholder in `gplug.yaml`, meaning anyone on the LAN can talk to the ESPHome API. Neither
exposes the meter key, which is entered on the device and never leaves it. If the LAN is not
trusted, build an image of your own with
`esphome -s ota_password <pw> -s version <v> compile dev.yaml` and your own API key.

GitHub Pages must be set to *Deploy from GitHub Actions* once, in the repository settings.

## Quick start

Prerequisites: Node.js (SPA build), [ESPHome](https://esphome.io/guides/installing_esphome) 2026.x
(`brew install esphome` or `pip install esphome`), a gPlug on USB.

```
# 1. Build the setup wizard -> firmware/components/gplug_smi/spa.html.gz (embedded into the firmware image)
cd spa && npm install && npm run build

# 2. Compile the firmware (gplug_smi + captive_portal + the SPA). dev.yaml = gplug.yaml with local components
cd ../firmware && esphome compile dev.yaml

# 3. Flash over USB and watch the boot log (Ctrl+C to stop the log; add --no-logs to return after flashing)
esphome run dev.yaml --device /dev/cu.usbmodemXXXX
```

**Replacing Tasmota on a gPlug you already own?** Follow [`migration.md`](migration.md) instead of
the three commands above. It covers the one irreversible step — copying the meter key out of the
Tasmota script before erasing it — along with installing the tools on macOS, Linux or Windows, and
the way back to Tasmota if you want it.

Already have an ESPHome Device Builder (e.g. the Home Assistant add-on)? A flashed gPlug shows up
there under *Discovered* and can be adopted; the adopted config pulls `firmware/gplug.yaml` from
this repo's `stable` branch (the latest release) as a package, no clone needed.

First boot: the device has no WiFi yet, so it opens the `gPlug-Setup` access point. Join it from a
phone, the captive portal asks for your home WiFi, the device reboots into it, and the setup wizard
is then at `http://gplug.local/` (pins, meter preset). From then on that address opens the live
app instead — four tabs: **Live** (current power, 1 h chart, counters, per-phase), **History**
(Day/Week/Month/Year from the device's own 15-min flash history, ~374 days, with a CSV export),
**Data Stream** (the last captured raw DLMS frames, with hex export, for diagnosing a meter that
won't decode) and **Setup** (addresses, Wi-Fi change, key status, firmware update, event log, language, light/dark). The whole
app speaks German, French, Italian and English, switched in the Setup tab and remembered per
browser; a fresh phone gets whichever of the four it asks for. The wizard stays one tap away. Re-entering setup directly: hold the AP button >= 3 s.

Dev loop without hardware: `cd spa && npm run dev` serves the wizard against a mock device API on
http://localhost:8080. Host-side decoder tests: see [`firmware/README.md`](firmware/README.md).

See the linked READMEs for details (flashing caveats, wizard flow, device API).
