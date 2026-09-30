---
id: trouble-silent
title: « Aucun signal du compteur »
chapter: trouble
screen: diagnosis
order: 20
mock: 2026-09-30
---
## Story
En tant que propriétaire d'un gPlug qui ne reçoit rien du compteur, je veux savoir quoi vérifier.

## Steps
1. La carte {{S.dgLabel}} affiche {{S.dgSilent}} ; en haut à droite figure {{S.noData}}.
   {{img:diag-silent|Aucun signal du compteur}}
2. Vérifiez que le gPlug est bien enfiché dans l'interface client et que le câble est intact.
3. Touchez {{S.fixHardware}} et vérifiez que le bon modèle est sélectionné.
4. Si rien n'arrive toujours, demandez à votre gestionnaire de réseau si l'interface client est
   activée. En attendant, vous pouvez continuer avec {{S.continueAnyway}}.

## Acceptance
- Après la correction, le contrôle de la configuration affiche {{S.checkOk}}.
