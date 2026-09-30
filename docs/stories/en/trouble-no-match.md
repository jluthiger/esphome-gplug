---
id: trouble-no-match
title: “No matching values”
chapter: trouble
screen: diagnosis
order: 50
mock: 2026-09-30
---
## Story
As the owner of a gPlug whose meter sends readable data that contains none of the expected values, I
want to find the profile that fits my meter.

## Steps
1. The {{S.dgLabel}} card shows {{S.dgNoMatch}}; the top right shows {{S.dgNoMatchPill}}.
   {{img:diag-no-match|No matching values}}
2. Tap {{S.fixMeter}} and choose your grid operator's profile. {{S.showAll}} shows every profile.

## Acceptance
- After the correction the setup check shows {{S.checkOk}}.

## Notes
- The {{S.tabStream}} tab shows what the meter actually sends.
