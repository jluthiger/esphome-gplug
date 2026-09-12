import { QH_EPOCH } from "./fmt.js";

// The last 60 minutes, from both places the device keeps power data.
//
// /api/ring is 360 samples at 10 s in RAM, and RAM does not survive a restart: for an hour after
// every reboot -- including every firmware update -- the Live chart and the "60 min" view had
// nothing to draw, even though the same hour sits on flash as 15-minute records. This merges the
// two: the ring covers the recent part at full resolution, the stored records fill whatever is
// left, and slots neither can account for stay null so the charts can draw a gap instead of
// inventing a line through it.
//
// The stored part is necessarily a step function -- one average per quarter hour -- which is why
// the callers say so rather than passing it off as live data.

const WINDOW_S = 3600;

export function lastHour(ring, hist, nowSec = Date.now() / 1000) {
  const period = ring?.period || 10;
  const slots = Math.round(WINDOW_S / period);
  const samples = ring?.samples || [];
  const vals = new Array(slots).fill(null);

  // The ring ends "now" and runs backwards; anything older than the window is outside this view.
  for (let i = 0; i < samples.length && i < slots; i++) {
    const s = samples[samples.length - 1 - i];
    vals[slots - 1 - i] = s[0] - s[1];
  }

  // Fill the rest from storage. `pts` are [qh, dEi, dEo, p_min, p_max, p_avg, flags] with qh in
  // quarter hours since 2020-01-01Z; HF_NO_DATA (16) means the interval recorded nothing, which is
  // a gap and not a zero.
  let fromStore = 0;
  const covered = Math.min(samples.length, slots);
  if (hist?.pts?.length && covered < slots) {
    const avg = new Map();
    for (const p of hist.pts) if (p[0] !== null && !(p[6] & 16)) avg.set(p[0], p[5]);
    const windowStart = nowSec - WINDOW_S;
    for (let i = 0; i < slots - covered; i++) {
      const t = windowStart + i * period;
      const v = avg.get(Math.floor((t - QH_EPOCH) / 900));
      if (v !== undefined) { vals[i] = v; fromStore++; }
    }
  }

  // Leading nulls are simply "before what we know", not a hole inside the data: drop them so the
  // chart starts where the data does.
  let first = vals.findIndex((v) => v !== null);
  if (first < 0) first = vals.length;
  return { vals: vals.slice(first), fromStore, live: covered };
}
