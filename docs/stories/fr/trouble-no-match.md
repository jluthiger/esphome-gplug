---
id: trouble-no-match
title: « Aucune valeur correspondante »
chapter: trouble
screen: diagnosis
order: 50
mock: 2026-09-30
---
## Story
En tant que propriétaire d'un gPlug dont le compteur envoie des données lisibles ne contenant aucune
des valeurs attendues, je veux trouver le profil qui convient à mon compteur.

## Steps
1. La carte {{S.dgLabel}} affiche {{S.dgNoMatch}} ; en haut à droite figure {{S.dgNoMatchPill}}.
   {{img:diag-no-match|Aucune valeur correspondante}}
2. Touchez {{S.fixMeter}} et choisissez le profil de votre gestionnaire de réseau. {{S.showAll}}
   affiche tous les profils.

## Acceptance
- Après la correction, le contrôle de la configuration affiche {{S.checkOk}}.

## Notes
- L'onglet {{S.tabStream}} montre ce que le compteur envoie réellement.
