---
id: setup-meter
title: Confirmer le profil du compteur et saisir la clé
chapter: setup
screen: wizard-meter
order: 30
mock: 2026-09-30
hardware: gPlugK 2026-09-14
---
## Story
En tant que propriétaire d'un gPlug, je veux choisir le bon profil de compteur et enregistrer la clé de
mon gestionnaire de réseau afin que le gPlug puisse lire les valeurs de mon compteur.

## Steps
1. À l'étape {{S.meter}}, le gPlug écoute d'abord ce que votre compteur envoie. Après quelques
   secondes, la carte {{S.detectTitle}} affiche {{S.detectFound}} et le protocole reconnu.
2. Si un seul profil convient, il est déjà sélectionné. Si plusieurs conviennent, choisissez le
   profil de votre gestionnaire de réseau. {{S.otherProfile}} affiche d'autres profils.
3. Si le compteur envoie des données chiffrées, saisissez la clé (GUEK) sous {{S.key}} :
   32 caractères parmi 0–9 et A–F, comme dans la lettre de votre gestionnaire de réseau.
   {{img:wizard-meter|Compteur reconnu, profil proposé et champ de la clé}}
4. {{S.values}} indique les valeurs que le profil lit.
5. Touchez {{S.next}}.

## Acceptance
- {{S.next}} ne devient actif qu'une fois un profil choisi et la clé valide.
- L'assistant passe à {{S.done}} et y vérifie la liaison avec le compteur.

## Notes
- La clé est enregistrée uniquement sur le gPlug et n'est plus jamais affichée.
- Si aucun signal n'arrive après une demi-minute, vérifiez le câble. Certains gestionnaires de
  réseau doivent d'abord activer l'interface client.
