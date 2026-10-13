# Third-party notices

METER is licensed under the [GNU General Public License, version 3 or later](LICENSE). It contains
or is built with the software below, which keeps its own copyright and license. All of these
licenses are compatible with GPLv3.

## In this repository

| What | Where | License | Copyright |
|---|---|---|---|
| ESPHome `captive_portal` (fork, based on esphome 2026.6.5) | `firmware/components/captive_portal/` | C++ files GPLv3, other files MIT; see [`NOTICE.md`](firmware/components/captive_portal/NOTICE.md) | © 2019 ESPHome; setup page © 2021 wilberforce |
| Meter presets | `firmware/components/gplug_smi/presets.json` | Derived from the Tasmota scripts gPlug publishes for its adapters; no license terms stated by the source | gPlug |

## Built into the app (`spa.html.gz`)

The bundle is minified without license comments to fit the device's flash budget; the notices are
kept here instead.

| What | License | Copyright |
|---|---|---|
| [Preact](https://github.com/preactjs/preact) | [MIT](https://github.com/preactjs/preact/blob/main/LICENSE) | © 2015-present Jason Miller |
| [htm](https://github.com/developit/htm) | [Apache-2.0](https://github.com/developit/htm/blob/master/LICENSE) | © 2018 Google Inc. |

## Linked into the firmware image

Not in this repository; the build fetches them.

| What | License |
|---|---|
| [ESPHome](https://github.com/esphome/esphome) | C++ runtime GPLv3, Python code generation MIT, © 2019 ESPHome |
| [ESP-IDF](https://github.com/espressif/esp-idf) | Apache-2.0, © Espressif Systems, with components under their own licenses (FreeRTOS: MIT, lwIP: BSD-3-Clause, Mbed TLS: Apache-2.0) |

Because the image links ESPHome's GPLv3 runtime, a released firmware binary is distributed under
GPLv3; its complete source is this repository at the release's tag.

## Build and documentation tools

Used to build, not distributed: [esbuild](https://github.com/evanw/esbuild) (MIT),
[marked](https://github.com/markedjs/marked) (MIT), [Playwright](https://github.com/microsoft/playwright) (Apache-2.0).
