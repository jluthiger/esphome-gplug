---
id: setup-port-closed
title: Weitermachen, obwohl die Kundenschnittstelle noch nicht freigeschaltet ist
chapter: setup
screen: wizard-done
order: 50
mock: 2026-09-30
---
## Story
Als Besitzer eines gPlug, dessen Netzbetreiber die Kundenschnittstelle noch nicht freigeschaltet hat,
möchte ich die Einrichtung trotzdem abschliessen, damit der gPlug sofort liest, sobald der Zähler
sendet.

## Steps
1. Im Schritt {{S.done}} erscheint nach einer Minute {{S.dgSilent}}.
2. Sind Modell und Kabel sicher richtig, tippen Sie auf {{S.continueAnyway}}.
   {{img:diag-silent|Keine Signale vom Zähler, mit dem Weg zur Live-Ansicht}}

## Acceptance
- Die Live-Ansicht öffnet sich und zeigt dieselbe Karte {{S.dgLabel}}, bis Daten ankommen.
  {{img:live-diag|Live-Ansicht, solange der Zähler schweigt}}
- Sobald der Netzbetreiber freischaltet, erscheinen die Werte ohne weiteres Zutun.
