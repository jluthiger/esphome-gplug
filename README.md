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
| [`migration.md`](migration.md) | Replacing Tasmota on an existing gPlug: what to save first, software setup for macOS, Linux and Windows, erasing, flashing, and the way back |

## Ready-made firmware

Releases are built and published by GitHub Actions ([`.github/workflows/release.yml`](.github/workflows/release.yml)),
so nothing below is needed just to run the firmware:

| Where | What | For |
|---|---|---|
| <https://jluthiger.github.io/esphome-gplug/> | Web installer (ESP Web Tools) | A fresh gPlug, or one still on Tasmota: flash over USB from Chrome or Edge, no tools installed |
| [Latest release](https://github.com/jluthiger/esphome-gplug/releases/latest) | `gplug-<version>.ota.bin` | Updating a gPlug that already runs this firmware: *Setup → Firmware* in the device's own app |
| | `gplug-<version>.factory.bin` | Full flash from `0x0` with esptool |

Cutting a release: push a tag — `git tag v1.2.3 && git push origin v1.2.3`. The workflow rebuilds
the SPA (and refuses to release if the committed `spa.html.gz` is stale), compiles `dev.yaml` with
`version` substituted from the tag, attaches both images to the release and redeploys the installer
page with the new image. `workflow_dispatch` does the same as a dry run: build artefacts only, no
release, no deploy.

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
this repo as a package, no clone needed.

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
