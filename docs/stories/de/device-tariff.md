---
id: device-tariff
title: Den dynamischen Tarif einschalten
chapter: device
screen: device-tariff
order: 45
mock: 2026-11-13
---
## Story
Als Besitzer eines gPlug im Netz der BKW oder der CKW möchte ich den Viertelstunden-Tarif meines
Energieversorgers neben meiner Energie sehen, damit ich weiss, wann sich Einspeisen oder Bezug lohnt.

## Steps
1. Öffnen Sie im Tab {{S.tabDevice}} die Karte {{S.tariffTitle}}.
2. Wählen Sie {{S.tariffBkw}} oder {{S.tariffCkw}}. Die Wahl gilt sofort.
3. Oder wählen Sie {{S.tariffSth}}, wenn Sie ein Konto bei swisstariffhub.ch haben: Tragen Sie
   {{S.tariffSthId}} und {{S.tariffSthKey}} ein und tippen Sie auf {{S.tariffSthSave}}.

## Acceptance
- Unter der Auswahl steht, welcher Tarif gezeigt wird: {{S.tariffKindFeedIn}} oder {{S.tariffKindGrid}}
- Nach wenigen Sekunden steht dort „{{S.tariffFetched(0)}}“, und im Tab {{S.tabHist}} erscheint die
  Karte mit dem Tarif.
- Kann der gPlug den Anbieter nicht erreichen, steht dort {{S.tariffErrNet}}
- Lehnt Swiss Tariff Hub den Schlüssel ab, steht dort {{S.tariffErrAuth}}
- Der gespeicherte Schlüssel wird nie wieder angezeigt; das leere Feld zeigt {{S.tariffSthKeySet}}.
- {{S.tariffOff}} schaltet den Abruf ab; der gPlug verbindet sich dann mit keinem Anbieter mehr.

## Notes
- Der gPlug braucht dafür Internet. Er ruft den Tarif selbst ab, höchstens einmal pro Stunde, solange
  Preise fehlen, und behält ihn nur bis zum nächsten Neustart.
- BKW veröffentlicht nur den heutigen Tag; frühere Tage zeigt der gPlug nur, wenn er sie selbst
  abgerufen hat. Bei CKW holt er die letzten drei Tage nach.
