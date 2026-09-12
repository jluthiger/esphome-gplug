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

- **`#setup`** – the wizard: Welcome → Device (variant + pins) → Smart Meter (preset + GUEK) →
  Done (`src/steps/{welcome,hardware,meter,done}.js`). No separate Wi-Fi step: this SPA is
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
| History | `live/hist-tab.js` | Day/Week/Month/Year from `/api/history` – power for the day, energy per bucket for the longer ones, fetched once per range and cached, never on the 10 s poll, because they read flash on the device. Deliberately no "last hour" range: that is live data rather than stored history, and the Live tab draws exactly the same series one tap away (it was the same call to `hour.js` in both places) |
| Data Stream | `live/stream-tab.js` | the last 5 frames the device received, whatever the profile speaks: CRC verdict, body, copy/download, multi-select export. The two view modes follow the protocol – ciphertext vs decrypted APDU (hex) for DLMS, hex vs the telegram's own ASCII for DSMR, which the firmware signals with `encoding` |
| Setup | `live/setup-tab.js` | reachable addresses, Wi-Fi change (reuses `steps/wifi.js`), GUEK status (masked, never fetched), firmware update (`live/firmware-card.js`: header check, upload with progress, confirmation by image identity), event log (`live/log-card.js`), language, light/dark appearance (`theme.js`) |

With no hash yet (first load), the app picks a screen once `/api/status` answers: live view if
the device already has hardware + meter configured and is connected to WiFi, wizard otherwise. The
wizard stays reachable at `#setup`; `#setup/hardware`, `#setup/meter` and `#setup/key` jump to a
step directly (the last one with the stored key deliberately not kept).

**Setup check and the way back** (`src/diag.js`). The wizard's last step (`steps/done.js`) doesn't
hand over to the live view until the meter has been read: it polls `/api/live` and shows either
a "data received" message with the meter ID and value count, or – after the firmware's 60 s grace –
the diagnosis from its `diag` verdict with a button to the step that fixes it (table in
`../firmware/README.md`, "Setup diagnosis"). A "continue to live view anyway" option stays
available: a customer port the utility hasn't enabled yet is not a setup error. The Live tab shows
the same card when the problem appears later; there is deliberately no automatic redirect, since
"no data" is as often a cable or the meter as a wrong setting. When the device already holds a
GUEK, the meter step offers a "use stored key" toggle (on by default) and sends `keep_key` instead
of the key,
which the SPA never gets back – so fixing only the profile doesn't mean retyping 32 hex digits.

**The meter step proposes the profile** (`steps/meter.js`). Committing the hardware step sends the
variant's line parameters (`baud`, `parity`, `serial_flags` from `variants` in `presets.json`)
with the pins, so the firmware's UART runs right away and its header sniffer reports what the line
speaks in `/api/live` `detect` (`protocol_sniff.h` in the firmware). The meter step polls that and
follows it: exactly one of the variant's profiles fits → it is selected and shown alone, the user
only confirms (plus the GUEK when the line is encrypted); several fit (gPlugM has two DLMS
profiles) → the list is narrowed to those. Tapping a card ends the following; a "choose a
different profile" action and a "show all profiles" toggle widen the list on request. Until the
first frame the card shows a "waiting for signal" message, after 30 s without a byte it hints at
the cable / customer port. A firmware without `detect` gets the old manual list. The same sniffer
feeds the `protocol` diagnosis (shown as "wrong profile") when a configured profile contradicts
the line. Mock: `MOCK_DETECT=dsmr|dlms|none npm run dev`.

## Languages

German, French, Italian and English, all four in the one bundle the device serves. The page is
sent to the phone gzipped straight from flash and decompressed by the browser, so extra languages
cost flash bytes and nothing else -- no RAM on the ESP32 and no work for it. Measured: 26.5 kB
gzipped for German alone, 35.3 kB for all four. The firmware image grew by the same 8.8 kB and its
static RAM did not move at all, leaving ~396 kB free in the app slot.

- `src/i18n/{de,fr,it,en}.js` hold the tables, `src/i18n/index.js` the selection, and
  `src/strings.js` re-exports both so a component never imports a language directly.
- `S` is a proxy that reads through to the active table, with German as the fallback for a key a
  translation is missing. Switching language is a state change, not a reload: `setLang()` notifies
  the app root, which re-renders the tree (nothing is memoised, so every `S.x` read is fresh).
- The choice is stored per browser in `localStorage` under `gplug.lang`; without one, the phone's
  own `navigator.languages` decides, and German is the last resort. `<html lang>` follows.
