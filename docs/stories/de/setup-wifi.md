---
id: setup-wifi
title: Den gPlug mit dem WLAN verbinden
chapter: setup
screen: captive
order: 10
mock: 2026-09-30
hardware: gPlugK 2026-09-16 (Android)
---
## Story
Als neue Besitzerin oder neuer Besitzer eines gPlug möchte ich ihn mit meinem Heim-WLAN verbinden,
damit ich seine App im Browser öffnen kann.

## Steps
1. Stecken Sie den gPlug an die Kundenschnittstelle des Zählers. Solange er kein WLAN kennt, öffnet
   er ein eigenes Netz namens `gPlug-Setup`.
2. Verbinden Sie Ihr Telefon mit `gPlug-Setup`. Das Telefon öffnet die Einrichtungsseite von selbst;
   falls nicht, öffnen Sie im Browser `http://192.168.4.1/`.
3. Tippen Sie in der Liste auf Ihr Heimnetz und geben Sie unter {{C.set}} das Passwort ein.
   {{img:captive|Einrichtungsseite mit den gefundenen Netzen}}
4. Notieren Sie die Adresse unter {{C.addr}} – oder tippen Sie auf {{C.copy}}. Sie hat die Form
   `http://gplug-xxxxxx.local/`.
5. Tippen Sie auf {{C.save}}. Machen Sie jetzt ein Bildschirmfoto: Die Seite schliesst sich, sobald
   das Telefon ins Heim-WLAN zurückwechselt.
   {{img:captive-saved|Nach dem Speichern: die spätere Adresse des gPlug}}
6. Verbinden Sie das Telefon wieder mit dem Heim-WLAN und öffnen Sie die notierte Adresse.

## Acceptance
- Unter der notierten Adresse erscheint der Einrichtungsassistent des gPlug.
- Das Netz `gPlug-Setup` verschwindet nach kurzer Zeit.

## Notes
- Die Einrichtungsseite richtet sich nach der Sprache des Telefons.
- Öffnet sich die Adresse nicht, finden Sie den gPlug in der Geräteliste Ihres Routers unter seinem
  Namen `gplug-xxxxxx` und öffnen dessen IP-Adresse.
