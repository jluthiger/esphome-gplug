import { html } from "../h.js";
import { S } from "../strings.js";

// End of the wizard. Operational stuff (live readings, reachable address, WLAN change) lives in
// the Live view now (steps/live.js) -- that's the screen the user actually returns to, this one
// is seen once per setup.
export function Done({ onOpenLive }) {
  return html`
    <h2>${S.done}</h2>
    <p>${S.doneText}</p>
    <div class="nav"><button class="primary" style="width:100%" onClick=${onOpenLive}>${S.openLive}</button></div>`;
}
