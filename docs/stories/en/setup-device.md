---
id: setup-device
title: Start the assistant and choose the device
chapter: setup
screen: wizard-welcome, wizard-hardware
order: 20
mock: 2026-09-30
---
## Story
As the owner of a freshly connected gPlug I want to tell it which model I have so that it talks to
my meter's customer interface correctly.

## Steps
1. Open the gPlug's address. The first time, the assistant opens with {{S.welcome}}. It shows the
   firmware version and the device's name. Tap {{S.next}}.
   {{img:wizard-welcome|The assistant's welcome page}}
2. Under {{S.hardware}}, choose your model, for example gPlugK. The model name is printed on the
   case.
   {{img:wizard-hardware|Choosing the model}}
3. You only need {{S.pins}} if you have modified the gPlug yourself. Otherwise leave the proposed
   values as they are.
4. Tap {{S.next}}.

## Acceptance
- The assistant moves on to the {{S.meter}} step.
- The chosen model is saved, even if you leave the assistant now.
