---
id: setup-port-closed
title: Proseguire anche se l'interfaccia cliente non è ancora attivata
chapter: setup
screen: wizard-done
order: 50
mock: 2026-09-30
---
## Story
Come proprietario di un gPlug il cui gestore di rete non ha ancora attivato l'interfaccia cliente,
voglio concludere comunque la configurazione, affinché il gPlug legga non appena il contatore invia.

## Steps
1. Nel passo {{S.done}}, dopo un minuto appare {{S.dgSilent}}.
2. Se siete sicuri che modello e cavo sono corretti, toccate {{S.continueAnyway}}.
   {{img:diag-silent|Nessun segnale dal contatore, con il passaggio alla vista in tempo reale}}

## Acceptance
- Si apre la vista in tempo reale e mostra la stessa scheda {{S.dgLabel}} finché arrivano dati.
  {{img:live-diag|Vista in tempo reale mentre il contatore tace}}
- Non appena il gestore di rete attiva l'interfaccia, i valori appaiono senza altri interventi.
