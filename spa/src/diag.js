import { html } from "./h.js";
import { S } from "./strings.js";

// The firmware's setup verdict (/api/live "diag", see GplugSmi::diag_) turned into what the user
// sees and which wizard step fixes it. Fix buttons are deep links (#setup/<step>, see main.js), so
// the same card works from the wizard's last step and from the Live tab.
const DIAG = {
  silent: { pill: S.noData, title: S.dgSilent, hint: S.dgSilentHint, fix: [["hardware", S.fixHardware]] },
  garbled: { pill: S.dgGarbledPill, title: S.dgGarbled, hint: S.dgGarbledHint, fix: [["meter", S.fixMeter], ["hardware", S.fixHardware]] },
  no_match: { pill: S.dgNoMatchPill, title: S.dgNoMatch, hint: S.dgNoMatchHint, fix: [["meter", S.fixMeter]] },
  key: { pill: S.keyWrong, title: S.dgKey, hint: S.dgKeyHint, fix: [["key", S.fixKey]] },
  // firmware without "diag": only the LED's no-data verdict is known
  nodata: { pill: S.noData, title: S.noData, hint: S.noDataHint, fix: [["meter", S.fixMeter], ["hardware", S.fixHardware]] },
};

export function diagOf(live) {
  if (!live) return null;
  if (live.diag) return live.diag;
  return live.key_invalid ? "key" : live.no_data ? "nodata" : live.age != null ? "ok" : "waiting";
}

export const diagInfo = (d) => DIAG[d] || null;

export function DiagCard({ diag }) {
  const d = DIAG[diag];
  if (!d) return null;
  return html`
    <div class="card diag">
      <div class="lbl orange">${S.dgLabel}</div>
      <div class="t">${d.title}</div>
      <p class="hint" style="margin:6px 0 0">${d.hint}</p>
      <div class="fixes">
        ${d.fix.map(([step, label], i) => html`
          <button class=${i === 0 ? "primary" : ""} onClick=${() => { location.hash = "#setup/" + step; }}>${label}</button>`)}
      </div>
    </div>`;
}
