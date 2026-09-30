---
id: device-rerun-setup
title: Riaprire la procedura di configurazione
chapter: device
screen: wizard-hardware, wizard-meter
order: 90
mock: 2026-09-30
---
## Story
Come proprietario di un gPlug voglio cambiare in seguito il modello o il profilo del contatore, per
esempio dopo la sostituzione del contatore, senza configurare tutto da capo.

## Steps
1. Aggiungete all'indirizzo del gPlug una delle seguenti parti e apritelo:
   - `#setup` – tutta la procedura da {{S.welcome}}
   - `#setup/hardware` – direttamente al passo {{S.hardware}}
   - `#setup/meter` – direttamente al passo {{S.meter}}
   - `#setup/key` – al passo {{S.meter}}, con il campo della chiave vuoto
2. Modificate ciò che serve e proseguite con {{S.next}} fino a {{S.done}}.

## Acceptance
- La verifica della configurazione mostra {{S.checkOk}}; lo storico resta.

## Notes
- I pulsanti della scheda {{S.dgLabel}} portano esattamente a questi indirizzi.
