// Number and date formatting in the language the user picked. Everything here goes through Intl
// with a Swiss locale (i18n/index.js maps de -> de-CH and so on), which is both smaller than
// hand-rolled formatting and more correct than what this file used to do: it formatted
// German-German ("12 843,60") on a device sold in Switzerland, where German and Italian want
// "12’843.60" and French "12 843,60".
import { getLocale } from "./i18n/index.js";

// Intl formatters are expensive to construct and a history chart formats hundreds of labels, so
// they are built once per locale and option set. The cache key carries the locale, which is what
// makes a language switch pick up new formatters without any explicit invalidation.
const cache = new Map();
function fmt(kind, opts) {
  const key = kind + getLocale() + JSON.stringify(opts);
  let f = cache.get(key);
  if (!f) {
    f = kind === "n" ? new Intl.NumberFormat(getLocale(), opts) : new Intl.DateTimeFormat(getLocale(), opts);
    cache.set(key, f);
  }
  return f;
}

// "–" (en dash) for a value the device does not have, distinct from a real 0. The minus sign is
// normalised to U+2212: Intl emits an ASCII hyphen, which is visibly too short next to the digits
// at the sizes the Live screen uses.
export function num(v, digits = 0) {
  if (v == null || !isFinite(v)) return "–";
  const s = fmt("n", { minimumFractionDigits: digits, maximumFractionDigits: digits }).format(v);
  return s.replace(/^-/, "−");
}

export function clock(d = new Date()) {
  return fmt("d", { hour: "2-digit", minute: "2-digit", hour12: false }).format(d);
}

// The device stores quarter-hour indices since 2020-01-01 UTC and has no timezone of its own
// (ESPHome runs UTC), so every label is rendered in the viewer's local time.
export const QH_EPOCH = 1577836800;
export const qhDate = (qh) => new Date((QH_EPOCH + qh * 900) * 1000);

// A calendar date for prose ("3828 intervals stored, 3 Aug 2026 to 12 Sep 2026"). Date *inputs*
// keep their ISO value: <input type="date"> takes and returns ISO and renders it in the browser's
// own locale, which the page cannot influence.
export function dateShort(d) {
  return fmt("d", { year: "numeric", month: "short", day: "numeric" }).format(d);
}

// "45 s", "12 min", "3 h 20 min" -- for event-log entries written before the clock synced, where
// uptime is all the device knows. The unit abbreviations are the same in all four languages.
export function dur(sec) {
  if (sec < 90) return `${Math.round(sec)} s`;
  const m = Math.round(sec / 60);
  if (m < 90) return `${m} min`;
  return `${Math.floor(m / 60)} h ${String(m % 60).padStart(2, "0")} min`;
}

export function bucketLabel(range, qh) {
  const d = qhDate(qh);
  if (range === "day") return clock(d);
  if (range === "week") return fmt("d", { weekday: "short" }).format(d);
  if (range === "month") return fmt("d", { day: "numeric", month: "numeric" }).format(d);
  return fmt("d", { month: "short" }).format(d);
}
