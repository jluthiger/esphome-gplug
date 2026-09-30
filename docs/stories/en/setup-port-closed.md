---
id: setup-port-closed
title: Continue although the customer interface is not enabled yet
chapter: setup
screen: wizard-done
order: 50
mock: 2026-09-30
---
## Story
As the owner of a gPlug whose grid operator has not enabled the customer interface yet, I want to
finish the setup anyway so that the gPlug starts reading as soon as the meter sends.

## Steps
1. In the {{S.done}} step, {{S.dgSilent}} appears after a minute.
2. If you are sure that the model and the cable are right, tap {{S.continueAnyway}}.
   {{img:diag-silent|No signals from the meter, with the way to the live view}}

## Acceptance
- The live view opens and shows the same {{S.dgLabel}} card until data arrives.
  {{img:live-diag|Live view while the meter is silent}}
- As soon as the grid operator enables the interface, the values appear without anything else to do.
