---
id: setup-check
title: Comprendre le contrôle de la configuration
chapter: setup
screen: wizard-done, diagnosis
order: 40
mock: 2026-09-30
---
## Story
En tant que propriétaire d'un gPlug, je veux voir à la fin de la configuration si le compteur est
vraiment lu, afin de corriger une erreur tant que je suis encore dans l'assistant.

## Steps
1. À l'étape {{S.done}}, la carte {{S.checkTitle}} affiche d'abord {{S.checkWaiting}}. Selon le
   compteur, cela prend jusqu'à une minute.
   {{img:wizard-done-waiting|Le contrôle attend les premières données}}
2. Dès que des données arrivent, {{S.checkOk}} s'affiche avec le numéro du compteur, la puissance
   actuelle et le nombre de valeurs. Touchez {{S.openLive}}.
   {{img:wizard-done-ok|Données reçues}}
3. Si aucune donnée exploitable n'arrive, la carte {{S.dgLabel}} apparaît à la place, avec une
   explication et un bouton qui vous ramène directement à la bonne étape, par exemple
   {{S.fixMeter}}. La signification de chaque message est expliquée au chapitre *Quand quelque chose
   ne va pas*.

## Acceptance
- Après {{S.openLive}}, vous voyez la vue en direct avec {{S.dataOk}}.
- Après un bouton de la carte {{S.dgLabel}}, vous êtes à la bonne étape de l'assistant ; vos autres
  réglages sont conservés.
