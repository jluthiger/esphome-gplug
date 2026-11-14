---
id: daily-tariff
title: Confrontare tariffa ed energia
chapter: daily
screen: history-tariff
order: 35
mock: 2026-11-13
---
## Story
Come proprietario di un gPlug con una tariffa dinamica scelta voglio vedere il prezzo di ogni quarto
d'ora accanto alla mia energia e sapere quanto ha reso la mia giornata, o quanto è costata in
corrispettivo di rete.

## Steps
1. Tocchi {{S.tabHist}}. Sotto il grafico dell'energia c'è la scheda {{S.tariffChartFeedIn}} (BKW)
   {{S.tariffChartGrid}} (CKW) o {{S.tariffChartTotal}}.
2. {{S.tariffPrev}} e {{S.tariffNext}} (‹ ›) cambiano giorno, finché il gPlug ha prezzi.
3. Passi il dito sul grafico: sopra compaiono ora, prezzo, energia e importo di quel quarto d'ora.

## Acceptance
- La linea mostra il prezzo, le barre l'energia dello stesso quarto d'ora: l'{{S.statExport}} per
  BKW, il {{S.statImport}} per CKW.
- I quarti d'ora con prezzo negativo sono ombreggiati, e sotto compare «{{S.tariffNeg(8)}}»
- La riga sotto mostra l'energia, il {{S.tariffAvg}} e la {{S.tariffEarned}} (BKW) o il
  {{S.tariffCost}} (CKW), o il {{S.tariffCostTotal}} (Swiss Tariff Hub), in franchi per la giornata.
- Se manca l'energia di quarti d'ora passati, lo dice una nota, per esempio
  «{{S.tariffPartial(60, 96)}}»
- Se il gPlug non ha prezzi per un giorno, compare {{S.tariffNoPrices}}

## Notes
- Gli importi sono una stima dalle sue misure e dalla tariffa pubblicata, non una fattura.
- CKW mostra solo il corrispettivo di rete; il prezzo dell'energia si aggiunge in fattura.
