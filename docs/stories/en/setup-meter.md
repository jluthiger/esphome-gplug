---
id: setup-meter
title: Confirm the meter profile and enter the key
chapter: setup
screen: wizard-meter
order: 30
mock: 2026-09-30
hardware: gPlugK 2026-09-14
---
## Story
As the owner of a gPlug I want to choose the matching meter profile and store my grid operator's key
so that the gPlug can read my meter's values.

## Steps
1. In the {{S.meter}} step the gPlug first listens to what your meter sends. After a few seconds the
   {{S.detectTitle}} card shows {{S.detectFound}} and the protocol it recognised.
2. If exactly one profile fits, it is already selected. If several fit, choose your grid operator's
   profile. {{S.otherProfile}} shows more profiles.
3. If the meter sends encrypted data, enter the key (GUEK) under {{S.key}}: 32 characters from 0–9
   and A–F, as in your grid operator's letter.
   {{img:wizard-meter|Detected meter, proposed profile and key field}}
4. {{S.values}} lists the values the profile reads.
5. Tap {{S.next}}.

## Acceptance
- {{S.next}} only becomes active once a profile is chosen and the key is valid.
- The assistant moves on to {{S.done}} and checks the connection to the meter there.

## Notes
- The key is stored on the gPlug only and never shown again.
- If there is still no signal after half a minute, check the cable. Some grid operators have to
  enable the customer interface first.
