---
id: setup-check
title: Capire la verifica della configurazione
chapter: setup
screen: wizard-done, diagnosis
order: 40
mock: 2026-09-30
---
## Story
Come proprietario di un gPlug voglio vedere alla fine della configurazione se il contatore viene
davvero letto, per correggere un errore mentre sono ancora nella procedura.

## Steps
1. Nel passo {{S.done}} la scheda {{S.checkTitle}} mostra prima {{S.checkWaiting}}. A seconda del
   contatore ci vuole fino a un minuto.
   {{img:wizard-done-waiting|La verifica attende i primi dati}}
2. Quando arrivano dati, appare {{S.checkOk}} con il numero del contatore, la potenza attuale e il
   numero di valori. Toccate {{S.openLive}}.
   {{img:wizard-done-ok|Dati ricevuti}}
3. Se non arrivano dati utilizzabili, appare invece la scheda {{S.dgLabel}} con una spiegazione e un
   pulsante che vi riporta direttamente al passo giusto, per esempio {{S.fixMeter}}. Il significato
   di ogni messaggio è spiegato nel capitolo *Quando qualcosa non va*.

## Acceptance
- Dopo {{S.openLive}} vedete la vista in tempo reale con {{S.dataOk}}.
- Dopo un pulsante della scheda {{S.dgLabel}} siete nel passo giusto della procedura; le altre
  impostazioni restano.
