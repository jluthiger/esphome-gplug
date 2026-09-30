---
id: trouble-protocol
title: « Le profil ne correspond pas au compteur »
chapter: trouble
screen: diagnosis
order: 40
mock: 2026-09-30
---
## Story
En tant que propriétaire d'un gPlug ayant choisi un profil avec le mauvais protocole, je veux arriver
rapidement au bon profil.

## Steps
1. La carte {{S.dgLabel}} affiche {{S.dgProtocol}} ; en haut à droite figure {{S.dgProtocolPill}}.
   Ce message arrive immédiatement, sans délai d'attente.
   {{img:diag-protocol|Le profil ne correspond pas au compteur}}
2. Touchez {{S.fixMeter}}. L'étape {{S.meter}} propose le profil qui correspond au compteur
   reconnu.
3. Confirmez-le, saisissez la clé si nécessaire et touchez {{S.next}}.

## Acceptance
- Après la correction, le contrôle de la configuration affiche {{S.checkOk}}.
