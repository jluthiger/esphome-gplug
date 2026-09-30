---
id: device-mqtt
title: Inviare i valori misurati via MQTT
chapter: device
screen: device-mqtt
order: 40
mock: 2026-09-30
---
## Story
Come proprietario di un gPlug con una mia domotica (Node-RED, ioBroker, InfluxDB) voglio inviare i
valori misurati al mio broker MQTT, nel formato che il mio sistema si aspetta.

## Steps
1. Nella sezione {{S.tabDevice}} aprite la scheda {{S.mqttTitle}} e attivate {{S.mqttEnable}}.
2. Inserite {{S.mqttHost}} e {{S.mqttPort}}, se necessario {{S.mqttUser}} e {{S.mqttPassword}},
   e {{S.mqttPeriod}}.
3. Scegliete un modello sotto {{S.mqttPreset}}: {{S.mqttPresetJson}}, {{S.mqttPresetEach}} o
   {{S.mqttPresetInflux}}. Per esigenze particolari modificate voi stessi {{S.mqttTopic}} e
   {{S.mqttPayload}}; {{S.mqttKeys}} elenca i segnaposto.
4. {{S.mqttPreview}} mostra esattamente cosa verrebbe inviato.
   {{img:device-mqtt|Impostazioni MQTT con anteprima}}
5. Facoltativo: {{S.mqttStatusEnable}} invia in più memoria, firmware, Wi-Fi e diagnosi del gPlug su
   un topic proprio; il {{S.mqttAvailTopic}} segnala `online` e `offline`.
6. Toccate {{S.mqttSave}}.

## Acceptance
- Dopo {{S.mqttSaved}} il titolo della scheda mostra {{S.mqttConnected}} e sul broker arrivano
  messaggi.
- Un errore in un modello appare sotto il campo con posizione e motivo, per esempio
  «{{S.tplAt("Topic", "segnaposto o registro sconosciuto", 12)}}», e {{S.mqttSave}} resta
  bloccato finché non è corretto.
- Se il gPlug non raggiunge il broker, il titolo della scheda ne indica il motivo, per esempio
  {{S.mqttErrTcp}} o {{S.mqttErrAuth}}.
- Se dopo un cambio di profilo del contatore un modello non va più bene, il titolo mostra
  {{S.mqttError}} finché non adattate il modello.

## Notes
- Home Assistant non ha bisogno di MQTT; si collega direttamente al gPlug.
