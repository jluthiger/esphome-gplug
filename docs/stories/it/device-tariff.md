---
id: device-tariff
title: Attivare la tariffa dinamica
chapter: device
screen: device-tariff
order: 45
mock: 2026-11-13
---
## Story
Come proprietario di un gPlug nella rete di BKW o di CKW voglio vedere la tariffa al quarto d'ora del
mio fornitore accanto alla mia energia, per sapere quando conviene immettere o prelevare.

## Steps
1. Nella scheda {{S.tabDevice}} apra la scheda {{S.tariffTitle}}.
2. Scelga {{S.tariffBkw}} o {{S.tariffCkw}}. La scelta vale subito.
3. Oppure scelga {{S.tariffSth}} se ha un conto su swisstariffhub.ch: inserisca {{S.tariffSthId}} e
   {{S.tariffSthKey}} e tocchi {{S.tariffSthSave}}.

## Acceptance
- Sotto la scelta è indicata la tariffa mostrata: {{S.tariffKindFeedIn}} oppure {{S.tariffKindGrid}}
- Dopo pochi secondi compare «{{S.tariffFetched(0)}}», e nella scheda {{S.tabHist}} appare la
  scheda della tariffa.
- Se il gPlug non raggiunge il fornitore, compare {{S.tariffErrNet}}
- Se Swiss Tariff Hub rifiuta la chiave, compare {{S.tariffErrAuth}}
- La chiave salvata non viene mai più mostrata; il campo vuoto indica {{S.tariffSthKeySet}}.
- {{S.tariffOff}} ferma lo scaricamento; il gPlug non contatta più alcun fornitore.

## Notes
- Il gPlug ha bisogno di internet. Scarica la tariffa da sé, al massimo una volta all'ora finché
  mancano prezzi, e la conserva solo fino al prossimo riavvio.
- BKW pubblica solo il giorno corrente; il gPlug mostra i giorni precedenti solo se li ha scaricati
  da sé. Per CKW recupera gli ultimi tre giorni.
