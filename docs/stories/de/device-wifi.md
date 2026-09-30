---
id: device-wifi
title: Das WLAN wechseln
chapter: device
screen: device-wifi
order: 20
mock: 2026-09-30
---
## Story
Als Besitzer eines gPlug möchte ich ihn in ein anderes WLAN bringen, zum Beispiel nach einem
Routerwechsel, ohne ihn neu einzurichten.

## Steps
1. Tippen Sie auf {{S.tabDevice}} und in der Karte {{S.setupConn}} auf {{S.changeWifi}}.
2. Sie sehen die aktuelle Verbindung. Tippen Sie nochmals auf {{S.changeWifi}}; der gPlug sucht die
   Netze in Reichweite.
   {{img:device-wifi-scan|Gefundene Netze}}
3. Tippen Sie auf das neue Netz, geben Sie das {{S.password}} ein und tippen Sie auf {{S.connect}}.
   Ist das Netz verborgen, wählen Sie {{S.manual}}.
4. Nach {{S.connecting}} erscheint {{S.connected}} mit Netz, IP und Signal.
   {{img:device-wifi-ok|Mit dem neuen Netz verbunden}}

## Acceptance
- {{S.connected}} zeigt das neue Netz.
- Bei falschem Passwort erscheint {{S.connectFailed}} und der gPlug bleibt erreichbar.

## Notes
- Liegt das neue Netz in einem anderen WLAN als Ihr Telefon, verbinden Sie das Telefon danach mit
  diesem Netz und öffnen die Adresse erneut.
