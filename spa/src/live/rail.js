import { html } from "../h.js";
import { S } from "../strings.js";
import { TABS } from "./tabbar.js";

// Navigation for wide screens: the four tabs down the left edge, where a pointer expects them and
// where they cost no height, with the clock and Wi-Fi line the phone shows in its status bar at the
// foot of the rail. Same tabs and icons as the phone's bottom bar (TABS), different place.
export function Rail({ tab, onChange, now, wifi }) {
  return html`
    <nav class="rail">
      <div class="rail-brand">gPlug<span>${S.brand}</span></div>
      <div class="rail-tabs">
        ${TABS.map(([id, key, r]) => html`
          <button class="rail-tab ${tab === id ? "active" : ""}" aria-current=${tab === id ? "page" : undefined} onClick=${() => onChange(id)}>
            <i style="border-radius:${r}"></i>${S[key]}
          </button>`)}
      </div>
      <div class="rail-foot">
        <div class="num">${now}</div>
        <div><i class="dot"></i>${wifi}</div>
      </div>
    </nav>`;
}
