---
id: daily-tariff
title: Tarif und Energie vergleichen
chapter: daily
screen: history-tariff
order: 35
mock: 2026-11-13
---
## Story
Als Besitzer eines gPlug mit gewähltem dynamischem Tarif möchte ich den Preis jeder Viertelstunde
neben meiner Energie sehen und wissen, was mein Tag eingebracht oder an Netzentgelt gekostet hat.

## Steps
1. Tippen Sie auf {{S.tabHist}}. Unter dem Energie-Diagramm steht die Karte {{S.tariffChartFeedIn}}
   (BKW), {{S.tariffChartGrid}} (CKW) oder {{S.tariffChartTotal}}.
2. Mit {{S.tariffPrev}} und {{S.tariffNext}} (‹ ›) wechseln Sie den Tag, soweit der gPlug Preise hat.
3. Fahren Sie mit dem Finger über das Diagramm: Über dem Diagramm stehen Uhrzeit, Preis, Energie und
   Betrag dieser Viertelstunde.

## Acceptance
- Die Linie zeigt den Preis, die Balken die Energie derselben Viertelstunde: bei BKW die
  {{S.statExport}}, bei CKW der {{S.statImport}}.
- Viertelstunden mit negativem Preis sind schattiert, darunter steht
  „{{S.tariffNeg(8)}}“
- Die Zeile darunter zeigt die Energie, {{S.tariffAvg}} und {{S.tariffEarned}} (BKW) oder
  {{S.tariffCost}} (CKW) oder {{S.tariffCostTotal}} (Swiss Tariff Hub) in Franken für den Tag.
- Fehlt für vergangene Viertelstunden die Energie, sagt das ein Hinweis, etwa
  „{{S.tariffPartial(60, 96)}}“
- Hat der gPlug für einen Tag keine Preise, steht dort {{S.tariffNoPrices}}

## Notes
- Die Beträge sind eine Schätzung aus Ihren Messwerten und dem veröffentlichten Tarif, keine
  Abrechnung.
- CKW zeigt nur das Netzentgelt; der Energiepreis kommt auf Ihrer Rechnung dazu.
