---
id: device-key
title: Check or replace the stored key
chapter: device
screen: device-key, wizard-meter
order: 30
mock: 2026-09-30
hardware: gPlugK 2026-09-25
---
## Story
As the owner of a gPlug with an encrypted meter I want to check whether the stored key matches the one
in my grid operator's letter, and replace it if needed, without ever seeing it in plain text.

## Steps
1. On the {{S.tabDevice}} tab the {{S.setupKey}} card shows whether the key is {{S.keySet}} or
   {{S.keyInvalidBadge}}.
   {{img:device-key|Key card}}
2. To compare, open `#setup/meter` (the gPlug's address with `#setup/meter` appended).
   {{S.keepKey}} is switched on and shows an ID of the stored key.
3. Open {{S.keyCheck}}, enter the key from the letter and tap {{S.keyCheckBtn}}.
   {{img:wizard-meter-keep|Comparing the stored key with the letter}}
4. To replace the key, switch {{S.keepKey}} off, enter the new one under {{S.key}} and tap
   {{S.next}}.

## Acceptance
- The comparison shows {{S.keyMatch}} or {{S.keyMismatch}}.
- After replacing it, the setup check shows {{S.checkOk}}.

## Notes
- {{S.keyNote}}