- Numbers and dates go through `Intl` with a Swiss locale (`de-CH`, `fr-CH`, `it-CH`, `en-CH`), so
  German and Italian get `12’843.60` and French `12 843,60`. Before this, `fmt.js` formatted
  German-German (`12 843,60`) for every user, which is not a Swiss convention at all.
- `build.mjs` refuses to build when the tables disagree -- a missing key, a key one language has
  and another doesn't, a string where another language has a function, or a function taking a
  different number of arguments. A forgotten translation cannot reach a device.

Two things deliberately stay as they are. The `<input type="date">` fields in the export card
render in the *browser's* locale, not the page's; that is the control's own behaviour and the page
cannot influence it. And the preset names the device serves ("P1 DSMR", "Kamstrup DLMS Push") are
technical identifiers, not prose -- the SPA appends the encryption state in the user's language
from the preset's own `encrypted` flag (`presetLabel()`), which is why `scripts2presets.py` no
longer puts that qualifier in the name.

The French and Italian wording follows Swiss grid-operator usage rather than literal translation:
*soutirage*/*injection* and *prelievo*/*immissione* for the two directions, *courbe de charge* and
*curva di carico* for the load profile, RCP and CEL for the two self-consumption arrangements
(ZEV/LEG in German). **Both tables still want a native-speaker review** before this is put in
front of customers; the terminology was chosen deliberately, but only the German is a first
language here.

**The last hour comes from two places** (`src/hour.js`). `/api/ring` is 360 samples at 10 s held in
RAM, and RAM does not survive a restart -- so for a full hour after every reboot, *including every
firmware update*, the Live chart and the "60 min" view were empty while the same hour sat on flash
as 15-minute records. `lastHour()` merges them for the Live chart: the ring covers the recent part at full
resolution, stored records fill the rest, and slots neither can account for stay `null` so the
charts draw a gap rather than a line through zero, which would read as a measured 0 kW. The
stored part is a step function, one average per quarter hour, and the card says so instead of
passing it off as live data. The Live screen owns the day-range fetch and hands it to both tabs,
so the device is not asked for the same flash scan twice.

**Copying text needs a fallback on this device** (`src/dl.js`). `navigator.clipboard` exists only
in a *secure context*, and the gPlug serves plain HTTP on the local network, so on a real device
the modern one-liner is simply absent. Developing against `http://localhost` hides this, because
localhost counts as secure: the copy buttons worked all the way through development and did
nothing on hardware. `copyText()` therefore tries the modern API, falls back to a off-screen
textarea with `document.execCommand("copy")`, and **returns whether it worked** so a button never
claims a copy that did not happen. The event log also offers a plain download, which has no
secure-context restriction at all and is the better thing to attach to a support mail anyway.

**Direction has its own colour pair** (`--import` / `--export` in `style.css`): red for energy drawn
from the grid, green for energy fed back. It is deliberately not `--orange`, which marks errors --
an import reading is not a fault, and the two must not be confusable. Everything that shows a
direction uses it: the live power figure and its caption, both counters, the per-phase bars, the
history bars, and the day/week/month/year totals. The Live chart splits its path at the zero
crossing so each side carries its own colour rather than the whole hour taking the sign of the
last sample.

Design source: the four tabs follow variant 1a of the "gPlug OBIS Monitor" Claude Design canvas.
Its fonts and dark-green-only look are superseded (2026-09-11): one system-ui sans stack for the
whole app, monospace only in raw hex/telegram dumps, and a light + dark token set in `style.css`
(no colour literals outside the two token blocks).

Mock quirks: WiFi password `wrong` fails, GUEK starting with `dead` shows a "key invalid" message.
`MOCK_OTA_PASSWORD=x npm run dev` puts the mock's `/update` behind Basic auth (user `admin`); a
successful upload "reboots" the mock (API down 6 s), bumps its `build` and takes on the uploaded
file's image identity. The mock's event log carries the events a bench device never produces -- a crash, a watchdog, a
brownout, a storage failure, a flapping Wi-Fi -- because those are the ones whose rendering has to
be right when a support case finally needs them. `MOCK_OTA_ROLLBACK=1` accepts and reboots but keeps the old identity, the
way the bootloader behaves with an image that crash-loops -- the only way to exercise the card's
rollback verdict without breaking a real device. The mock's `diag` uses an
8 s grace; `MOCK_DIAG=silent|garbled|no_match npm run dev` makes the first meter config fail that
way and the next one work, to walk the whole fix loop.

## Device API used

