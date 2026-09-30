---
id: daily-stream
title: Vedere, copiare e salvare i dati grezzi ricevuti
chapter: daily
screen: stream
order: 40
mock: 2026-09-30
---
## Story
Come proprietario di un gPlug interessato alla tecnica voglio vedere i dati che il contatore invia
davvero, per mostrare cosa arriva in caso di problemi.

## Steps
1. Toccate {{S.tabStream}} in basso. Vedete gli ultimi frame con l'ora e {{S.crcOk}} o
   {{S.crcFail}}.
2. Scegliete la vista in alto: per i contatori cifrati {{S.streamRaw}} o {{S.streamPlain}}, per i
   contatori DSMR {{S.streamHex}} o {{S.streamText}}.
3. L'interruttore {{S.tailOn}} ferma la visualizzazione per leggere con calma; il gPlug continua a
   registrare.
4. Toccate un frame per aprirlo; offre {{S.copy}} e {{S.frameTxt}}.
5. Selezionate dei frame o toccate {{S.streamSelectAll}} e scaricateli nella scheda
   {{S.exportSel}}.
   {{img:stream|Flusso di dati con gli ultimi frame}}

## Acceptance
- Un frame copiato o salvato contiene i byte come testo esadecimale con marca temporale e CRC.

## Notes
- {{S.dlHint}}
