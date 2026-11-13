---
id: device-tariff
title: Turn on the dynamic tariff
chapter: device
screen: device-tariff
order: 45
mock: 2026-11-13
---
## Story
As the owner of a gPlug in the BKW or CKW grid I want to see my utility's quarter-hour tariff next to
my energy, so that I know when feeding in or drawing power pays off.

## Steps
1. On the {{S.tabDevice}} tab, open the {{S.tariffTitle}} card.
2. Choose {{S.tariffBkw}} or {{S.tariffCkw}}. The choice applies at once.
3. Or choose {{S.tariffSth}} if you have an account at swisstariffhub.ch: enter {{S.tariffSthId}}
   and {{S.tariffSthKey}} and tap {{S.tariffSthSave}}.

## Acceptance
- Below the choice it says which tariff is shown: {{S.tariffKindFeedIn}} or {{S.tariffKindGrid}}
- After a few seconds it says "{{S.tariffFetched(0)}}", and the tariff card appears on the
  {{S.tabHist}} tab.
- If the gPlug cannot reach the provider, it says {{S.tariffErrNet}}
- If Swiss Tariff Hub refuses the key, it says {{S.tariffErrAuth}}
- The stored key is never shown again; the empty field says {{S.tariffSthKeySet}}.
- {{S.tariffOff}} stops fetching; the gPlug then contacts no provider at all.

## Notes
- The gPlug needs internet access for this. It fetches the tariff itself, at most once an hour while
  prices are missing, and keeps it only until the next restart.
- BKW publishes today only; the gPlug shows earlier days only if it fetched them itself. For CKW it
  fills in the last three days.
