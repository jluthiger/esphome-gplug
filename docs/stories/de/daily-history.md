---
id: daily-history
title: Energie pro Tag, Woche, Monat und Jahr ansehen
chapter: daily
screen: history, history-regs
order: 20
mock: 2026-09-30
---
## Story
Als Besitzer eines gPlug möchte ich sehen, wie viel Energie ich über einen Tag, eine Woche, einen
Monat oder ein Jahr bezogen und eingespeist habe, damit ich Zeiträume vergleichen kann.

## Steps
1. Tippen Sie unten auf {{S.tabHist}}.
2. Wählen Sie oben den Zeitraum: {{S.rangeDay}}, {{S.rangeWeek}}, {{S.rangeMonth}} oder
   {{S.rangeYear}}.
3. Die Grafik {{S.energyPerBucket}} zeigt Bezug nach oben und Einspeisung nach unten. Darunter
   stehen die Summen {{S.statImport}}, {{S.statExport}} und {{S.statSum}}.
   {{img:history|Verlauf eines Tages mit Summen}}
4. Tippen Sie auf {{S.registers}}, um alle aktuellen Werte des Zählers mit ihrem OBIS-Code zu sehen.
   {{img:history-regs|Liste der aktuellen Register}}

## Acceptance
- Jeder Zeitraum zeigt eine Grafik und die drei Summen.
- {{S.statSum}} ist Bezug minus Einspeisung.

## Notes
- Der gPlug speichert alle 15 Minuten einen Wert. Er hält etwa ein Jahr und behält ihn auch bei
  einem Neustart oder Firmware-Update.
- {{S.timeEstimated}} erscheint, wenn Werte aufgezeichnet wurden, bevor der gPlug die Uhrzeit aus dem
  Internet hatte.
