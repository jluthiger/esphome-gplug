---
id: trouble-waiting
title: « En attente des premières données » ne disparaît pas
chapter: trouble
screen: wizard-done
order: 10
mock: 2026-09-30
---
## Story
En tant que propriétaire d'un gPlug, je veux savoir combien de temps le contrôle peut durer, afin de
ne rien changer trop tôt.

## Steps
1. Après l'enregistrement du profil de compteur, {{S.checkTitle}} affiche {{S.checkWaiting}} avec un
   compteur de secondes.
   {{img:wizard-done-waiting|Le contrôle attend}}
2. Attendez jusqu'à une minute. Certains compteurs n'envoient que toutes les 10 à 30 secondes.

## Acceptance
- Au plus tard après une minute, {{S.checkOk}} apparaît, ou la carte {{S.dgLabel}} avec l'un des
  messages de ce chapitre.
