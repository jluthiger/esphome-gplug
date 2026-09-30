---
id: trouble-waiting
title: „Warte auf erste Daten“ bleibt stehen
chapter: trouble
screen: wizard-done
order: 10
mock: 2026-09-30
---
## Story
Als Besitzer eines gPlug möchte ich wissen, wie lange die Prüfung warten darf, damit ich nicht zu
früh etwas ändere.

## Steps
1. Nach dem Speichern des Zählerprofils zeigt {{S.checkTitle}} {{S.checkWaiting}} mit einem
   Sekundenzähler.
   {{img:wizard-done-waiting|Die Prüfung wartet}}
2. Warten Sie bis zu einer Minute. Manche Zähler senden nur alle 10 bis 30 Sekunden.

## Acceptance
- Spätestens nach einer Minute erscheint {{S.checkOk}} oder die Karte {{S.dgLabel}} mit einer der
  Meldungen dieses Kapitels.
