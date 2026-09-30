---
id: trouble-key
title: “Key does not fit”
chapter: trouble
screen: diagnosis, device-key
order: 60
mock: 2026-09-30
---
## Story
As the owner of a gPlug with an encrypted meter whose data cannot be decrypted, I want to enter the
right key.

## Steps
1. The {{S.dgLabel}} card shows {{S.dgKey}}; the top right shows {{S.keyWrong}}. On the
   {{S.tabDevice}} tab the {{S.setupKey}} card shows {{S.keyInvalidBadge}}.
   {{img:diag-key|Key does not fit}}
2. Tap {{S.fixKey}}. The key field is empty.
3. Enter the key (GUEK) from your grid operator's letter – 32 characters, without spaces – and tap
   {{S.next}}.

## Acceptance
- After the correction the setup check shows {{S.checkOk}}, and the {{S.setupKey}} card shows
  {{S.keySet}}.

## Notes
- Do not confuse the GUEK with other keys in the letter (such as the authentication key).
