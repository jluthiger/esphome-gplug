---
id: device-firmware
title: Die Firmware aktualisieren
chapter: device
screen: device-fw
order: 50
mock: 2026-09-30
hardware: gPlugK 2026-09-25
---
## Story
Als Besitzer eines gPlug möchte ich eine neue Firmware installieren, ohne ein Kabel anzuschliessen,
damit mein Gerät Fehlerbehebungen und neue Funktionen bekommt.

## Steps
1. Öffnen Sie im Tab {{S.tabDevice}} die Karte {{S.fwTitle}}. Sie zeigt die installierte Version.
2. Tippen Sie auf {{S.fwRelCheck}}. Gibt es eine neuere Version, erscheint zum Beispiel
   {{S.fwRelAvail("0.6.0", "0.7.0")}} mit dem Link {{S.fwRelNotes}}.
   {{img:device-fw|Ein Update ist verfügbar}}
3. Tippen Sie auf {{S.fwRelInstall("0.7.0")}} und bestätigen Sie mit {{S.fwRelConfirm}}.
   {{img:device-fw-confirm|Installation bestätigen}}
4. Der gPlug lädt die Firmware selbst herunter und startet neu. Lassen Sie ihn am Strom.
   {{img:device-fw-done|Firmware aktualisiert}}

## Acceptance
- Die Karte zeigt {{S.fwDone}} und die neue Version.
- Einstellungen und Verlauf sind erhalten; nur die laufende Viertelstunde geht verloren.

## Notes
- Der gPlug fragt nie von selbst nach Updates, nur wenn Sie {{S.fwRelCheck}} tippen.
- Eine Firmware-Datei vom Computer installieren Sie mit {{S.fwPick}} und {{S.fwInstall}}. Nehmen
  Sie die Datei im OTA-Format (`.ota.bin`).
- Ist ein OTA-Passwort gesetzt, fragt die Karte nach dem {{S.fwPassword}}.
