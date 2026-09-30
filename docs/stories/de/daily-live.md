---
id: daily-live
title: Aktuelle Leistung und Zählerstände ablesen
chapter: daily
screen: live
order: 10
mock: 2026-09-30
hardware: gPlugK 2026-09-14
---
## Story
Als Besitzer eines gPlug möchte ich auf einen Blick sehen, wie viel Strom ich gerade beziehe oder
einspeise, damit ich meinen Verbrauch verstehe.

## Steps
1. Öffnen Sie die Adresse des gPlug. Die App startet im Tab {{S.tabLive}}.
2. Oben steht die {{S.activePower}} in kW mit der Richtung: {{S.drawFromGrid}} oder
   {{S.feedToGrid}}.
3. Die Grafik darunter zeigt die letzte Stunde ({{S.ago60}} bis {{S.now}}). Tippen oder fahren Sie
   über die Grafik, um einen Zeitpunkt abzulesen.
4. Die Karten {{S.importLbl}} und {{S.exportLbl}} zeigen die Zählerstände in kWh, wie sie der
   Zähler anzeigt.
5. Bei dreiphasigen Profilen zeigt {{S.phases}} Leistung, Spannung und Strom pro Phase.
   {{img:live|Live-Ansicht mit Leistung, Zählerständen und Phasen}}

## Acceptance
- Oben rechts steht {{S.dataOk}}; die Werte erneuern sich alle paar Sekunden.
- Die Zählerstände stimmen mit der Anzeige am Zähler überein.

## Notes
- Nach einem Neustart des gPlug stammt der Teil der Grafik vor dem Neustart aus dem Gerätespeicher
  (ein Mittelwert je 15 Minuten).
