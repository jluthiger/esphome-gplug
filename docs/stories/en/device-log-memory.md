---
id: device-log-memory
title: View the event log and memory
chapter: device
screen: device-log, device-mem
order: 70
mock: 2026-09-30
---
## Story
As the owner of a gPlug I want to see what happened on the device – such as why it restarted – so that
I can report a problem or make sense of it myself.

## Steps
1. On the {{S.tabDevice}} tab, open the {{S.logTitle}} card. It lists the last events with their
   time, for example {{S.evBoot}}, {{S.evWifiLost}} or {{S.evMeterLost}}.
   {{img:device-log|Event log}}
2. {{S.logCopy}} or {{S.logDownload}} lets you pass the log on.
3. The {{S.memTitle}} card shows {{S.memFree}}, {{S.memMin}} and {{S.memLargest}}, and the trend of
   the last 24 hours.
   {{img:device-mem|Memory}}

## Acceptance
- The log is kept across restarts and gives the reason for each restart.

## Notes
- {{S.memHint}}
