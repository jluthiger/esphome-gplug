// German number formatting: decimal comma, thin-space thousands ("12 843", "2,60").
export function de(v, digits = 0) {
  if (v == null || !isFinite(v)) return "–";
  const [int, frac] = Math.abs(v).toFixed(digits).split(".");
  const grouped = int.replace(/\B(?=(\d{3})+(?!\d))/g, " ");
  return (v < 0 ? "−" : "") + grouped + (frac ? "," + frac : "");
}

export function clock(d = new Date()) {
  return String(d.getHours()).padStart(2, "0") + ":" + String(d.getMinutes()).padStart(2, "0");
}

// The device stores quarter-hour indices since 2020-01-01 UTC and has no timezone of its own
// (ESPHome runs UTC), so every label is rendered in the viewer's local time.
export const QH_EPOCH = 1577836800;
export const qhDate = (qh) => new Date((QH_EPOCH + qh * 900) * 1000);

const WD = ["So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"];
const MON = ["Jan", "Feb", "Mär", "Apr", "Mai", "Jun", "Jul", "Aug", "Sep", "Okt", "Nov", "Dez"];

export function bucketLabel(range, qh) {
  const d = qhDate(qh);
  if (range === "day") return clock(d);
  if (range === "week") return WD[d.getDay()];
  if (range === "month") return d.getDate() + "." + (d.getMonth() + 1) + ".";
  return MON[d.getMonth()];
}
