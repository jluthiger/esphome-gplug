---
id: trouble-key
title: « La clé ne convient pas »
chapter: trouble
screen: diagnosis, device-key
order: 60
mock: 2026-09-30
---
## Story
En tant que propriétaire d'un gPlug avec un compteur chiffré dont les données ne peuvent pas être
déchiffrées, je veux saisir la bonne clé.

## Steps
1. La carte {{S.dgLabel}} affiche {{S.dgKey}} ; en haut à droite figure {{S.keyWrong}}. Dans
   l'onglet {{S.tabDevice}}, la carte {{S.setupKey}} affiche {{S.keyInvalidBadge}}.
   {{img:diag-key|La clé ne convient pas}}
2. Touchez {{S.fixKey}}. Le champ de la clé est vide.
3. Saisissez la clé (GUEK) de la lettre de votre gestionnaire de réseau – 32 caractères, sans
   espaces – et touchez {{S.next}}.

## Acceptance
- Après la correction, le contrôle de la configuration affiche {{S.checkOk}} et la carte
  {{S.setupKey}} affiche {{S.keySet}}.

## Notes
- Ne confondez pas la GUEK avec d'autres clés de la lettre (par exemple la clé d'authentification).
