---
id: device-restart
title: Riavviare il gPlug
chapter: device
screen: device-fw
order: 60
mock: 2026-09-30
---
## Story
Come proprietario di un gPlug voglio riavviarlo dall'app senza staccare la spina.

## Steps
1. Nella sezione {{S.tabDevice}} aprite la scheda {{S.fwTitle}}.
2. Toccate {{S.rsButton}} e confermate con {{S.rsConfirm}}.

## Acceptance
- Dopo pochi secondi la scheda segnala {{S.rsBack}}; il {{S.fwUptime}} riparte da zero.
- Il registro eventi annota il riavvio.

## Notes
- {{S.rsHint}}
