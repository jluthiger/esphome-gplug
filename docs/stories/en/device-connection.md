---
id: device-connection
title: Look up the gPlug's address and connection
chapter: device
screen: device-conn
order: 10
mock: 2026-09-30
---
## Story
As the owner of a gPlug I want to look up the address it is reachable at and how good its Wi-Fi
reception is, so that I can find it again and choose a good spot for it.

## Steps
1. Tap {{S.tabDevice}} at the bottom. The {{S.setupConn}} card is open.
2. It shows the {{S.host}} (the address ending in `.local`), the {{S.ip}}, the {{S.wlan}} with its
   signal strength and the selected {{S.profile}}.
   {{img:device|Device tab with connection and key}}

## Acceptance
- The address shown also opens the app on another device in the same Wi-Fi.

## Notes
- The signal strength is given in dBm: −50 is very good, below −80 the connection becomes unreliable.
- Every card on the {{S.tabDevice}} tab opens and closes with a tap on its title; the browser
  remembers your choice.
