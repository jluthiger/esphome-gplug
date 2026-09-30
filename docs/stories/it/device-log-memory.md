---
id: device-log-memory
title: Consultare il registro eventi e la memoria
chapter: device
screen: device-log, device-mem
order: 70
mock: 2026-09-30
---
## Story
Come proprietario di un gPlug voglio vedere cosa è successo sul dispositivo – per esempio perché si è
riavviato – per segnalare un problema o capirlo da solo.

## Steps
1. Nella sezione {{S.tabDevice}} aprite la scheda {{S.logTitle}}. Elenca gli ultimi eventi con l'ora,
   per esempio {{S.evBoot}}, {{S.evWifiLost}} o {{S.evMeterLost}}.
   {{img:device-log|Registro eventi}}
2. Con {{S.logCopy}} o {{S.logDownload}} potete trasmettere il registro.
3. La scheda {{S.memTitle}} mostra {{S.memFree}}, {{S.memMin}} e {{S.memLargest}}, oltre
   all'andamento delle ultime 24 ore.
   {{img:device-mem|Memoria}}

## Acceptance
- Il registro resta dopo un riavvio e indica il motivo di ogni riavvio.

## Notes
- {{S.memHint}}
