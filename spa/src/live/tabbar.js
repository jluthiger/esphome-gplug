import { html } from "../h.js";
import { S } from "../strings.js";

// Icon shapes are pure CSS squares with different corner radii, as in the design canvas.
const TABS = [
  ["live", "tabLive", "50%"],
  ["hist", "tabHist", "3px"],
  ["stream", "tabStream", "999px"],
  ["setup", "tabSetup", "6px"],
];

export function TabBar({ tab, onChange }) {
  return html`
    <nav class="tabbar"><div class="inner">
      ${TABS.map(([id, key, r]) => html`
        <button class="tab ${tab === id ? "active" : ""}" onClick=${() => onChange(id)}>
          <i style="border-radius:${r}"></i>${S[key]}
        </button>`)}
    </div></nav>`;
}
