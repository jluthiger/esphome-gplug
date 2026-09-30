---
id: daily-stream
title: View, copy and save the received raw data
chapter: daily
screen: stream
order: 40
mock: 2026-09-30
---
## Story
As a technically minded owner of a gPlug I want to see the data the meter actually sends, so that I
can show what arrives when there is a problem.

## Steps
1. Tap {{S.tabStream}} at the bottom. You see the last frames with their time and {{S.crcOk}} or
   {{S.crcFail}}.
2. Choose the view at the top: for encrypted meters {{S.streamRaw}} or {{S.streamPlain}}, for DSMR
   meters {{S.streamHex}} or {{S.streamText}}.
3. The {{S.tailOn}} switch freezes the view so you can read in peace; the gPlug keeps recording.
4. Tap a frame to open it; it offers {{S.copy}} and {{S.frameTxt}}.
5. Tick frames or tap {{S.streamSelectAll}} and download them in the {{S.exportSel}} card.
   {{img:stream|Data stream with the last frames}}

## Acceptance
- A copied or saved frame contains the bytes as hex text with timestamp and CRC.

## Notes
- {{S.dlHint}}
