---
id: device-connection
title: Adresse und Verbindung des gPlug nachsehen
chapter: device
screen: device-conn
order: 10
mock: 2026-09-30
---
## Story
Als Besitzer eines gPlug möchte ich nachsehen, unter welcher Adresse er erreichbar ist und wie gut
sein WLAN-Empfang ist, damit ich ihn wiederfinde und einen guten Standort wähle.

## Steps
1. Tippen Sie unten auf {{S.tabDevice}}. Die Karte {{S.setupConn}} ist offen.
2. Sie zeigt {{S.host}} (die Adresse mit `.local`), {{S.ip}}, das {{S.wlan}} mit Signalstärke
   und das gewählte {{S.profile}}.
   {{img:device|Geräte-Tab mit Verbindung und Schlüssel}}

## Acceptance
- Die angezeigte Adresse öffnet die App auch auf einem anderen Gerät im selben WLAN.

## Notes
- Die Signalstärke ist in dBm angegeben: −50 ist sehr gut, unter −80 wird die Verbindung unzuverlässig.
- Jede Karte im Tab {{S.tabDevice}} lässt sich mit einem Tipp auf ihren Titel auf- und zuklappen; der
  Browser merkt sich das.
