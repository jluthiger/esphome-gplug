# gPlug SPA – setup wizard

Preact + htm, no JSX, no framework build step. esbuild bundles everything into one
`dist/index.html` (and `.gz`) that the ESPHome firmware embeds and serves at `/`.

```
npm install
npm run presets   # regenerate mock/presets.json from ../gplug/**/script.txt
npm run build     # -> dist/index.html, dist/index.html.gz (prints sizes)
npm run dev       # mock device API + SPA on http://localhost:8080
```

Mock quirks: WiFi password `wrong` fails, GUEK starting with `dead` shows "Schlüssel ungültig".

## Wizard flow

Welcome → Gerät (variant + pins) → Smart Meter (preset + GUEK) → WLAN → Abschluss.

WiFi is last on purpose: everything else is saved while the phone is still on the
captive-portal AP; the final screen shows the new address.

## Device API used

| Method | Path | Body / result |
|---|---|---|
| GET | `/api/status` | `{version, hostname, hardware?, meter?, wifi:{connected, ssid, ip, rssi, error}}` |
| GET | `/api/presets` | `{variants:[…], presets:[…]}` (see `mock/presets.json`) |
| GET | `/api/wifi/scan` | `[{ssid, rssi, secure}]` |
| POST | `/api/config/wifi` | `{ssid, psk}` |
| POST | `/api/config/hardware` | `{variant, pins:{rx, red, green, blue, button}}` |
| POST | `/api/config/meter` | `{preset, key?, descriptor:{schema, protocol, mode, baud, rx, serial_flags, buffer, obis[]}}` |
| GET | `/api/live` | `{smid, age, p, p1, p2, p3, ei, eo, key_invalid?}` |

Source layout: `src/main.js` (wizard shell + commit per step), `src/steps/*.js`,
`src/api.js`, `src/strings.js` (German UI text), `src/style.css`.
