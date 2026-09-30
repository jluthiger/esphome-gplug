---
id: device-rerun-setup
title: Rouvrir l'assistant de configuration
chapter: device
screen: wizard-hardware, wizard-meter
order: 90
mock: 2026-09-30
---
## Story
En tant que propriétaire d'un gPlug, je veux changer plus tard le modèle ou le profil du compteur, par
exemple après un remplacement du compteur, sans tout reconfigurer.

## Steps
1. Ajoutez l'un des éléments suivants à l'adresse du gPlug et ouvrez-la :
   - `#setup` – tout l'assistant depuis {{S.welcome}}
   - `#setup/hardware` – directement à l'étape {{S.hardware}}
   - `#setup/meter` – directement à l'étape {{S.meter}}
   - `#setup/key` – à l'étape {{S.meter}}, avec un champ de clé vide
2. Modifiez ce qui est nécessaire et continuez avec {{S.next}} jusqu'à {{S.done}}.

## Acceptance
- Le contrôle de la configuration affiche {{S.checkOk}} ; l'historique est conservé.

## Notes
- Les boutons de la carte {{S.dgLabel}} mènent exactement à ces adresses.
