---
id: setup-wifi
title: Connecter le gPlug au Wi-Fi
chapter: setup
screen: captive
order: 10
mock: 2026-09-30
hardware: gPlugK 2026-09-16 (Android)
---
## Story
En tant que nouveau propriétaire d'un gPlug, je veux le connecter à mon Wi-Fi domestique afin de
pouvoir ouvrir son application dans un navigateur.

## Steps
1. Branchez le gPlug sur l'interface client du compteur. Tant qu'il ne connaît aucun Wi-Fi, il ouvre
   son propre réseau nommé `gPlug-Setup`.
2. Connectez votre téléphone à `gPlug-Setup`. Le téléphone ouvre la page de configuration de
   lui-même ; sinon, ouvrez `http://192.168.4.1/` dans le navigateur.
3. Touchez votre réseau domestique dans la liste et saisissez son mot de passe sous {{C.set}}.
   {{img:captive|Page de configuration avec les réseaux trouvés}}
4. Notez l'adresse sous {{C.addr}} – ou touchez {{C.copy}}. Elle a la forme
   `http://gplug-xxxxxx.local/`.
5. Touchez {{C.save}}. Faites maintenant une capture d'écran : la page se ferme dès que le téléphone
   revient sur votre Wi-Fi domestique.
   {{img:captive-saved|Après l'enregistrement : la future adresse du gPlug}}
6. Reconnectez le téléphone à votre Wi-Fi domestique et ouvrez l'adresse notée.

## Acceptance
- L'adresse notée affiche l'assistant de configuration du gPlug.
- Le réseau `gPlug-Setup` disparaît peu après.

## Notes
- La page de configuration suit la langue du téléphone.
- Si l'adresse ne s'ouvre pas, cherchez le gPlug dans la liste des appareils de votre routeur sous
  son nom `gplug-xxxxxx` et ouvrez son adresse IP.