| Method | Path | Body / result |
|---|---|---|
| GET | `/api/status` | `{version, hostname, uptime, build, app, ota_auth, heap, hardware?, meter?, wifi:{…}, time:{valid, epoch}, history:{ok, addr, size, sectors, slots, interval, count, seq, oldest_qh, newest_qh, erases, writes, crc_errors}}`. `app` is the first 16 hex digits of the running image's ELF SHA-256; the firmware card reads the same bytes at offset `0xB0` of the file it uploads and compares the two after the reboot, because `build` does not move when only embedded assets change |
| GET | `/api/presets` | `{variants:[…], presets:[…]}` (see `../firmware/components/gplug_smi/presets.json`) |
| GET | `/api/wifi/scan` | `[{ssid, rssi, secure}]` |
| POST | `/api/config/wifi` | `{ssid, psk}` |
| POST | `/api/config/hardware` | `{variant, pins:{rx, red, green, blue, button}}` |
| POST | `/api/config/meter` | `{preset, key? \| keep_key?, descriptor:{schema, protocol, mode, baud, rx, serial_flags, buffer, obis[]}}` |
| GET | `/api/live` | `{smid, age, no_data, diag, rx_bytes, p, pi, po, ei, eo, key_invalid?, values:{…}, last_qh:{qh, values:{…}}}` — no per-phase power field; the Live tab takes that from `/api/ring`'s latest sample. `last_qh` is every register the meter sent as of the last stored quarter hour |
| GET | `/api/ring` | `{period:10, samples:[[pi,po,p1,p2,p3],…]}`, 360 samples = 1 h at 10 s resolution |
| GET | `/api/history?range=day\|week\|month\|year` | flash-backed 15-min history, downsampled server-side to ≤365 points: `{period:900, bucket, qh_epoch, epoch_valid, now_qh, count, pts:[[qh, d_ei_wh, d_eo_wh, p_min, p_max, p_avg, flags],…]}`. `qh` = quarter-hours since 2020-01-01Z, `null` when the record predates a clock sync; the energy deltas are `null` when a counter is absent or the chain is broken (meter swap, config change) |
| GET | `/api/history.csv?from=<qh>&to=<qh>&format=…` | Load-profile download (History tab → "export load profile"): every stored 15-min record, `text/csv` as attachment. `format=full` (default): `;`-separated, integers in Wh / W, local interval times; column names are the implementation's own (German), see `../firmware/components/gplug_smi/history_csv.h`. Two other `format` values select the CKW customer-portal shape instead (tab separated, `DD.MM.YY HH:MM`, kWh with 3 decimals, one direction per file) for a line-by-line compare against a real CKW export -- one value per direction, exact spelling in `../firmware/components/gplug_smi/gplug_smi.cpp`. `from` inclusive, `to` exclusive quarter-hour indices (the tab derives them from a local date range); no params = everything including records without a timestamp. Navigated to, not fetched. Columns and the rules for empty cells: `../firmware/components/gplug_smi/history_csv.h`. Mock: `MOCK_HIST_DAYS=n` (default 40) |
| GET | `/api/log` | `{now, uptime, cap, events:[{t, up, code, detail, value, repeat},…]}`, oldest first. The device stores numbers, not text: `live/log-card.js` turns them into sentences in the user's language, so the wording can change without touching firmware. `t` 0 means the record predates any clock sync and `up` (uptime) is all there is. Mock: `MOCK_LOG=empty npm run dev` for the empty state |
| GET | `/api/frames` | `{protocol, encoding, cap, len, count, frames:[{i, age, ok, raw_len, raw_trunc, plain_len, plain_trunc},…]}` — metadata only, newest first. `protocol` is `dlms`/`dsmr`/`none`; `encoding` is `hex` (DLMS) or `text` (DSMR), i.e. what the readable view of a frame is. `ok` = HDLC FCS/HCS valid, or telegram CRC valid |
| POST | `/update` | ESPHome's `ota.web_server`: multipart field `update` = `firmware.ota.bin`, `text/plain` "Update Successful!"/"Update Failed!", then the device reboots. HTTP Basic (user `admin`) when `ota_auth`. Not under `/api` – it is ESPHome's handler, not `gplug_smi`'s |
| GET | `/api/frames/<i>/raw\|plain` | `text/plain` body of one captured frame. DLMS: hex of the ciphertext / of the decrypted APDU. DSMR: hex of the telegram / the telegram verbatim — one capture, two views, nothing stored twice. 404 when the slot or that half is empty |

Source layout: `src/main.js` (routing + wizard shell + commit per step), `src/steps/*.js` (wizard),
`src/live/*.js` (the four tabs), `src/api.js`, `src/fmt.js` (locale-aware numbers and dates via
`Intl`, quarter-hour helpers), `src/strings.js` + `src/i18n/*` (UI text in four languages),
`src/style.css`.
