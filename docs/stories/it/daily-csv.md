---
id: daily-csv
title: Esportare la curva di carico in CSV
chapter: daily
screen: history-csv
order: 30
mock: 2026-09-30
---
## Story
Come proprietario di un gPlug voglio scaricare i valori quartorari salvati come file, per verificare
la bolletta elettrica o fare il conteggio in un raggruppamento ai fini del consumo proprio.

## Steps
1. Toccate {{S.tabHist}} e aprite la scheda {{S.csvTitle}}. Anche chiusa, la sua riga mostra il
   periodo salvato.
2. Scegliete {{S.csvFrom}} e {{S.csvTo}}.
3. Toccate {{S.csvDownload}}.
   {{img:history-csv|Esportare la curva di carico}}

## Acceptance
- Il browser salva un file CSV con letture, energia e potenza per quarto d'ora, separati da punto e
  virgola.
- Se {{S.csvFrom}} è dopo {{S.csvTo}}, appare {{S.csvOrder}} e non viene scaricato nulla.

## Notes
- {{S.csvAll}} include anche gli intervalli senza marca temporale.
