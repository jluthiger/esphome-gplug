---
id: device-restart
title: Redémarrer le gPlug
chapter: device
screen: device-fw
order: 60
mock: 2026-09-30
---
## Story
En tant que propriétaire d'un gPlug, je veux le redémarrer depuis l'application sans le débrancher.

## Steps
1. Dans l'onglet {{S.tabDevice}}, ouvrez la carte {{S.fwTitle}}.
2. Touchez {{S.rsButton}} et confirmez avec {{S.rsConfirm}}.

## Acceptance
- Après quelques secondes, la carte indique {{S.rsBack}} ; la {{S.fwUptime}} repart de zéro.
- Le journal des événements enregistre le redémarrage.

## Notes
- {{S.rsHint}}
