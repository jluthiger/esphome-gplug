# gPlug ESPHome firmware

Replace the Tasmota firmware on [gPlug](https://gplug.ch/) smart-meter adapters with an
[ESPHome](https://esphome.io/) firmware that keeps the Tasmota Smart Meter Interface's HAN
decoding capabilities, but is usable by non-technical people via a guided setup wizard hosted
on the device itself. See [`intent/intent.md`](intent/intent.md) for goal, scope and constraints.

## Layout

| Path | What |
|---|---|
| [`firmware/`](firmware/README.md) | ESPHome external component (`gplug_smi`): DSMR/P1, DLMS/COSEM + HDLC, AES-GCM decoding, host-side test suite |
| [`spa/`](spa/README.md) | Setup wizard (Preact, no build framework), bundled into the firmware image and served from the device |
| [`gplug/`](gplug) | Existing Tasmota scripts per variant/provider, source of the SPA's presets (`gPlugD`, `gPlugD-E`, `gPlugK`, `gPlugM`) |
| [`intent/`](intent) | Intent doc: goal, scope, constraints |

## Quick start

```
cd spa && npm install && npm run build   # produces dist/index.html.gz, embedded by the firmware
cd ../firmware && esphome compile gplug.yaml
```

See the linked READMEs for details (host tests, flashing caveats, wizard flow, device API).
