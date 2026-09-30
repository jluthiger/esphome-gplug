---
id: trouble-key
title: «La chiave non è corretta»
chapter: trouble
screen: diagnosis, device-key
order: 60
mock: 2026-09-30
---
## Story
Come proprietario di un gPlug con contatore cifrato i cui dati non si lasciano decifrare voglio
inserire la chiave giusta.

## Steps
1. La scheda {{S.dgLabel}} mostra {{S.dgKey}}; in alto a destra c'è {{S.keyWrong}}. Nella sezione
   {{S.tabDevice}} la scheda {{S.setupKey}} mostra {{S.keyInvalidBadge}}.
   {{img:diag-key|La chiave non è corretta}}
2. Toccate {{S.fixKey}}. Il campo della chiave è vuoto.
3. Inserite la chiave (GUEK) dalla lettera del gestore di rete – 32 caratteri, senza spazi – e
   toccate {{S.next}}.

## Acceptance
- Dopo la correzione la verifica della configurazione mostra {{S.checkOk}} e la scheda
  {{S.setupKey}} mostra {{S.keySet}}.

## Notes
- Non confondete la GUEK con altre chiavi della lettera (per esempio la chiave di autenticazione).
