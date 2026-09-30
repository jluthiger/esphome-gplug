---
id: trouble-key
title: „Schlüssel passt nicht“
chapter: trouble
screen: diagnosis, device-key
order: 60
mock: 2026-09-30
---
## Story
Als Besitzer eines gPlug mit verschlüsseltem Zähler, dessen Daten sich nicht entschlüsseln lassen,
möchte ich den richtigen Schlüssel eingeben.

## Steps
1. Die Karte {{S.dgLabel}} zeigt {{S.dgKey}}; oben rechts steht {{S.keyWrong}}. Im Tab
   {{S.tabDevice}} zeigt die Karte {{S.setupKey}} {{S.keyInvalidBadge}}.
   {{img:diag-key|Schlüssel passt nicht}}
2. Tippen Sie auf {{S.fixKey}}. Das Schlüsselfeld ist leer.
3. Geben Sie den Schlüssel (GUEK) aus dem Brief Ihres Netzbetreibers ein – 32 Zeichen, ohne
   Leerzeichen – und tippen Sie auf {{S.next}}.

## Acceptance
- Nach der Korrektur zeigt die Einrichtungsprüfung {{S.checkOk}}, und die Karte {{S.setupKey}} zeigt
  {{S.keySet}}.

## Notes
- Verwechseln Sie den GUEK nicht mit anderen Schlüsseln im Brief (etwa dem Authentisierungsschlüssel).
