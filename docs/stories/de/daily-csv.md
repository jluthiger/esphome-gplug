---
id: daily-csv
title: Den Lastgang als CSV exportieren
chapter: daily
screen: history-csv
order: 30
mock: 2026-09-30
---
## Story
Als Besitzer eines gPlug möchte ich die gespeicherten 15-Minuten-Werte als Datei herunterladen, damit
ich die Stromrechnung prüfen oder in einem Zusammenschluss zum Eigenverbrauch abrechnen kann.

## Steps
1. Tippen Sie auf {{S.tabHist}} und öffnen Sie die Karte {{S.csvTitle}}. Die Zeile zeigt schon
   geschlossen, welcher Zeitraum gespeichert ist.
2. Wählen Sie {{S.csvFrom}} und {{S.csvTo}}.
3. Tippen Sie auf {{S.csvDownload}}.
   {{img:history-csv|Lastgang exportieren}}

## Acceptance
- Der Browser speichert eine CSV-Datei mit Zählerständen, Energie und Leistung je 15 Minuten,
  getrennt durch Semikolon.
- Liegt {{S.csvFrom}} nach {{S.csvTo}}, erscheint {{S.csvOrder}} und es wird nichts heruntergeladen.

## Notes
- {{S.csvAll}} nimmt auch Intervalle ohne Zeitstempel mit.
