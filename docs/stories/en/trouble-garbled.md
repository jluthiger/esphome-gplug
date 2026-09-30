---
id: trouble-garbled
title: “Data unreadable”
chapter: trouble
screen: diagnosis
order: 30
mock: 2026-09-30
---
## Story
As the owner of a gPlug that receives data it cannot read, I want to know which setting is wrong.

## Steps
1. The {{S.dgLabel}} card shows {{S.dgGarbled}}.
   {{img:diag-garbled|Data unreadable}}
2. Tap {{S.fixMeter}} and choose a different profile; the gPlug suggests the protocol it recognised.
3. If that does not help, check the selected model with {{S.fixHardware}}.

## Acceptance
- After the correction the setup check shows {{S.checkOk}}.
