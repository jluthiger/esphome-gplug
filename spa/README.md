# gPlug SPA – setup wizard

Preact + htm, no JSX, no framework build step. esbuild bundles everything into one gzipped
`index.html` that the ESPHome firmware embeds (bundled inside `components/gplug_smi`) and serves at `/`.

```
npm install
npm run presets   # regenerate ../firmware/components/gplug_smi/presets.json from ../gplug/**/script.txt
npm run build     # -> dist/index.html (for the mock), and ../firmware/components/gplug_smi/spa.html.gz (what the firmware embeds; committed)
npm run dev       # mock device API + SPA on http://localhost:8080
```

## Two screens

`src/main.js` routes between two top-level screens, kept in `location.hash` so switching is a
state change, not a page reload:

- **`#setup`** – the wizard: Welcome → Gerät (variant + pins) → Smart Meter (preset + GUEK) →
  Abschluss (`src/steps/{welcome,hardware,meter,done}.js`). No separate WLAN step: this SPA is
  only ever reached after the device has already joined WiFi (ESPHome's stock `captive_portal`
  handles that first join before `gplug_smi`'s handler, and thus this SPA, resumes serving `/`).
  Everything is saved to the device as each step is left, so an interrupted wizard doesn't lose
  earlier steps.
- **`#live`** – the live view (`src/steps/live.js`): current power, import/export counters,
  per-phase power (hidden on single-phase installs), a 1 h sparkline, and the reachable
  address / "WLAN ändern" panel (`src/steps/wifi.js`, reused from the wizard).

With no hash yet (first load), the app picks a screen once `/api/status` answers: live view if
the device already has hardware + meter configured and is connected to WiFi, wizard otherwise. The
wizard's last step links to the live view; the live view's "Einrichtung" button links back to the
wizard (variant/preset change, or a WiFi network switch).

Mock quirks: WiFi password `wrong` fails, GUEK starting with `dead` shows "Schlüssel ungültig".

## Device API used

| Method | Path | Body / result |
|---|---|---|
| GET | `/api/status` | `{version, hostname, hardware?, meter?, wifi:{connected, ssid, ip, rssi, error}}` |
| GET | `/api/presets` | `{variants:[…], presets:[…]}` (see `../firmware/components/gplug_smi/presets.json`) |
| GET | `/api/wifi/scan` | `[{ssid, rssi, secure}]` |
| POST | `/api/config/wifi` | `{ssid, psk}` |
| POST | `/api/config/hardware` | `{variant, pins:{rx, red, green, blue, button}}` |
| POST | `/api/config/meter` | `{preset, key?, descriptor:{schema, protocol, mode, baud, rx, serial_flags, buffer, obis[]}}` |
| GET | `/api/live` | `{smid, age, no_data, p, ei, eo, key_invalid?}` — no per-phase field; the live view takes that from `/api/ring`'s latest sample instead |
| GET | `/api/ring` | `{period:10, samples:[[pi,po,p1,p2,p3],…]}`, 360 samples = 1 h at 10 s resolution |

Source layout: `src/main.js` (routing + wizard shell + commit per step), `src/steps/*.js`,
`src/api.js`, `src/strings.js` (German UI text), `src/style.css`.
