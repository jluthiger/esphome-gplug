---
id: device-language-theme
title: Changer la langue et l'apparence
chapter: device
screen: device-lang, device-theme
order: 80
mock: 2026-09-30
---
## Story
En tant qu'utilisateur de l'application, je veux choisir la langue et une apparence claire ou sombre,
afin de la lire confortablement.

## Steps
1. Dans l'onglet {{S.tabDevice}}, ouvrez la carte {{S.language}} et touchez Deutsch, Français,
   Italiano ou English.
   {{img:device-lang|Choisir la langue}}
2. Ouvrez la carte {{S.theme}} et choisissez {{S.themeLight}} ou {{S.themeDark}}.
   {{img:device-theme|Choisir l'apparence}}

## Acceptance
- L'application change immédiatement, sans rechargement.
- Le choix vaut pour ce navigateur ; un autre téléphone garde le sien.
