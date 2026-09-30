---
id: device-key
title: Vérifier ou remplacer la clé enregistrée
chapter: device
screen: device-key, wizard-meter
order: 30
mock: 2026-09-30
hardware: gPlugK 2026-09-25
---
## Story
En tant que propriétaire d'un gPlug avec un compteur chiffré, je veux vérifier que la clé enregistrée
correspond à celle de la lettre de mon gestionnaire de réseau, et la remplacer au besoin, sans jamais
la voir en clair.

## Steps
1. Dans l'onglet {{S.tabDevice}}, la carte {{S.setupKey}} indique si la clé est {{S.keySet}} ou
   {{S.keyInvalidBadge}}.
   {{img:device-key|Carte Clé}}
2. Pour comparer, ouvrez `#setup/meter` (l'adresse du gPlug suivie de `#setup/meter`).
   {{S.keepKey}} est activé et affiche un identifiant de la clé enregistrée.
3. Ouvrez {{S.keyCheck}}, saisissez la clé de la lettre et touchez {{S.keyCheckBtn}}.
   {{img:wizard-meter-keep|Comparer la clé enregistrée avec la lettre}}
4. Pour remplacer la clé, désactivez {{S.keepKey}}, saisissez la nouvelle sous {{S.key}} et touchez
   {{S.next}}.

## Acceptance
- La comparaison affiche {{S.keyMatch}} ou {{S.keyMismatch}}.
- Après le remplacement, le contrôle de la configuration affiche {{S.checkOk}}.

## Notes
- {{S.keyNote}}
