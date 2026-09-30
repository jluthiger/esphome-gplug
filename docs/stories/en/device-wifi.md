---
id: device-wifi
title: Change the Wi-Fi
chapter: device
screen: device-wifi
order: 20
mock: 2026-09-30
---
## Story
As the owner of a gPlug I want to move it to another Wi-Fi network, for example after changing my
router, without setting it up again.

## Steps
1. Tap {{S.tabDevice}} and, on the {{S.setupConn}} card, {{S.changeWifi}}.
2. You see the current connection. Tap {{S.changeWifi}} again; the gPlug searches for networks in
   range.
   {{img:device-wifi-scan|Networks found}}
3. Tap the new network, enter the {{S.password}} and tap {{S.connect}}. If the network is hidden,
   choose {{S.manual}}.
4. After {{S.connecting}}, {{S.connected}} appears with network, IP and signal.
   {{img:device-wifi-ok|Connected to the new network}}

## Acceptance
- {{S.connected}} shows the new network.
- With a wrong password, {{S.connectFailed}} appears and the gPlug stays reachable.

## Notes
- If the new network is not the one your phone is on, connect the phone to it afterwards and open the
  address again.
