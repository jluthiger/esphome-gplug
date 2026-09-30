---
id: daily-csv
title: Exporter la courbe de charge en CSV
chapter: daily
screen: history-csv
order: 30
mock: 2026-09-30
---
## Story
En tant que propriétaire d'un gPlug, je veux télécharger les valeurs quart-horaires enregistrées sous
forme de fichier, afin de vérifier ma facture d'électricité ou de faire le décompte dans un
regroupement pour la consommation propre.

## Steps
1. Touchez {{S.tabHist}} et ouvrez la carte {{S.csvTitle}}. Même fermée, sa ligne indique la
   période enregistrée.
2. Choisissez {{S.csvFrom}} et {{S.csvTo}}.
3. Touchez {{S.csvDownload}}.
   {{img:history-csv|Exporter la courbe de charge}}

## Acceptance
- Le navigateur enregistre un fichier CSV avec les index, l'énergie et la puissance par quart
  d'heure, séparés par des points-virgules.
- Si {{S.csvFrom}} est après {{S.csvTo}}, {{S.csvOrder}} s'affiche et rien n'est téléchargé.

## Notes
- {{S.csvAll}} inclut aussi les intervalles sans horodatage.
