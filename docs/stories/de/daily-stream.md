---
id: daily-stream
title: Empfangene Rohdaten ansehen, kopieren und speichern
chapter: daily
screen: stream
order: 40
mock: 2026-09-30
---
## Story
Als technisch interessierter Besitzer eines gPlug möchte ich die Daten sehen, die der Zähler
tatsächlich sendet, damit ich bei einem Problem zeigen kann, was ankommt.

## Steps
1. Tippen Sie unten auf {{S.tabStream}}. Sie sehen die letzten Frames mit Uhrzeit und
   {{S.crcOk}} oder {{S.crcFail}}.
2. Oben wählen Sie die Ansicht: bei verschlüsselten Zählern {{S.streamRaw}} oder
   {{S.streamPlain}}, bei DSMR-Zählern {{S.streamHex}} oder {{S.streamText}}.
3. Mit dem Schalter {{S.tailOn}} halten Sie die Anzeige an, um in Ruhe zu lesen; der gPlug zeichnet
   weiter auf.
4. Tippen Sie auf einen Frame, um ihn zu öffnen; dort gibt es {{S.copy}} und {{S.frameTxt}}.
5. Markieren Sie Frames oder tippen Sie auf {{S.streamSelectAll}} und laden Sie sie in der Karte
   {{S.exportSel}} herunter.
   {{img:stream|Datenstrom mit den letzten Frames}}

## Acceptance
- Ein kopierter oder gespeicherter Frame enthält die Bytes als Hex-Text mit Zeitstempel und CRC.

## Notes
- {{S.dlHint}}
