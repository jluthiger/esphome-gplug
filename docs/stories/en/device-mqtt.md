---
id: device-mqtt
title: Send meter values over MQTT
chapter: device
screen: device-mqtt
order: 40
mock: 2026-09-30
---
## Story
As the owner of a gPlug with a home automation system of my own (Node-RED, ioBroker, InfluxDB) I want
to send the meter values to my MQTT broker, in the format my system expects.

## Steps
1. On the {{S.tabDevice}} tab, open the {{S.mqttTitle}} card and switch {{S.mqttEnable}} on.
2. Enter the {{S.mqttHost}} and {{S.mqttPort}}, if needed {{S.mqttUser}} and {{S.mqttPassword}},
   and the {{S.mqttPeriod}}.
3. Choose a {{S.mqttPreset}}: {{S.mqttPresetJson}}, {{S.mqttPresetEach}} or
   {{S.mqttPresetInflux}}. If you need more, edit {{S.mqttTopic}} and {{S.mqttPayload}} yourself;
   {{S.mqttKeys}} lists the placeholders.
4. {{S.mqttPreview}} shows exactly what would be sent.
   {{img:device-mqtt|MQTT settings with preview}}
5. Optional: {{S.mqttStatusEnable}} also sends the gPlug's memory, firmware, Wi-Fi and diagnosis on
   a topic of its own; the {{S.mqttAvailTopic}} reports `online` and `offline`.
6. Tap {{S.mqttSave}}.

## Acceptance
- After {{S.mqttSaved}} the card title shows {{S.mqttConnected}}, and messages arrive on the broker.
- A mistake in a template appears below the field with its position and reason, for example
  “{{S.tplAt("Topic", "unknown placeholder or register", 12)}}”, and {{S.mqttSave}} stays disabled
  until it is fixed.
- If the gPlug cannot reach the broker, the card title shows the reason, such as {{S.mqttErrTcp}}
  or {{S.mqttErrAuth}}.
- If a template no longer fits after a change of meter profile, the title shows {{S.mqttError}}
  until you adapt the template.

## Notes
- Home Assistant does not need MQTT; it connects to the gPlug directly.
