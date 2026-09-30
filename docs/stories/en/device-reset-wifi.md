---
id: device-reset-wifi
title: Put the gPlug back into setup mode
chapter: device
screen: captive
order: 95
mock: 2026-09-30
hardware: gPlugK 2026-09-10
---
## Story
As the owner of a gPlug that can no longer reach its Wi-Fi (new router, changed password), I want to
put it back into setup mode so that I can connect it again.

## Steps
1. Hold the button on the gPlug for at least 3 seconds.
2. The gPlug forgets its Wi-Fi credentials, restarts and opens the `gPlug-Setup` network again; the
   light blinks blue.
3. Connect it as in the first setup and tap {{C.save}}.

## Acceptance
- The gPlug is reachable in the new Wi-Fi.
- Model, meter profile, key and history are kept; the assistant does not have to be run again.
