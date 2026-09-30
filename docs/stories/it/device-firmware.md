---
id: device-firmware
title: Aggiornare il firmware
chapter: device
screen: device-fw
order: 50
mock: 2026-09-30
hardware: gPlugK 2026-09-25
---
## Story
Come proprietario di un gPlug voglio installare un nuovo firmware senza collegare un cavo, affinché il
mio dispositivo riceva correzioni e nuove funzioni.

## Steps
1. Nella sezione {{S.tabDevice}} aprite la scheda {{S.fwTitle}}. Mostra la versione installata.
2. Toccate {{S.fwRelCheck}}. Se esiste una versione più recente, appare una riga come
   {{S.fwRelAvail("0.6.0", "0.7.0")}} con il link {{S.fwRelNotes}}.
   {{img:device-fw|È disponibile un aggiornamento}}
3. Toccate {{S.fwRelInstall("0.7.0")}} e confermate con {{S.fwRelConfirm}}.
   {{img:device-fw-confirm|Confermare l'installazione}}
4. Il gPlug scarica da solo il firmware e si riavvia. Lasciatelo alimentato.
   {{img:device-fw-done|Firmware aggiornato}}

## Acceptance
- La scheda mostra {{S.fwDone}} e la nuova versione.
- Impostazioni e storico restano; si perde solo il quarto d'ora in corso.

## Notes
- Il gPlug non cerca mai aggiornamenti da solo, solo quando toccate {{S.fwRelCheck}}.
- Per installare un file dal computer usate {{S.fwPick}} e {{S.fwInstall}}. Prendete il file in
  formato OTA (`.ota.bin`).
- Se è impostata una password OTA, la scheda chiede la {{S.fwPassword}}.
