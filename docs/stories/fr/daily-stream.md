---
id: daily-stream
title: Voir, copier et enregistrer les données brutes reçues
chapter: daily
screen: stream
order: 40
mock: 2026-09-30
---
## Story
En tant que propriétaire d'un gPlug intéressé par la technique, je veux voir les données que le
compteur envoie réellement, afin de montrer ce qui arrive en cas de problème.

## Steps
1. Touchez {{S.tabStream}} en bas. Vous voyez les dernières trames avec leur heure et {{S.crcOk}}
   ou {{S.crcFail}}.
2. Choisissez la vue en haut : pour les compteurs chiffrés {{S.streamRaw}} ou {{S.streamPlain}},
   pour les compteurs DSMR {{S.streamHex}} ou {{S.streamText}}.
3. L'interrupteur {{S.tailOn}} fige l'affichage pour lire tranquillement ; le gPlug continue
   d'enregistrer.
4. Touchez une trame pour l'ouvrir ; elle propose {{S.copy}} et {{S.frameTxt}}.
5. Cochez des trames ou touchez {{S.streamSelectAll}} et téléchargez-les dans la carte
   {{S.exportSel}}.
   {{img:stream|Flux de données avec les dernières trames}}

## Acceptance
- Une trame copiée ou enregistrée contient les octets en texte hexadécimal avec horodatage et CRC.

## Notes
- {{S.dlHint}}
