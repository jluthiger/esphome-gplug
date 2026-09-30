---
id: trouble-no-match
title: «Nessun valore corrispondente»
chapter: trouble
screen: diagnosis
order: 50
mock: 2026-09-30
---
## Story
Come proprietario di un gPlug il cui contatore invia dati leggibili che non contengono nessuno dei
valori attesi, voglio trovare il profilo adatto al mio contatore.

## Steps
1. La scheda {{S.dgLabel}} mostra {{S.dgNoMatch}}; in alto a destra c'è {{S.dgNoMatchPill}}.
   {{img:diag-no-match|Nessun valore corrispondente}}
2. Toccate {{S.fixMeter}} e scegliete il profilo del vostro gestore di rete. {{S.showAll}} mostra
   tutti i profili.

## Acceptance
- Dopo la correzione la verifica della configurazione mostra {{S.checkOk}}.

## Notes
- La sezione {{S.tabStream}} mostra cosa invia davvero il contatore.
