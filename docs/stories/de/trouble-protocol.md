---
id: trouble-protocol
title: „Profil passt nicht zum Zähler“
chapter: trouble
screen: diagnosis
order: 40
mock: 2026-09-30
---
## Story
Als Besitzer eines gPlug, der ein Profil mit dem falschen Protokoll gewählt hat, möchte ich schnell zum
richtigen Profil kommen.

## Steps
1. Die Karte {{S.dgLabel}} zeigt {{S.dgProtocol}}; oben rechts steht {{S.dgProtocolPill}}. Diese
   Meldung kommt sofort, ohne Wartezeit.
   {{img:diag-protocol|Profil passt nicht zum Zähler}}
2. Tippen Sie auf {{S.fixMeter}}. Der Schritt {{S.meter}} schlägt das Profil vor, das zum erkannten
   Zähler passt.
3. Bestätigen Sie es, geben Sie falls nötig den Schlüssel ein und tippen Sie auf {{S.next}}.

## Acceptance
- Nach der Korrektur zeigt die Einrichtungsprüfung {{S.checkOk}}.
