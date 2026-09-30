---
id: trouble-garbled
title: „Daten nicht lesbar“
chapter: trouble
screen: diagnosis
order: 30
mock: 2026-09-30
---
## Story
Als Besitzer eines gPlug, bei dem Daten ankommen, aber nicht gelesen werden können, möchte ich wissen,
welche Einstellung falsch ist.

## Steps
1. Die Karte {{S.dgLabel}} zeigt {{S.dgGarbled}}.
   {{img:diag-garbled|Daten nicht lesbar}}
2. Tippen Sie auf {{S.fixMeter}} und wählen Sie ein anderes Profil; der gPlug schlägt das erkannte
   Protokoll vor.
3. Hilft das nicht, prüfen Sie mit {{S.fixHardware}} das gewählte Modell.

## Acceptance
- Nach der Korrektur zeigt die Einrichtungsprüfung {{S.checkOk}}.
