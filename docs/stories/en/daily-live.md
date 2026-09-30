---
id: daily-live
title: Read the current power and the meter readings
chapter: daily
screen: live
order: 10
mock: 2026-09-30
hardware: gPlugK 2026-09-14
---
## Story
As the owner of a gPlug I want to see at a glance how much electricity I am drawing or feeding in
right now, so that I understand my consumption.

## Steps
1. Open the gPlug's address. The app starts on the {{S.tabLive}} tab.
2. At the top is the {{S.activePower}} in kW with its direction: {{S.drawFromGrid}} or
   {{S.feedToGrid}}.
3. The chart below shows the last hour ({{S.ago60}} to {{S.now}}). Tap or hover over the chart to
   read a point in time.
4. The {{S.importLbl}} and {{S.exportLbl}} cards show the meter readings in kWh, as the meter
   displays them.
5. With three-phase profiles, {{S.phases}} shows power, voltage and current per phase.
   {{img:live|Live view with power, meter readings and phases}}

## Acceptance
- {{S.dataOk}} is shown at the top right; the values update every few seconds.
- The meter readings match the meter's display.

## Notes
- After the gPlug restarts, the part of the chart before the restart comes from the device's storage
  (one average per 15 minutes).
