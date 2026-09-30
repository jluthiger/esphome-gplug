---
id: device-reset-wifi
title: Den gPlug in den Einrichtungsmodus zurücksetzen
chapter: device
screen: captive
order: 95
mock: 2026-09-30
hardware: gPlugK 2026-09-10
---
## Story
Als Besitzer eines gPlug, der sein WLAN nicht mehr erreicht (neuer Router, geändertes Passwort),
möchte ich ihn wieder in den Einrichtungsmodus bringen, damit ich ihn neu verbinden kann.

## Steps
1. Halten Sie die Taste am gPlug mindestens 3 Sekunden gedrückt.
2. Der gPlug vergisst seine WLAN-Zugangsdaten, startet neu und öffnet wieder das Netz
   `gPlug-Setup`; die Leuchte blinkt blau.
3. Verbinden Sie ihn wie bei der Ersteinrichtung und tippen Sie auf {{C.save}}.

## Acceptance
- Der gPlug ist im neuen WLAN erreichbar.
- Modell, Zählerprofil, Schlüssel und Verlauf sind erhalten; der Assistent muss nicht erneut
  durchlaufen werden.
