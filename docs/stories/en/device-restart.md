---
id: device-restart
title: Restart the gPlug
chapter: device
screen: device-fw
order: 60
mock: 2026-09-30
---
## Story
As the owner of a gPlug I want to restart it from the app without pulling the plug.

## Steps
1. On the {{S.tabDevice}} tab, open the {{S.fwTitle}} card.
2. Tap {{S.rsButton}} and confirm with {{S.rsConfirm}}.

## Acceptance
- After a few seconds the card reports {{S.rsBack}}; the {{S.fwUptime}} starts again from zero.
- The event log records the restart.

## Notes
- {{S.rsHint}}
