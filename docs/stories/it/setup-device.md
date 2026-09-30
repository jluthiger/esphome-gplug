---
id: setup-device
title: Avviare la procedura e scegliere il dispositivo
chapter: setup
screen: wizard-welcome, wizard-hardware
order: 20
mock: 2026-09-30
---
## Story
Come proprietario di un gPlug appena collegato voglio indicare quale modello possiedo, affinché
comunichi correttamente con l'interfaccia cliente del mio contatore.

## Steps
1. Aprite l'indirizzo del gPlug. La prima volta la procedura si apre con {{S.welcome}}. Mostra la
   versione del firmware e il nome del dispositivo. Toccate {{S.next}}.
   {{img:wizard-welcome|Pagina di benvenuto della procedura}}
2. Sotto {{S.hardware}} scegliete il vostro modello, per esempio gPlugK. La denominazione è sul
   contenitore.
   {{img:wizard-hardware|Scelta del modello}}
3. {{S.pins}} serve solo se avete modificato voi stessi il gPlug. Altrimenti lasciate i valori
   proposti.
4. Toccate {{S.next}}.

## Acceptance
- La procedura passa al passo {{S.meter}}.
- Il modello scelto è salvato, anche se lasciate ora la procedura.
