// Language selection for the SPA. All four tables ship in the one bundle the device serves
// (measured at ~3 kB gzip each on top of the ~26 kB app); the page is sent to the phone straight
// from flash as gzip, so extra languages cost flash bytes and nothing else -- no RAM and no work
// on the ESP32.
//
// Every string goes through the `S` proxy below rather than a direct table import, so switching
// language is a state change in the running app, not a reload. German is the fallback for a key a
// translation is missing; build.mjs refuses to build when the key sets differ, so that fallback
// should never fire on a real device.
import { de } from "./de.js";
import { en } from "./en.js";
import { fr } from "./fr.js";
import { it } from "./it.js";

const TABLES = { de, fr, it, en };
const KEY = "gplug.lang";

// Picker order: the three Swiss national languages the meters are billed in, then English.
// Each label is written in its own language -- someone who lands in the wrong one has to be able
// to find their way out.
export const LANGS = [
  ["de", "Deutsch"],
  ["fr", "Français"],
  ["it", "Italiano"],
  ["en", "English"],
];

// Swiss regional locales, so Intl gives Swiss number and date conventions (12’843.60 for German
// and Italian, 12 843,60 for French) rather than the German-German or US defaults.
const LOCALES = { de: "de-CH", fr: "fr-CH", it: "it-CH", en: "en-CH" };

function detect() {
  try {
    const saved = localStorage.getItem(KEY);
    if (TABLES[saved]) return saved;
  } catch { /* private mode, ignore */ }
  for (const tag of navigator.languages || [navigator.language || ""]) {
    const code = String(tag).slice(0, 2).toLowerCase();
    if (TABLES[code]) return code;
  }
  return "de";
}

let lang = detect();
const listeners = new Set();

export const getLang = () => lang;
export const getLocale = () => LOCALES[lang];

export function setLang(code) {
  if (!TABLES[code] || code === lang) return;
  lang = code;
  try { localStorage.setItem(KEY, code); } catch { /* private mode, ignore */ }
  applyLang();
  for (const fn of listeners) fn(code);
}

// <html lang> drives the browser's own hyphenation, spell-check and screen-reader voice; it is
// wrong out of the box because index.html has to ship with some fixed value.
export function applyLang() {
  document.documentElement.lang = lang;
}

// Returns an unsubscribe function, so a component can `useEffect(() => onLangChange(fn), [])`.
export function onLangChange(fn) {
  listeners.add(fn);
  return () => listeners.delete(fn);
}

// A preset's name comes from the device (presets.json) and is deliberately technical and
// language-neutral -- "P1 DSMR", "Kamstrup DLMS Push". Only the encryption state is prose, so it
// is appended here from the preset's own flag rather than baked into the name on the device.
export function presetLabel(p) {
  if (!p) return null;
  if (typeof p === "string") return p;   // an id the device reports without a matching preset
  return p.name + " · " + (p.encrypted ? S.detectEncrypted : S.detectPlain);
}

export const S = new Proxy({}, {
  get(_, k) {
    const v = TABLES[lang][k];
    return v === undefined ? de[k] : v;
  },
});
