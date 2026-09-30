---
id: device-log-memory
title: Ereignisprotokoll und Speicher ansehen
chapter: device
screen: device-log, device-mem
order: 70
mock: 2026-09-30
---
## Story
Als Besitzer eines gPlug möchte ich sehen, was auf dem Gerät passiert ist – etwa warum es neu
gestartet hat –, damit ich ein Problem melden oder selbst einordnen kann.

## Steps
1. Öffnen Sie im Tab {{S.tabDevice}} die Karte {{S.logTitle}}. Sie listet die letzten Ereignisse mit
   Zeit, zum Beispiel {{S.evBoot}}, {{S.evWifiLost}} oder {{S.evMeterLost}}.
   {{img:device-log|Ereignisprotokoll}}
2. Mit {{S.logCopy}} oder {{S.logDownload}} geben Sie das Protokoll weiter.
3. Die Karte {{S.memTitle}} zeigt {{S.memFree}}, {{S.memMin}} und {{S.memLargest}} sowie den
   Verlauf der letzten 24 Stunden.
   {{img:device-mem|Arbeitsspeicher}}

## Acceptance
- Das Protokoll bleibt über Neustarts erhalten und nennt bei einem Neustart den Grund.

## Notes
- {{S.memHint}}
