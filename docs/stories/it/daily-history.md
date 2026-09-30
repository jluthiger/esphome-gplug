---
id: daily-history
title: Vedere l'energia per giorno, settimana, mese e anno
chapter: daily
screen: history, history-regs
order: 20
mock: 2026-09-30
---
## Story
Come proprietario di un gPlug voglio vedere quanta energia ho prelevato e immesso in un giorno, una
settimana, un mese o un anno, per confrontare i periodi.

## Steps
1. Toccate {{S.tabHist}} in basso.
2. Scegliete il periodo in alto: {{S.rangeDay}}, {{S.rangeWeek}}, {{S.rangeMonth}} o
   {{S.rangeYear}}.
3. Il grafico {{S.energyPerBucket}} mostra il prelievo verso l'alto e l'immissione verso il basso.
   Sotto ci sono i totali {{S.statImport}}, {{S.statExport}} e {{S.statSum}}.
   {{img:history|Storico di un giorno con i totali}}
4. Toccate {{S.registers}} per vedere tutti i valori attuali del contatore con il loro codice OBIS.
   {{img:history-regs|Elenco dei registri attuali}}

## Acceptance
- Ogni periodo mostra un grafico e i tre totali.
- {{S.statSum}} è il prelievo meno l'immissione.

## Notes
- Il gPlug salva un valore ogni 15 minuti. Ne conserva circa un anno, anche dopo un riavvio o un
  aggiornamento del firmware.
- {{S.timeEstimated}} appare quando dei valori sono stati registrati prima che il gPlug ricevesse
  l'ora da internet.
