---
id: trouble-silent
title: «Nessun segnale dal contatore»
chapter: trouble
screen: diagnosis
order: 20
mock: 2026-09-30
---
## Story
Come proprietario di un gPlug che non riceve nulla dal contatore voglio sapere cosa verificare.

## Steps
1. La scheda {{S.dgLabel}} mostra {{S.dgSilent}}; in alto a destra c'è {{S.noData}}.
   {{img:diag-silent|Nessun segnale dal contatore}}
2. Verificate che il gPlug sia ben inserito nell'interfaccia cliente e che il cavo sia integro.
3. Toccate {{S.fixHardware}} e verificate che sia selezionato il modello giusto.
4. Se continua a non arrivare nulla, chiedete al gestore di rete se l'interfaccia cliente è attivata.
   Nel frattempo potete proseguire con {{S.continueAnyway}}.

## Acceptance
- Dopo la correzione la verifica della configurazione mostra {{S.checkOk}}.
