---
id: trouble-silent
title: „Keine Signale vom Zähler“
chapter: trouble
screen: diagnosis
order: 20
mock: 2026-09-30
---
## Story
Als Besitzer eines gPlug, bei dem gar nichts vom Zähler ankommt, möchte ich wissen, was ich prüfen
muss.

## Steps
1. Die Karte {{S.dgLabel}} zeigt {{S.dgSilent}}; oben rechts steht {{S.noData}}.
   {{img:diag-silent|Keine Signale vom Zähler}}
2. Prüfen Sie, ob der gPlug fest in der Kundenschnittstelle steckt und das Kabel unbeschädigt ist.
3. Tippen Sie auf {{S.fixHardware}} und prüfen Sie, ob das richtige Modell gewählt ist.
4. Kommt weiterhin nichts an, fragen Sie Ihren Netzbetreiber, ob die Kundenschnittstelle
   freigeschaltet ist. Bis dahin können Sie mit {{S.continueAnyway}} weitermachen.

## Acceptance
- Nach der Korrektur zeigt die Einrichtungsprüfung {{S.checkOk}}.
