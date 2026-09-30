---
id: device-wifi
title: Changer de Wi-Fi
chapter: device
screen: device-wifi
order: 20
mock: 2026-09-30
---
## Story
En tant que propriétaire d'un gPlug, je veux le faire passer sur un autre réseau Wi-Fi, par exemple
après un changement de routeur, sans le reconfigurer.

## Steps
1. Touchez {{S.tabDevice}} puis, dans la carte {{S.setupConn}}, {{S.changeWifi}}.
2. Vous voyez la connexion actuelle. Touchez à nouveau {{S.changeWifi}} ; le gPlug cherche les
   réseaux à portée.
   {{img:device-wifi-scan|Réseaux trouvés}}
3. Touchez le nouveau réseau, saisissez le {{S.password}} et touchez {{S.connect}}. Si le réseau est
   masqué, choisissez {{S.manual}}.
4. Après {{S.connecting}}, {{S.connected}} s'affiche avec le réseau, l'IP et le signal.
   {{img:device-wifi-ok|Connecté au nouveau réseau}}

## Acceptance
- {{S.connected}} indique le nouveau réseau.
- Avec un mauvais mot de passe, {{S.connectFailed}} s'affiche et le gPlug reste joignable.

## Notes
- Si le nouveau réseau n'est pas celui de votre téléphone, connectez ensuite le téléphone à ce réseau
  et rouvrez l'adresse.
