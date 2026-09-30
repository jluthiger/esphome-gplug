---
id: device-firmware
title: Update the firmware
chapter: device
screen: device-fw
order: 50
mock: 2026-09-30
hardware: gPlugK 2026-09-25
---
## Story
As the owner of a gPlug I want to install new firmware without connecting a cable, so that my device
gets bug fixes and new features.

## Steps
1. On the {{S.tabDevice}} tab, open the {{S.fwTitle}} card. It shows the installed version.
2. Tap {{S.fwRelCheck}}. If there is a newer version, a line such as
   {{S.fwRelAvail("0.6.0", "0.7.0")}} appears with the link {{S.fwRelNotes}}.
   {{img:device-fw|An update is available}}
3. Tap {{S.fwRelInstall("0.7.0")}} and confirm with {{S.fwRelConfirm}}.
   {{img:device-fw-confirm|Confirming the installation}}
4. The gPlug downloads the firmware itself and restarts. Keep it powered.
   {{img:device-fw-done|Firmware updated}}

## Acceptance
- The card shows {{S.fwDone}} and the new version.
- Settings and history are kept; only the quarter hour in progress is lost.

## Notes
- The gPlug never checks for updates on its own, only when you tap {{S.fwRelCheck}}.
- To install a firmware file from your computer, use {{S.fwPick}} and {{S.fwInstall}}. Take the file
  in OTA format (`.ota.bin`).
- If an OTA password is set, the card asks for the {{S.fwPassword}}.
