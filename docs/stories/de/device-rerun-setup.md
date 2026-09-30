---
id: device-rerun-setup
title: Den Einrichtungsassistenten erneut öffnen
chapter: device
screen: wizard-hardware, wizard-meter
order: 90
mock: 2026-09-30
---
## Story
Als Besitzer eines gPlug möchte ich Modell oder Zählerprofil später ändern, zum Beispiel nach einem
Zählerwechsel, ohne alles neu einzurichten.

## Steps
1. Hängen Sie an die Adresse des gPlug einen der folgenden Teile an und öffnen Sie sie:
   - `#setup` – der ganze Assistent ab {{S.welcome}}
   - `#setup/hardware` – direkt zum Schritt {{S.hardware}}
   - `#setup/meter` – direkt zum Schritt {{S.meter}}
   - `#setup/key` – zum Schritt {{S.meter}}, mit leerem Schlüsselfeld
2. Ändern Sie, was nötig ist, und gehen Sie mit {{S.next}} bis {{S.done}}.

## Acceptance
- Die Einrichtungsprüfung zeigt {{S.checkOk}}; der Verlauf bleibt erhalten.

## Notes
- Die Knöpfe der Karte {{S.dgLabel}} führen genau zu diesen Adressen.
