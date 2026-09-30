---
id: trouble-waiting
title: «In attesa dei primi dati» non scompare
chapter: trouble
screen: wizard-done
order: 10
mock: 2026-09-30
---
## Story
Come proprietario di un gPlug voglio sapere quanto può durare la verifica, per non cambiare nulla
troppo presto.

## Steps
1. Dopo il salvataggio del profilo del contatore, {{S.checkTitle}} mostra {{S.checkWaiting}} con un
   contatore dei secondi.
   {{img:wizard-done-waiting|La verifica è in attesa}}
2. Attendete fino a un minuto. Alcuni contatori inviano solo ogni 10–30 secondi.

## Acceptance
- Al più tardi dopo un minuto appare {{S.checkOk}} oppure la scheda {{S.dgLabel}} con uno dei
  messaggi di questo capitolo.
