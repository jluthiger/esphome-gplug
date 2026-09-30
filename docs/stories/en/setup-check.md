---
id: setup-check
title: Understand the setup check
chapter: setup
screen: wizard-done, diagnosis
order: 40
mock: 2026-09-30
---
## Story
As the owner of a gPlug I want to see at the end of the setup whether the meter is really being read,
so that I can fix a mistake while I am still in the assistant.

## Steps
1. In the {{S.done}} step the {{S.checkTitle}} card first shows {{S.checkWaiting}}. Depending on the
   meter this takes up to a minute.
   {{img:wizard-done-waiting|The check waits for the first data}}
2. When data arrives, {{S.checkOk}} appears with the meter number, the current power and the number
   of values. Tap {{S.openLive}}.
   {{img:wizard-done-ok|Data received}}
3. If no usable data arrives, the {{S.dgLabel}} card appears instead, with an explanation and a
   button that takes you straight back to the right step, such as {{S.fixMeter}}. What each message
   means is explained in the chapter *When something is wrong*.

## Acceptance
- After {{S.openLive}} you see the live view with {{S.dataOk}}.
- After a button on the {{S.dgLabel}} card you are in the matching step of the assistant; your other
  entries are kept.
