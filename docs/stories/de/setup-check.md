---
id: setup-check
title: Die Einrichtungsprüfung verstehen
chapter: setup
screen: wizard-done, diagnosis
order: 40
mock: 2026-09-30
---
## Story
Als Besitzer eines gPlug möchte ich am Ende der Einrichtung sehen, ob der Zähler wirklich gelesen
wird, damit ich einen Fehler behebe, solange ich noch im Assistenten bin.

## Steps
1. Im Schritt {{S.done}} zeigt die Karte {{S.checkTitle}} zuerst {{S.checkWaiting}}. Das dauert je
   nach Zähler bis zu einer Minute.
   {{img:wizard-done-waiting|Die Prüfung wartet auf die ersten Daten}}
2. Kommen Daten an, erscheint {{S.checkOk}} mit der Zählernummer, der aktuellen Leistung und der Zahl
   der Werte. Tippen Sie auf {{S.openLive}}.
   {{img:wizard-done-ok|Daten empfangen}}
3. Kommen keine brauchbaren Daten an, erscheint stattdessen die Karte {{S.dgLabel}} mit einer
   Erklärung und einem Knopf, der Sie direkt zum richtigen Schritt zurückbringt, etwa
   {{S.fixMeter}}. Welche Meldung was bedeutet, steht im Kapitel *Wenn etwas nicht stimmt*.

## Acceptance
- Nach {{S.openLive}} sehen Sie die Live-Ansicht mit {{S.dataOk}}.
- Nach einem Knopf der Karte {{S.dgLabel}} sind Sie im passenden Schritt des Assistenten; Ihre
  übrigen Angaben bleiben erhalten.
