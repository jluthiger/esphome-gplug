---
id: device-firmware
title: Mettre à jour le micrologiciel
chapter: device
screen: device-fw
order: 50
mock: 2026-09-30
hardware: gPlugK 2026-09-25
---
## Story
En tant que propriétaire d'un gPlug, je veux installer un nouveau micrologiciel sans brancher de
câble, afin que mon appareil reçoive corrections et nouvelles fonctions.

## Steps
1. Dans l'onglet {{S.tabDevice}}, ouvrez la carte {{S.fwTitle}}. Elle affiche la version installée.
2. Touchez {{S.fwRelCheck}}. S'il existe une version plus récente, une ligne comme
   {{S.fwRelAvail("0.6.0", "0.7.0")}} apparaît avec le lien {{S.fwRelNotes}}.
   {{img:device-fw|Une mise à jour est disponible}}
3. Touchez {{S.fwRelInstall("0.7.0")}} et confirmez avec {{S.fwRelConfirm}}.
   {{img:device-fw-confirm|Confirmer l'installation}}
4. Le gPlug télécharge lui-même le micrologiciel et redémarre. Laissez-le sous tension.
   {{img:device-fw-done|Micrologiciel mis à jour}}

## Acceptance
- La carte affiche {{S.fwDone}} et la nouvelle version.
- Les réglages et l'historique sont conservés ; seul le quart d'heure en cours est perdu.

## Notes
- Le gPlug ne cherche jamais de mises à jour de lui-même, seulement quand vous touchez
  {{S.fwRelCheck}}.
- Pour installer un fichier depuis votre ordinateur, utilisez {{S.fwPick}} et {{S.fwInstall}}.
  Prenez le fichier au format OTA (`.ota.bin`).
- Si un mot de passe OTA est défini, la carte demande le {{S.fwPassword}}.
