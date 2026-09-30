---
id: device-reset-wifi
title: Riportare il gPlug in modalità di configurazione
chapter: device
screen: captive
order: 95
mock: 2026-09-30
hardware: gPlugK 2026-09-10
---
## Story
Come proprietario di un gPlug che non raggiunge più il suo Wi-Fi (nuovo router, password cambiata)
voglio riportarlo in modalità di configurazione per poterlo ricollegare.

## Steps
1. Tenete premuto il pulsante del gPlug per almeno 3 secondi.
2. Il gPlug dimentica i dati di accesso Wi-Fi, si riavvia e apre di nuovo la rete `gPlug-Setup`; la
   spia lampeggia in blu.
3. Collegatelo come nella prima configurazione e toccate {{C.save}}.

## Acceptance
- Il gPlug è raggiungibile nel nuovo Wi-Fi.
- Modello, profilo del contatore, chiave e storico restano; non serve ripetere la procedura.
