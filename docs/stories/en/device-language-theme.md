---
id: device-language-theme
title: Change the language and appearance
chapter: device
screen: device-lang, device-theme
order: 80
mock: 2026-09-30
---
## Story
As a user of the app I want to choose the language and a light or dark appearance, so that the app is
comfortable to read.

## Steps
1. On the {{S.tabDevice}} tab, open the {{S.language}} card and tap Deutsch, Français, Italiano or
   English.
   {{img:device-lang|Choosing the language}}
2. Open the {{S.theme}} card and choose {{S.themeLight}} or {{S.themeDark}}.
   {{img:device-theme|Choosing the appearance}}

## Acceptance
- The app switches at once, without reloading.
- The choice applies to this browser; another phone keeps its own.
