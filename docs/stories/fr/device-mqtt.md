---
id: device-mqtt
title: Envoyer les mesures par MQTT
chapter: device
screen: device-mqtt
order: 40
mock: 2026-09-30
---
## Story
En tant que propriétaire d'un gPlug avec ma propre domotique (Node-RED, ioBroker, InfluxDB), je veux
envoyer les mesures à mon broker MQTT, dans le format attendu par mon système.

## Steps
1. Dans l'onglet {{S.tabDevice}}, ouvrez la carte {{S.mqttTitle}} et activez {{S.mqttEnable}}.
2. Saisissez {{S.mqttHost}} et {{S.mqttPort}}, au besoin {{S.mqttUser}} et {{S.mqttPassword}},
   ainsi que {{S.mqttPeriod}}.
3. Choisissez un modèle sous {{S.mqttPreset}} : {{S.mqttPresetJson}}, {{S.mqttPresetEach}} ou
   {{S.mqttPresetInflux}}. Pour aller plus loin, modifiez vous-même {{S.mqttTopic}} et
   {{S.mqttPayload}} ; {{S.mqttKeys}} liste les espaces réservés.
4. {{S.mqttPreview}} montre exactement ce qui serait envoyé.
   {{img:device-mqtt|Réglages MQTT avec aperçu}}
5. En option : {{S.mqttStatusEnable}} envoie en plus la mémoire, le micrologiciel, le Wi-Fi et le
   diagnostic du gPlug sur un topic séparé ; le {{S.mqttAvailTopic}} signale `online` et `offline`.
6. Touchez {{S.mqttSave}}.

## Acceptance
- Après {{S.mqttSaved}}, le titre de la carte affiche {{S.mqttConnected}} et des messages arrivent
  sur le broker.
- Une erreur dans un modèle s'affiche sous le champ avec sa position et sa cause, par exemple
  « {{S.tplAt("Topic", "espace réservé ou registre inconnu", 12)}} », et {{S.mqttSave}} reste
  bloqué tant qu'elle n'est pas corrigée.
- Si le gPlug n'atteint pas le broker, le titre de la carte en indique la cause, par exemple
  {{S.mqttErrTcp}} ou {{S.mqttErrAuth}}.
- Si un modèle ne convient plus après un changement de profil de compteur, le titre affiche
  {{S.mqttError}} jusqu'à ce que vous adaptiez le modèle.

## Notes
- Home Assistant n'a pas besoin de MQTT ; il se connecte directement au gPlug.
