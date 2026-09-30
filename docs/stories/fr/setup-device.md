---
id: setup-device
title: Lancer l'assistant et choisir l'appareil
chapter: setup
screen: wizard-welcome, wizard-hardware
order: 20
mock: 2026-09-30
---
## Story
En tant que propriétaire d'un gPlug fraîchement connecté, je veux indiquer quel modèle je possède afin
qu'il communique correctement avec l'interface client de mon compteur.

## Steps
1. Ouvrez l'adresse du gPlug. La première fois, l'assistant s'ouvre sur {{S.welcome}}. Il affiche
   la version du micrologiciel et le nom de l'appareil. Touchez {{S.next}}.
   {{img:wizard-welcome|Page d'accueil de l'assistant}}
2. Sous {{S.hardware}}, choisissez votre modèle, par exemple gPlugK. La désignation figure sur le
   boîtier.
   {{img:wizard-hardware|Choix du modèle}}
3. {{S.pins}} ne sert que si vous avez modifié le gPlug vous-même. Sinon, laissez les valeurs
   proposées.
4. Touchez {{S.next}}.

## Acceptance
- L'assistant passe à l'étape {{S.meter}}.
- Le modèle choisi est enregistré, même si vous quittez l'assistant maintenant.
