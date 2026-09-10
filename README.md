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

Already have an ESPHome Device Builder (e.g. the Home Assistant add-on)? A flashed gPlug shows up
there under *Discovered* and can be adopted; the adopted config pulls `firmware/gplug.yaml` from
this repo as a package, no clone needed.

First boot: the device has no WiFi yet, so it opens the `gPlug-Setup` access point. Join it from a
phone, the captive portal asks for your home WiFi, the device reboots into it, and the setup wizard
is then at `http://gplug.local/` (pins, meter preset). From then on that address opens the live
app instead — four tabs: **Live** (current power, 1 h chart, counters, per-phase), **Verlauf**
(60 min from RAM, plus Tag/Woche/Monat/Jahr from the device's own 15-min flash history, ~374 days),
**Datenstrom** (the last captured raw DLMS frames, with hex export, for diagnosing a meter that
won't decode) and **Setup** (addresses, WLAN ändern, key status, night mode). The wizard stays one
tap away. Re-entering setup directly: hold the AP button >= 3 s.

Dev loop without hardware: `cd spa && npm run dev` serves the wizard against a mock device API on
http://localhost:8080. Host-side decoder tests: see [`firmware/README.md`](firmware/README.md).

See the linked READMEs for details (flashing caveats, wizard flow, device API).
