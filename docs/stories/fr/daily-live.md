---
id: daily-live
title: Lire la puissance actuelle et les index
chapter: daily
screen: live
order: 10
mock: 2026-09-30
hardware: gPlugK 2026-09-14
---
## Story
En tant que propriétaire d'un gPlug, je veux voir d'un coup d'œil combien d'électricité je soutire ou
j'injecte en ce moment, afin de comprendre ma consommation.

## Steps
1. Ouvrez l'adresse du gPlug. L'application démarre sur l'onglet {{S.tabLive}}.
2. En haut figure la {{S.activePower}} en kW avec son sens : {{S.drawFromGrid}} ou
   {{S.feedToGrid}}.
3. Le graphique en dessous montre la dernière heure ({{S.ago60}} à {{S.now}}). Touchez ou survolez
   le graphique pour lire un instant précis.
4. Les cartes {{S.importLbl}} et {{S.exportLbl}} affichent les index en kWh, tels que le compteur
   les affiche.
5. Avec les profils triphasés, {{S.phases}} affiche la puissance, la tension et le courant par phase.
   {{img:live|Vue en direct avec puissance, index et phases}}

## Acceptance
- {{S.dataOk}} s'affiche en haut à droite ; les valeurs se mettent à jour toutes les quelques
  secondes.
- Les index correspondent à l'affichage du compteur.

## Notes
- Après un redémarrage du gPlug, la partie du graphique antérieure au redémarrage provient de la
  mémoire de l'appareil (une moyenne par 15 minutes).
