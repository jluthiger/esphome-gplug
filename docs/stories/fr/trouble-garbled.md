---
id: trouble-garbled
title: « Données illisibles »
chapter: trouble
screen: diagnosis
order: 30
mock: 2026-09-30
---
## Story
En tant que propriétaire d'un gPlug qui reçoit des données qu'il ne peut pas lire, je veux savoir quel
réglage est faux.

## Steps
1. La carte {{S.dgLabel}} affiche {{S.dgGarbled}}.
   {{img:diag-garbled|Données illisibles}}
2. Touchez {{S.fixMeter}} et choisissez un autre profil ; le gPlug propose le protocole reconnu.
3. Si cela n'aide pas, vérifiez le modèle choisi avec {{S.fixHardware}}.

## Acceptance
- Après la correction, le contrôle de la configuration affiche {{S.checkOk}}.
