---
id: device-wifi
title: Cambiare il Wi-Fi
chapter: device
screen: device-wifi
order: 20
mock: 2026-09-30
---
## Story
Come proprietario di un gPlug voglio spostarlo su un'altra rete Wi-Fi, per esempio dopo aver cambiato
router, senza configurarlo di nuovo.

## Steps
1. Toccate {{S.tabDevice}} e, nella scheda {{S.setupConn}}, {{S.changeWifi}}.
2. Vedete la connessione attuale. Toccate di nuovo {{S.changeWifi}}; il gPlug cerca le reti nelle
   vicinanze.
   {{img:device-wifi-scan|Reti trovate}}
3. Toccate la nuova rete, inserite la {{S.password}} e toccate {{S.connect}}. Se la rete è
   nascosta, scegliete {{S.manual}}.
4. Dopo {{S.connecting}} appare {{S.connected}} con rete, IP e segnale.
   {{img:device-wifi-ok|Collegato alla nuova rete}}

## Acceptance
- {{S.connected}} mostra la nuova rete.
- Con una password errata appare {{S.connectFailed}} e il gPlug resta raggiungibile.

## Notes
- Se la nuova rete non è quella del vostro telefono, collegate poi il telefono a quella rete e
  riaprite l'indirizzo.
