---
id: setup-port-closed
title: Continuer alors que l'interface client n'est pas encore activée
chapter: setup
screen: wizard-done
order: 50
mock: 2026-09-30
---
## Story
En tant que propriétaire d'un gPlug dont le gestionnaire de réseau n'a pas encore activé l'interface
client, je veux terminer quand même la configuration afin que le gPlug lise dès que le compteur
envoie.

## Steps
1. À l'étape {{S.done}}, {{S.dgSilent}} apparaît après une minute.
2. Si vous êtes sûr que le modèle et le câble sont corrects, touchez {{S.continueAnyway}}.
   {{img:diag-silent|Aucun signal du compteur, avec le chemin vers la vue en direct}}

## Acceptance
- La vue en direct s'ouvre et affiche la même carte {{S.dgLabel}} jusqu'à l'arrivée de données.
  {{img:live-diag|Vue en direct tant que le compteur reste muet}}
- Dès que le gestionnaire de réseau active l'interface, les valeurs apparaissent sans autre action.
