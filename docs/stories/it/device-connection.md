---
id: device-connection
title: Consultare l'indirizzo e la connessione del gPlug
chapter: device
screen: device-conn
order: 10
mock: 2026-09-30
---
## Story
Come proprietario di un gPlug voglio sapere a quale indirizzo è raggiungibile e quanto è buona la sua
ricezione Wi-Fi, per ritrovarlo e scegliergli una buona posizione.

## Steps
1. Toccate {{S.tabDevice}} in basso. La scheda {{S.setupConn}} è aperta.
2. Mostra {{S.host}} (l'indirizzo con `.local`), {{S.ip}}, il {{S.wlan}} con l'intensità del
   segnale e il {{S.profile}} scelto.
   {{img:device|Scheda Dispositivo con connessione e chiave}}

## Acceptance
- L'indirizzo mostrato apre l'app anche su un altro dispositivo nello stesso Wi-Fi.

## Notes
- L'intensità del segnale è indicata in dBm: −50 è molto buona, sotto −80 la connessione diventa
  inaffidabile.
- Ogni scheda della sezione {{S.tabDevice}} si apre e si chiude toccandone il titolo; il browser se
  lo ricorda.
