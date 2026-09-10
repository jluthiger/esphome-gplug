import { html } from "../h.js";
import { S } from "../strings.js";

const TABS = [
  ["live", () => S.tabLive],
  ["hist", () => S.tabHist],
  ["stream", () => S.tabStream],
  ["setup", () => S.tabSetup],
];

export function TabBar({ tab, onChange }) {
  return html`
    <div class="tabbar">
      ${TABS.map(([id, label]) => html`
        <button class="tab ${tab === id ? "active" : ""}" onClick=${() => onChange(id)}>${label()}</button>`)}
    </div>`;
}
