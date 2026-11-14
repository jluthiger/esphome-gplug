---
id: device-tariff
title: Activer le tarif dynamique
chapter: device
screen: device-tariff
order: 45
mock: 2026-11-13
---
## Story
En tant que propriétaire d'un gPlug sur le réseau de BKW ou de CKW, je veux voir le tarif au quart
d'heure de mon fournisseur à côté de mon énergie, afin de savoir quand l'injection ou le soutirage
est avantageux.

## Steps
1. Dans l'onglet {{S.tabDevice}}, ouvrez la carte {{S.tariffTitle}}.
2. Choisissez {{S.tariffBkw}} ou {{S.tariffCkw}}. Le choix s'applique immédiatement.
3. Ou choisissez {{S.tariffSth}} si vous avez un compte sur swisstariffhub.ch : saisissez
   {{S.tariffSthId}} et {{S.tariffSthKey}}, puis touchez {{S.tariffSthSave}}.

## Acceptance
- Sous le choix figure le tarif affiché : {{S.tariffKindFeedIn}} ou {{S.tariffKindGrid}}
- Après quelques secondes s'affiche « {{S.tariffFetched(0)}} », et la carte du tarif apparaît dans
  l'onglet {{S.tabHist}}.
- Si le gPlug ne peut pas joindre le fournisseur, s'affiche {{S.tariffErrNet}}
- Si Swiss Tariff Hub refuse la clé, s'affiche {{S.tariffErrAuth}}
- La clé enregistrée n'est plus jamais affichée ; le champ vide indique {{S.tariffSthKeySet}}.
- {{S.tariffOff}} arrête la récupération ; le gPlug ne contacte alors plus aucun fournisseur.

## Notes
- Le gPlug a besoin d'internet. Il récupère lui-même le tarif, au plus une fois par heure tant que
  des prix manquent, et ne le conserve que jusqu'au prochain redémarrage.
- BKW ne publie que le jour même ; le gPlug n'affiche les jours précédents que s'il les a récupérés
  lui-même. Pour CKW, il rattrape les trois derniers jours.
