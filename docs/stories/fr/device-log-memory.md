---
id: device-log-memory
title: Consulter le journal des événements et la mémoire
chapter: device
screen: device-log, device-mem
order: 70
mock: 2026-09-30
---
## Story
En tant que propriétaire d'un gPlug, je veux voir ce qui s'est passé sur l'appareil – par exemple
pourquoi il a redémarré – afin de signaler un problème ou de le comprendre moi-même.

## Steps
1. Dans l'onglet {{S.tabDevice}}, ouvrez la carte {{S.logTitle}}. Elle liste les derniers
   événements avec leur heure, par exemple {{S.evBoot}}, {{S.evWifiLost}} ou {{S.evMeterLost}}.
   {{img:device-log|Journal des événements}}
2. {{S.logCopy}} ou {{S.logDownload}} vous permet de transmettre le journal.
3. La carte {{S.memTitle}} affiche {{S.memFree}}, {{S.memMin}} et {{S.memLargest}}, ainsi que
   l'évolution des dernières 24 heures.
   {{img:device-mem|Mémoire}}

## Acceptance
- Le journal est conservé après un redémarrage et indique la cause de chaque redémarrage.

## Notes
- {{S.memHint}}
