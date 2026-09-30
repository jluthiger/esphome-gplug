---
id: trouble-no-match
title: „Keine passenden Werte“
chapter: trouble
screen: diagnosis
order: 50
mock: 2026-09-30
---
## Story
Als Besitzer eines gPlug, dessen Zähler lesbare Daten sendet, in denen aber keiner der erwarteten Werte
steht, möchte ich das Profil finden, das zu meinem Zähler passt.

## Steps
1. Die Karte {{S.dgLabel}} zeigt {{S.dgNoMatch}}; oben rechts steht {{S.dgNoMatchPill}}.
   {{img:diag-no-match|Keine passenden Werte}}
2. Tippen Sie auf {{S.fixMeter}} und wählen Sie das Profil Ihres Netzbetreibers. Mit
   {{S.showAll}} sehen Sie alle Profile.

## Acceptance
- Nach der Korrektur zeigt die Einrichtungsprüfung {{S.checkOk}}.

## Notes
- Im Tab {{S.tabStream}} sehen Sie, was der Zähler tatsächlich sendet.
