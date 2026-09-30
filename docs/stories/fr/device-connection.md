---
id: device-connection
title: Consulter l'adresse et la connexion du gPlug
chapter: device
screen: device-conn
order: 10
mock: 2026-09-30
---
## Story
En tant que propriétaire d'un gPlug, je veux consulter l'adresse à laquelle il est joignable et la
qualité de sa réception Wi-Fi, afin de le retrouver et de lui choisir un bon emplacement.

## Steps
1. Touchez {{S.tabDevice}} en bas. La carte {{S.setupConn}} est ouverte.
2. Elle affiche {{S.host}} (l'adresse en `.local`), {{S.ip}}, le {{S.wlan}} avec la force du
   signal et le {{S.profile}} choisi.
   {{img:device|Onglet Appareil avec connexion et clé}}

## Acceptance
- L'adresse affichée ouvre aussi l'application sur un autre appareil du même Wi-Fi.

## Notes
- La force du signal est indiquée en dBm : −50 est très bon, en dessous de −80 la connexion devient
  peu fiable.
- Chaque carte de l'onglet {{S.tabDevice}} s'ouvre et se ferme d'un appui sur son titre ; le
  navigateur s'en souvient.
