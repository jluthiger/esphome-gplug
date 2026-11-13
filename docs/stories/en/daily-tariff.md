---
id: daily-tariff
title: Compare tariff and energy
chapter: daily
screen: history-tariff
order: 35
mock: 2026-11-13
---
## Story
As the owner of a gPlug with a dynamic tariff chosen I want to see each quarter hour's price next to
my energy and know what my day earned, or cost in grid fees.

## Steps
1. Tap {{S.tabHist}}. Below the energy chart is the {{S.tariffChartFeedIn}} (BKW) or
   {{S.tariffChartGrid}} (CKW) or {{S.tariffChartTotal}} card.
2. {{S.tariffPrev}} and {{S.tariffNext}} (‹ ›) change the day, as far as the gPlug has prices.
3. Move your finger across the chart: time, price, energy and amount of that quarter hour appear
   above it.

## Acceptance
- The line shows the price, the bars the energy of the same quarter hour: {{S.statExport}} for BKW,
  {{S.statImport}} for CKW.
- Quarter hours with a negative price are shaded, and below it says "{{S.tariffNeg(8)}}"
- The line below shows the energy, {{S.tariffAvg}} and {{S.tariffEarned}} (BKW), {{S.tariffCost}}
  (CKW) or {{S.tariffCostTotal}} (Swiss Tariff Hub) in francs for the day.
- If past quarter hours lack energy, a note says so, e.g. "{{S.tariffPartial(60, 96)}}"
- If the gPlug has no prices for a day, it says {{S.tariffNoPrices}}

## Notes
- The amounts are an estimate from your measurements and the published tariff, not a bill.
- CKW shows the grid fee only; the energy price comes on top of it on your bill.
