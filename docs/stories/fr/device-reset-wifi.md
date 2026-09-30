---
id: device-reset-wifi
title: Remettre le gPlug en mode configuration
chapter: device
screen: captive
order: 95
mock: 2026-09-30
hardware: gPlugK 2026-09-10
---
## Story
En tant que propriétaire d'un gPlug qui n'atteint plus son Wi-Fi (nouveau routeur, mot de passe
changé), je veux le remettre en mode configuration afin de pouvoir le reconnecter.

## Steps
1. Maintenez le bouton du gPlug enfoncé pendant au moins 3 secondes.
2. Le gPlug oublie ses identifiants Wi-Fi, redémarre et ouvre à nouveau le réseau `gPlug-Setup` ;
   le voyant clignote en bleu.
3. Connectez-le comme lors de la première mise en service et touchez {{C.save}}.

## Acceptance
- Le gPlug est joignable sur le nouveau Wi-Fi.
- Le modèle, le profil du compteur, la clé et l'historique sont conservés ; l'assistant n'a pas à
  être refait.
