---
id: daily-tariff
title: Comparer tarif et énergie
chapter: daily
screen: history-tariff
order: 35
mock: 2026-11-13
---
## Story
En tant que propriétaire d'un gPlug avec un tarif dynamique choisi, je veux voir le prix de chaque
quart d'heure à côté de mon énergie et savoir ce que ma journée a rapporté, ou coûté en redevance
réseau.

## Steps
1. Touchez {{S.tabHist}}. Sous le graphique d'énergie se trouve la carte {{S.tariffChartFeedIn}}
   (BKW), {{S.tariffChartGrid}} (CKW) ou {{S.tariffChartTotal}}.
2. {{S.tariffPrev}} et {{S.tariffNext}} (‹ ›) changent de jour, tant que le gPlug a des prix.
3. Glissez le doigt sur le graphique : l'heure, le prix, l'énergie et le montant de ce quart d'heure
   s'affichent au-dessus.

## Acceptance
- La ligne montre le prix, les barres l'énergie du même quart d'heure : l'{{S.statExport}} pour BKW,
  le {{S.statImport}} pour CKW.
- Les quarts d'heure à prix négatif sont ombrés, et en dessous s'affiche « {{S.tariffNeg(8)}} »
- La ligne en dessous montre l'énergie, le {{S.tariffAvg}} et la {{S.tariffEarned}} (BKW) ou la
  {{S.tariffCost}} (CKW), ou le {{S.tariffCostTotal}} (Swiss Tariff Hub), en francs pour la journée.
- S'il manque l'énergie de quarts d'heure passés, une remarque le dit, par exemple
  « {{S.tariffPartial(60, 96)}} »
- Si le gPlug n'a pas de prix pour un jour, s'affiche {{S.tariffNoPrices}}

## Notes
- Les montants sont une estimation à partir de vos mesures et du tarif publié, pas une facture.
- CKW ne montre que la redevance réseau ; le prix de l'énergie s'y ajoute sur votre facture.
