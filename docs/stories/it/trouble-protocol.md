---
id: trouble-protocol
title: «Il profilo non corrisponde al contatore»
chapter: trouble
screen: diagnosis
order: 40
mock: 2026-09-30
---
## Story
Come proprietario di un gPlug che ha scelto un profilo con il protocollo sbagliato voglio arrivare
rapidamente al profilo giusto.

## Steps
1. La scheda {{S.dgLabel}} mostra {{S.dgProtocol}}; in alto a destra c'è {{S.dgProtocolPill}}.
   Questo messaggio arriva subito, senza attesa.
   {{img:diag-protocol|Il profilo non corrisponde al contatore}}
2. Toccate {{S.fixMeter}}. Il passo {{S.meter}} propone il profilo adatto al contatore riconosciuto.
3. Confermatelo, inserite la chiave se necessario e toccate {{S.next}}.

## Acceptance
- Dopo la correzione la verifica della configurazione mostra {{S.checkOk}}.
