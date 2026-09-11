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
- **`#live`** – the day-to-day app (`src/live/`), four tabs behind a fixed bottom nav, with its own
  chrome (clock + WiFi strength, screen title, data-freshness pill). The tab is component state,
  deliberately *not* in the hash: only the two top-level screens are worth bookmarking.

| Tab | File | What |
|---|---|---|
| Live | `live/live-tab.js` | current power with direction, 1 h area chart from `/api/ring`, import/export counters, per-phase bars with V/A (hidden when the preset has no per-phase registers) |
| Verlauf | `live/hist-tab.js` | 60 Min from the RAM ring (10 s resolution), plus Tag/Woche/Monat/Jahr from `/api/history` – power for the short ranges, energy per bucket for the long ones. History is fetched once per range and cached, never on the 10 s poll, because it reads flash on the device |
| Datenstrom | `live/stream-tab.js` | the last 5 frames the device received, whatever the profile speaks: CRC verdict, body, copy/download, multi-select export. The two view modes follow the protocol – ciphertext vs decrypted APDU (hex) for DLMS, hex vs the telegram's own ASCII for DSMR, which the firmware signals with `encoding` |
| Setup | `live/setup-tab.js` | reachable addresses, WLAN ändern (reuses `steps/wifi.js`), GUEK status (masked, never fetched), Darstellung Hell/Dunkel (`theme.js`) |

With no hash yet (first load), the app picks a screen once `/api/status` answers: live view if
the device already has hardware + meter configured and is connected to WiFi, wizard otherwise. The
wizard's last step links to the live view; the wizard stays reachable at `#setup`.

Design source: the four tabs follow variant 1a of the "gPlug OBIS Monitor" Claude Design canvas.
Its fonts and dunkelgrün-only look are superseded (2026-09-11): one system-ui sans stack for the
whole app, monospace only in raw hex/telegram dumps, and a light + dark token set in `style.css`
(no colour literals outside the two token blocks).

Mock quirks: WiFi password `wrong` fails, GUEK starting with `dead` shows "Schlüssel ungültig".

## Device API used

| Method | Path | Body / result |
|---|---|---|
| GET | `/api/status` | `{version, hostname, uptime, heap, hardware?, meter?, wifi:{…}, time:{valid, epoch}, history:{ok, addr, size, sectors, slots, interval, count, seq, oldest_qh, newest_qh, erases, writes, crc_errors}}` |
| GET | `/api/presets` | `{variants:[…], presets:[…]}` (see `../firmware/components/gplug_smi/presets.json`) |
| GET | `/api/wifi/scan` | `[{ssid, rssi, secure}]` |
| POST | `/api/config/wifi` | `{ssid, psk}` |
| POST | `/api/config/hardware` | `{variant, pins:{rx, red, green, blue, button}}` |
| POST | `/api/config/meter` | `{preset, key?, descriptor:{schema, protocol, mode, baud, rx, serial_flags, buffer, obis[]}}` |
| GET | `/api/live` | `{smid, age, no_data, p, pi, po, ei, eo, key_invalid?, values:{…}, last_qh:{qh, values:{…}}}` — no per-phase power field; the Live tab takes that from `/api/ring`'s latest sample. `last_qh` is every register the meter sent as of the last stored quarter hour |
| GET | `/api/ring` | `{period:10, samples:[[pi,po,p1,p2,p3],…]}`, 360 samples = 1 h at 10 s resolution |
| GET | `/api/history?range=day\|week\|month\|year` | flash-backed 15-min history, downsampled server-side to ≤365 points: `{period:900, bucket, qh_epoch, epoch_valid, now_qh, count, pts:[[qh, d_ei_wh, d_eo_wh, p_min, p_max, p_avg, flags],…]}`. `qh` = quarter-hours since 2020-01-01Z, `null` when the record predates a clock sync; the energy deltas are `null` when a counter is absent or the chain is broken (meter swap, config change) |
| GET | `/api/frames` | `{protocol, encoding, cap, len, count, frames:[{i, age, ok, raw_len, raw_trunc, plain_len, plain_trunc},…]}` — metadata only, newest first. `protocol` is `dlms`/`dsmr`/`none`; `encoding` is `hex` (DLMS) or `text` (DSMR), i.e. what the readable view of a frame is. `ok` = HDLC FCS/HCS valid, or telegram CRC valid |
| GET | `/api/frames/<i>/raw\|plain` | `text/plain` body of one captured frame. DLMS: hex of the ciphertext / of the decrypted APDU. DSMR: hex of the telegram / the telegram verbatim — one capture, two views, nothing stored twice. 404 when the slot or that half is empty |

Source layout: `src/main.js` (routing + wizard shell + commit per step), `src/steps/*.js` (wizard),
`src/live/*.js` (the four tabs), `src/api.js`, `src/fmt.js` (German numbers + quarter-hour dates),
`src/strings.js` (German UI text), `src/style.css`.
