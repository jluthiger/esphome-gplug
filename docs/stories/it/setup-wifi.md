---
id: setup-wifi
title: Collegare il gPlug al Wi-Fi
chapter: setup
screen: captive
order: 10
mock: 2026-09-30
hardware: gPlugK 2026-09-16 (Android)
---
## Story
Come nuovo proprietario di un gPlug voglio collegarlo al Wi-Fi di casa per poter aprire la sua app nel
browser.

## Steps
1. Inserite il gPlug nell'interfaccia cliente del contatore. Finché non conosce nessun Wi-Fi, apre
   una propria rete chiamata `gPlug-Setup`.
2. Collegate il telefono a `gPlug-Setup`. Il telefono apre da solo la pagina di configurazione; se
   non lo fa, aprite `http://192.168.4.1/` nel browser.
3. Toccate la vostra rete di casa nell'elenco e inserite la password sotto {{C.set}}.
   {{img:captive|Pagina di configurazione con le reti trovate}}
4. Annotate l'indirizzo sotto {{C.addr}} – oppure toccate {{C.copy}}. Ha la forma
   `http://gplug-xxxxxx.local/`.
5. Toccate {{C.save}}. Fate ora uno screenshot: la pagina si chiude non appena il telefono torna al
   Wi-Fi di casa.
   {{img:captive-saved|Dopo il salvataggio: il futuro indirizzo del gPlug}}
6. Ricollegate il telefono al Wi-Fi di casa e aprite l'indirizzo annotato.

## Acceptance
- L'indirizzo annotato mostra la procedura di configurazione del gPlug.
- La rete `gPlug-Setup` scompare poco dopo.

## Notes
- La pagina di configurazione segue la lingua del telefono.
- Se l'indirizzo non si apre, cercate il gPlug nell'elenco dei dispositivi del router con il suo nome
  `gplug-xxxxxx` e aprite il suo indirizzo IP.
