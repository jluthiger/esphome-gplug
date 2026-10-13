# captive_portal: origin and license

This directory is a modified copy of the `captive_portal` component of
[ESPHome](https://github.com/esphome/esphome), based on esphome 2026.6.5.

Copyright (c) 2019 ESPHome. Modifications copyright (c) 2026 Jürg Luthiger.

ESPHome licenses its C++ runtime files (`.c`, `.cpp`, `.h`, `.hpp`, `.tcc`, `.ino`) under the GNU
General Public License version 3 and everything else under the MIT license; upstream files carry
no per-file headers. The same applies here:

| File | Upstream license | Changed |
|---|---|---|
| `dns_server_esp32_idf.h`, `dns_server_esp32_idf.cpp` | GPLv3 | no, byte-identical |
| `captive_portal.h`, `captive_portal.cpp` | GPLv3 | yes, 2026-09-10: the page is set from `captive.html` at code generation instead of the vendored `captive_index.h` |
| `__init__.py` | MIT | yes, 2026-09-10: loads and gzips `captive.html`; `compression: br` removed |
| `captive.html` | MIT: upstream ships the page as the generated `captive_index.h`, built from [esphome-webserver](https://github.com/esphome/esphome-webserver), copyright (c) 2021 wilberforce | yes, 2026-09-10 onwards: restyled, four languages, OTA form removed |

The modifications are licensed under GPL-3.0-or-later like the rest of this repository
([`LICENSE`](../../../LICENSE), which is also the GPLv3 text for the upstream C++ files). What was
changed and why is described in [`firmware/README.md`](../../README.md#component-captive_portal-fork).
