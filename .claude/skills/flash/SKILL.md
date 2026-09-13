---
name: flash
description: Flash the dev firmware to a gPlug on USB or OTA and read its logs/API to verify on hardware. Only when the user explicitly asks to flash, upload or test on the device.
disable-model-invocation: true
---

# Flash a gPlug

Hardware action — confirm device and target with the user before writing.

1. Build fresh: `cd spa && npm run build`, then `cd ../firmware && esphome compile dev.yaml`.
2. Find the port: `ls /dev/cu.usbmodem*` (USB) or ask for the IP/hostname (`gplug.local`) for OTA.
3. Flash without blocking on the log:
   `esphome upload dev.yaml --device /dev/cu.usbmodemXXXX` (or `--device <ip>`).
   The partition table is stock ESPHome layout (`app0` at `0x10000`), so plain `esphome upload`
   works. Never erase flash (`esptool erase_flash`) unless the user asks: it wipes WiFi, hardware
   and meter config and the 15-min history.
4. Logs: `esphome logs dev.yaml --device ...` run in background, read for a bounded time, stop it.
5. Check the API: `curl -s http://<host>/api/status` (compare `app` against the new build),
   `/api/live` (`diag`, `detect`, `age`), `/api/log`.
6. Report what was verified on which variant (gPlugD/D-E/K/M) with the date, in the style of the
   READMEs.
