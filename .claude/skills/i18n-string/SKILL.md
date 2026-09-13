---
name: i18n-string
description: Add, rename or remove a UI string in the gPlug SPA across de/fr/it/en. Use for any new visible text in spa/src components.
---

# UI string

1. Pick a camelCase key describing meaning, not wording. Check `spa/src/i18n/de.js` for an existing
   one first.
2. Add it to all four tables at the matching position: `de.js` (reference), `fr.js`, `it.js`,
   `en.js`. Same type in every table; a function key takes the same number of arguments everywhere.
   Numbers and dates go through `fmt.js` (`Intl`), not string concatenation.
3. Swiss usage: Swiss German spelling (`ss`, no `ß`), Swiss French/Italian energy terms (kWh,
   "Zähler"/"compteur"/"contatore", "Netzbetreiber"/"gestionnaire de réseau"/"gestore di rete").
   Match the tone of neighbouring strings; keep them short — phone width.
4. Use as `S.key` / `S.key(arg)` via `import { S } from "../strings.js"`.
5. `cd spa && npm run build` — fails with the exact missing/mismatched keys. Removing a key: delete
   from all four and grep `spa/src` for leftovers.
