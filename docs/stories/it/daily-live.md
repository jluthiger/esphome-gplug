---
id: daily-live
title: Leggere la potenza attuale e le letture del contatore
chapter: daily
screen: live
order: 10
mock: 2026-09-30
hardware: gPlugK 2026-09-14
---
## Story
Come proprietario di un gPlug voglio vedere a colpo d'occhio quanta corrente sto prelevando o
immettendo in questo momento, per capire il mio consumo.

## Steps
1. Aprite l'indirizzo del gPlug. L'app si avvia nella scheda {{S.tabLive}}.
2. In alto c'è la {{S.activePower}} in kW con la direzione: {{S.drawFromGrid}} o {{S.feedToGrid}}.
3. Il grafico sottostante mostra l'ultima ora ({{S.ago60}} fino a {{S.now}}). Toccate o passate
   sopra il grafico per leggere un momento preciso.
4. Le schede {{S.importLbl}} e {{S.exportLbl}} mostrano le letture in kWh, come le indica il
   contatore.
5. Con i profili trifase, {{S.phases}} mostra potenza, tensione e corrente per fase.
   {{img:live|Vista in tempo reale con potenza, letture e fasi}}

## Acceptance
- In alto a destra c'è {{S.dataOk}}; i valori si aggiornano ogni pochi secondi.
- Le letture corrispondono al display del contatore.

## Notes
- Dopo un riavvio del gPlug, la parte del grafico precedente al riavvio proviene dalla memoria del
  dispositivo (una media ogni 15 minuti).
