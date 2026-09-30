---
id: trouble-protocol
title: “Profile does not match the meter”
chapter: trouble
screen: diagnosis
order: 40
mock: 2026-09-30
---
## Story
As the owner of a gPlug who chose a profile with the wrong protocol, I want to get to the right profile
quickly.

## Steps
1. The {{S.dgLabel}} card shows {{S.dgProtocol}}; the top right shows {{S.dgProtocolPill}}. This
   message comes at once, without waiting.
   {{img:diag-protocol|Profile does not match the meter}}
2. Tap {{S.fixMeter}}. The {{S.meter}} step suggests the profile that fits the detected meter.
3. Confirm it, enter the key if needed and tap {{S.next}}.

## Acceptance
- After the correction the setup check shows {{S.checkOk}}.
