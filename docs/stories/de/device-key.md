---
id: device-key
title: Den gespeicherten Schlüssel prüfen oder ersetzen
chapter: device
screen: device-key, wizard-meter
order: 30
mock: 2026-09-30
hardware: gPlugK 2026-09-25
---
## Story
Als Besitzer eines gPlug mit verschlüsseltem Zähler möchte ich prüfen, ob der gespeicherte Schlüssel
dem aus dem Brief meines Netzbetreibers entspricht, und ihn bei Bedarf ersetzen, ohne ihn je im
Klartext zu sehen.

## Steps
1. Im Tab {{S.tabDevice}} zeigt die Karte {{S.setupKey}}, ob der Schlüssel {{S.keySet}} oder
   {{S.keyInvalidBadge}} ist.
   {{img:device-key|Karte Schlüssel}}
2. Zum Vergleichen öffnen Sie `#setup/meter` (die Adresse des gPlug mit angehängtem `#setup/meter`).
   {{S.keepKey}} ist eingeschaltet und zeigt eine Kennung des gespeicherten Schlüssels.
3. Öffnen Sie {{S.keyCheck}}, geben Sie den Schlüssel aus dem Brief ein und tippen Sie auf
   {{S.keyCheckBtn}}.
   {{img:wizard-meter-keep|Gespeicherten Schlüssel mit dem Brief vergleichen}}
4. Um den Schlüssel zu ersetzen, schalten Sie {{S.keepKey}} aus, geben den neuen unter {{S.key}} ein
   und tippen auf {{S.next}}.

## Acceptance
- Beim Vergleich erscheint {{S.keyMatch}} oder {{S.keyMismatch}}.
- Nach dem Ersetzen zeigt die Einrichtungsprüfung {{S.checkOk}}.

## Notes
- {{S.keyNote}}
