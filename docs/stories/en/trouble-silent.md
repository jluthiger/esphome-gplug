---
id: trouble-silent
title: “No signals from the meter”
chapter: trouble
screen: diagnosis
order: 20
mock: 2026-09-30
---
## Story
As the owner of a gPlug that receives nothing at all from the meter, I want to know what to check.

## Steps
1. The {{S.dgLabel}} card shows {{S.dgSilent}}; the top right shows {{S.noData}}.
   {{img:diag-silent|No signals from the meter}}
2. Check that the gPlug sits firmly in the customer interface and that the cable is undamaged.
3. Tap {{S.fixHardware}} and check that the right model is selected.
4. If still nothing arrives, ask your grid operator whether the customer interface is enabled. Until
   then you can carry on with {{S.continueAnyway}}.

## Acceptance
- After the correction the setup check shows {{S.checkOk}}.
