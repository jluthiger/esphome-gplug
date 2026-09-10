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
