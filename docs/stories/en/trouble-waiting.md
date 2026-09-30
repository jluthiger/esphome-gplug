---
id: trouble-waiting
title: “Waiting for the first data” does not go away
chapter: trouble
screen: wizard-done
order: 10
mock: 2026-09-30
---
## Story
As the owner of a gPlug I want to know how long the check may take, so that I do not change anything
too early.

## Steps
1. After the meter profile has been saved, {{S.checkTitle}} shows {{S.checkWaiting}} with a seconds
   counter.
   {{img:wizard-done-waiting|The check is waiting}}
2. Wait up to a minute. Some meters send only every 10 to 30 seconds.

## Acceptance
- After a minute at the latest, {{S.checkOk}} appears, or the {{S.dgLabel}} card with one of the
  messages in this chapter.
