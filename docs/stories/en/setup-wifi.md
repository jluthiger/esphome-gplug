---
id: setup-wifi
title: Connect the gPlug to your Wi-Fi
chapter: setup
screen: captive
order: 10
mock: 2026-09-30
hardware: gPlugK 2026-09-16 (Android)
---
## Story
As the new owner of a gPlug I want to connect it to my home Wi-Fi so that I can open its app in a
browser.

## Steps
1. Plug the gPlug into the meter's customer interface. As long as it knows no Wi-Fi, it opens a
   network of its own called `gPlug-Setup`.
2. Connect your phone to `gPlug-Setup`. The phone opens the setup page by itself; if it does not,
   open `http://192.168.4.1/` in the browser.
3. Tap your home network in the list and enter its password under {{C.set}}.
   {{img:captive|Setup page with the networks found}}
4. Note the address under {{C.addr}} – or tap {{C.copy}}. It looks like
   `http://gplug-xxxxxx.local/`.
5. Tap {{C.save}}. Take a screenshot now: the page closes as soon as the phone switches back to your
   home Wi-Fi.
   {{img:captive-saved|After saving: the gPlug's address from now on}}
6. Connect the phone to your home Wi-Fi again and open the address you noted.

## Acceptance
- The address you noted shows the gPlug's setup assistant.
- The `gPlug-Setup` network disappears shortly afterwards.

## Notes
- The setup page follows the phone's language.
- If the address does not open, look for the gPlug in your router's device list under its name
  `gplug-xxxxxx` and open its IP address.
