---
id: device-key
title: Verificare o sostituire la chiave salvata
chapter: device
screen: device-key, wizard-meter
order: 30
mock: 2026-09-30
hardware: gPlugK 2026-09-25
---
## Story
Come proprietario di un gPlug con contatore cifrato voglio verificare se la chiave salvata corrisponde
a quella della lettera del mio gestore di rete, e sostituirla se necessario, senza mai vederla in
chiaro.

## Steps
1. Nella sezione {{S.tabDevice}} la scheda {{S.setupKey}} mostra se la chiave è {{S.keySet}} o
   {{S.keyInvalidBadge}}.
   {{img:device-key|Scheda Chiave}}
2. Per confrontarla, aprite `#setup/meter` (l'indirizzo del gPlug seguito da `#setup/meter`).
   {{S.keepKey}} è attivo e mostra un identificativo della chiave salvata.
3. Aprite {{S.keyCheck}}, inserite la chiave della lettera e toccate {{S.keyCheckBtn}}.
   {{img:wizard-meter-keep|Confrontare la chiave salvata con la lettera}}
4. Per sostituire la chiave, disattivate {{S.keepKey}}, inserite la nuova sotto {{S.key}} e toccate
   {{S.next}}.

## Acceptance
- Il confronto mostra {{S.keyMatch}} o {{S.keyMismatch}}.
- Dopo la sostituzione, la verifica della configurazione mostra {{S.checkOk}}.

## Notes
- {{S.keyNote}}
