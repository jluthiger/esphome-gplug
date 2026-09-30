---
id: device-mqtt
title: Messwerte per MQTT senden
chapter: device
screen: device-mqtt
order: 40
mock: 2026-09-30
---
## Story
Als Besitzer eines gPlug mit einer eigenen Hausautomation (Node-RED, ioBroker, InfluxDB) möchte ich
die Messwerte an meinen MQTT-Broker senden, im Format, das mein System erwartet.

## Steps
1. Öffnen Sie im Tab {{S.tabDevice}} die Karte {{S.mqttTitle}} und schalten Sie {{S.mqttEnable}} ein.
2. Tragen Sie {{S.mqttHost}} und {{S.mqttPort}} ein, falls nötig {{S.mqttUser}} und
   {{S.mqttPassword}}, dazu {{S.mqttPeriod}}.
3. Wählen Sie unter {{S.mqttPreset}} eine Vorlage: {{S.mqttPresetJson}}, {{S.mqttPresetEach}} oder
   {{S.mqttPresetInflux}}. Wer mehr braucht, ändert {{S.mqttTopic}} und {{S.mqttPayload}} selbst;
   {{S.mqttKeys}} listet die Platzhalter.
4. {{S.mqttPreview}} zeigt genau, was gesendet würde.
   {{img:device-mqtt|MQTT-Einstellungen mit Vorschau}}
5. Optional: {{S.mqttStatusEnable}} sendet zusätzlich Speicher, Firmware, WLAN und Diagnose des
   gPlug auf eigenem Topic; {{S.mqttAvailTopic}} meldet `online` und `offline`.
6. Tippen Sie auf {{S.mqttSave}}.

## Acceptance
- Nach {{S.mqttSaved}} zeigt der Kartentitel {{S.mqttConnected}}, und auf dem Broker kommen
  Nachrichten an.
- Ein Fehler in einer Vorlage erscheint unter dem Feld mit Stelle und Grund, zum Beispiel
  „{{S.tplAt("Topic", "unbekannter Platzhalter oder Register", 12)}}“, und {{S.mqttSave}} bleibt
  gesperrt, bis er behoben ist.
- Kann der gPlug den Broker nicht erreichen, zeigt der Kartentitel den Grund, etwa {{S.mqttErrTcp}}
  oder {{S.mqttErrAuth}}.
- Passt eine Vorlage nach einem Wechsel des Zählerprofils nicht mehr, zeigt der Titel
  {{S.mqttError}}, bis Sie die Vorlage anpassen.

## Notes
- Home Assistant braucht MQTT nicht; es verbindet sich direkt mit dem gPlug.
