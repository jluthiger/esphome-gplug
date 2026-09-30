---
id: setup-device
title: Den Assistenten starten und das Gerät wählen
chapter: setup
screen: wizard-welcome, wizard-hardware
order: 20
mock: 2026-09-30
---
## Story
Als Besitzer eines frisch verbundenen gPlug möchte ich angeben, welches Modell ich habe, damit er die
Kundenschnittstelle meines Zählers richtig anspricht.

## Steps
1. Öffnen Sie die Adresse des gPlug. Beim ersten Mal erscheint der Assistent mit {{S.welcome}}. Er
   zeigt die Firmware-Version und den Namen des Geräts. Tippen Sie auf {{S.next}}.
   {{img:wizard-welcome|Willkommensseite des Assistenten}}
2. Unter {{S.hardware}} wählen Sie Ihr Modell, zum Beispiel gPlugK. Die Bezeichnung steht auf dem
   Gehäuse.
   {{img:wizard-hardware|Auswahl des Modells}}
3. {{S.pins}} brauchen Sie nur, wenn Sie den gPlug selbst umgebaut haben. Sonst lassen Sie die
   vorgeschlagenen Werte stehen.
4. Tippen Sie auf {{S.next}}.

## Acceptance
- Der Assistent wechselt zum Schritt {{S.meter}}.
- Das gewählte Modell ist gespeichert, auch wenn Sie den Assistenten jetzt abbrechen.
