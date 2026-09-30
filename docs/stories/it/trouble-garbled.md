---
id: trouble-garbled
title: «Dati illeggibili»
chapter: trouble
screen: diagnosis
order: 30
mock: 2026-09-30
---
## Story
Come proprietario di un gPlug che riceve dati che non riesce a leggere voglio sapere quale
impostazione è sbagliata.

## Steps
1. La scheda {{S.dgLabel}} mostra {{S.dgGarbled}}.
   {{img:diag-garbled|Dati illeggibili}}
2. Toccate {{S.fixMeter}} e scegliete un altro profilo; il gPlug propone il protocollo riconosciuto.
3. Se non basta, verificate il modello scelto con {{S.fixHardware}}.

## Acceptance
- Dopo la correzione la verifica della configurazione mostra {{S.checkOk}}.
