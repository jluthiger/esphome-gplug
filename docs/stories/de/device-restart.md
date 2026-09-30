---
id: device-restart
title: Den gPlug neu starten
chapter: device
screen: device-fw
order: 60
mock: 2026-09-30
---
## Story
Als Besitzer eines gPlug möchte ich ihn aus der App neu starten, ohne den Stecker zu ziehen.

## Steps
1. Öffnen Sie im Tab {{S.tabDevice}} die Karte {{S.fwTitle}}.
2. Tippen Sie auf {{S.rsButton}} und bestätigen Sie mit {{S.rsConfirm}}.

## Acceptance
- Nach wenigen Sekunden meldet die Karte {{S.rsBack}}; die {{S.fwUptime}} beginnt von vorn.
- Das Ereignisprotokoll verzeichnet den Neustart.

## Notes
- {{S.rsHint}}
