---
id: setup-meter
title: Confermare il profilo del contatore e inserire la chiave
chapter: setup
screen: wizard-meter
order: 30
mock: 2026-09-30
hardware: gPlugK 2026-09-14
---
## Story
Come proprietario di un gPlug voglio scegliere il profilo del contatore adatto e salvare la chiave del
mio gestore di rete, affinché il gPlug possa leggere i valori del mio contatore.

## Steps
1. Nel passo {{S.meter}} il gPlug ascolta prima ciò che il contatore invia. Dopo pochi secondi la
   scheda {{S.detectTitle}} mostra {{S.detectFound}} e il protocollo riconosciuto.
2. Se un solo profilo è adatto, è già selezionato. Se ce ne sono diversi, scegliete il profilo del
   vostro gestore di rete. {{S.otherProfile}} mostra altri profili.
3. Se il contatore invia dati cifrati, inserite la chiave (GUEK) sotto {{S.key}}: 32 caratteri tra
   0–9 e A–F, come nella lettera del gestore di rete.
   {{img:wizard-meter|Contatore riconosciuto, profilo proposto e campo della chiave}}
4. {{S.values}} elenca i valori che il profilo legge.
5. Toccate {{S.next}}.

## Acceptance
- {{S.next}} diventa attivo solo quando è scelto un profilo e la chiave è valida.
- La procedura passa a {{S.done}} e lì verifica il collegamento con il contatore.

## Notes
- La chiave è salvata solo sul gPlug e non viene più mostrata.
- Se dopo mezzo minuto non arriva ancora nessun segnale, verificate il cavo. Alcuni gestori di rete
  devono prima attivare l'interfaccia cliente.
