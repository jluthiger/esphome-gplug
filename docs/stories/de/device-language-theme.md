---
id: device-language-theme
title: Sprache und Darstellung ändern
chapter: device
screen: device-lang, device-theme
order: 80
mock: 2026-09-30
---
## Story
Als Benutzer der App möchte ich Sprache und helle oder dunkle Darstellung wählen, damit ich die App
bequem lesen kann.

## Steps
1. Öffnen Sie im Tab {{S.tabDevice}} die Karte {{S.language}} und tippen Sie auf Deutsch, Français,
   Italiano oder English.
   {{img:device-lang|Sprache wählen}}
2. Öffnen Sie die Karte {{S.theme}} und wählen Sie {{S.themeLight}} oder {{S.themeDark}}.
   {{img:device-theme|Darstellung wählen}}

## Acceptance
- Die App wechselt sofort, ohne neu zu laden.
- Die Wahl gilt für diesen Browser; ein anderes Telefon behält seine eigene.
