---
id: daily-csv
title: Export the load profile as CSV
chapter: daily
screen: history-csv
order: 30
mock: 2026-09-30
---
## Story
As the owner of a gPlug I want to download the stored 15-minute values as a file, so that I can check
my electricity bill or settle within a self-consumption group.

## Steps
1. Tap {{S.tabHist}} and open the {{S.csvTitle}} card. Even closed, its row shows which period is
   stored.
2. Choose {{S.csvFrom}} and {{S.csvTo}}.
3. Tap {{S.csvDownload}}.
   {{img:history-csv|Export load profile}}

## Acceptance
- The browser saves a CSV file with meter readings, energy and power per 15 minutes, separated by
  semicolons.
- If {{S.csvFrom}} is after {{S.csvTo}}, {{S.csvOrder}} appears and nothing is downloaded.

## Notes
- {{S.csvAll}} also includes intervals without a timestamp.
