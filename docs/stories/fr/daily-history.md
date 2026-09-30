---
id: daily-history
title: Voir l'énergie par jour, semaine, mois et année
chapter: daily
screen: history, history-regs
order: 20
mock: 2026-09-30
---
## Story
En tant que propriétaire d'un gPlug, je veux voir combien d'énergie j'ai soutirée et injectée sur un
jour, une semaine, un mois ou une année, afin de comparer des périodes.

## Steps
1. Touchez {{S.tabHist}} en bas.
2. Choisissez la période en haut : {{S.rangeDay}}, {{S.rangeWeek}}, {{S.rangeMonth}} ou
   {{S.rangeYear}}.
3. Le graphique {{S.energyPerBucket}} montre le soutirage vers le haut et l'injection vers le bas.
   En dessous figurent les totaux {{S.statImport}}, {{S.statExport}} et {{S.statSum}}.
   {{img:history|Historique d'une journée avec les totaux}}
4. Touchez {{S.registers}} pour voir toutes les valeurs actuelles du compteur avec leur code OBIS.
   {{img:history-regs|Liste des registres actuels}}

## Acceptance
- Chaque période affiche un graphique et les trois totaux.
- {{S.statSum}} correspond au soutirage moins l'injection.

## Notes
- Le gPlug enregistre une valeur toutes les 15 minutes. Il en conserve environ une année, y compris
  après un redémarrage ou une mise à jour du micrologiciel.
- {{S.timeEstimated}} apparaît lorsque des valeurs ont été enregistrées avant que le gPlug ait reçu
  l'heure depuis internet.
