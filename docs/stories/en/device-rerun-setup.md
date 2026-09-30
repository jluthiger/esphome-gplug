---
id: device-rerun-setup
title: Open the setup assistant again
chapter: device
screen: wizard-hardware, wizard-meter
order: 90
mock: 2026-09-30
---
## Story
As the owner of a gPlug I want to change the model or meter profile later, for example after the meter
has been replaced, without setting everything up again.

## Steps
1. Append one of the following to the gPlug's address and open it:
   - `#setup` – the whole assistant from {{S.welcome}}
   - `#setup/hardware` – straight to the {{S.hardware}} step
   - `#setup/meter` – straight to the {{S.meter}} step
   - `#setup/key` – to the {{S.meter}} step, with an empty key field
2. Change what is needed and continue with {{S.next}} up to {{S.done}}.

## Acceptance
- The setup check shows {{S.checkOk}}; the history is kept.

## Notes
- The buttons on the {{S.dgLabel}} card lead to exactly these addresses.
