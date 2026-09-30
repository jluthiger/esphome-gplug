---
id: daily-history
title: View energy per day, week, month and year
chapter: daily
screen: history, history-regs
order: 20
mock: 2026-09-30
---
## Story
As the owner of a gPlug I want to see how much energy I imported and exported over a day, a week, a
month or a year, so that I can compare periods.

## Steps
1. Tap {{S.tabHist}} at the bottom.
2. Choose the period at the top: {{S.rangeDay}}, {{S.rangeWeek}}, {{S.rangeMonth}} or
   {{S.rangeYear}}.
3. The {{S.energyPerBucket}} chart shows import upwards and export downwards. Below it are the
   totals {{S.statImport}}, {{S.statExport}} and {{S.statSum}}.
   {{img:history|History of one day with totals}}
4. Tap {{S.registers}} to see all current values of the meter with their OBIS codes.
   {{img:history-regs|List of the current registers}}

## Acceptance
- Every period shows a chart and the three totals.
- {{S.statSum}} is import minus export.

## Notes
- The gPlug stores one value every 15 minutes. It keeps about a year and keeps it across restarts and
  firmware updates.
- {{S.timeEstimated}} appears when values were recorded before the gPlug had the time from the
  internet.
