import { useState } from "preact/hooks";
import { html } from "../h.js";
import { TabBar } from "./tabbar.js";
import { LiveTab } from "./live-tab.js";
import { HistTab } from "./hist-tab.js";
import { StreamTab } from "./stream-tab.js";
import { SetupTab } from "./setup-tab.js";

// Day-to-day home screen once the device is configured and on WiFi (see main.js routing).
// Session-only tab state, not reflected in the URL hash -- only #setup/#live are real routes
// (see main.js's comment on why), this is a local sub-view switch within "#live".
export function Live({ status, presets, onOpenSetup }) {
  const [tab, setTab] = useState("live");

  return html`
    <${TabBar} tab=${tab} onChange=${setTab} />
    ${tab === "live" && html`<${LiveTab} />`}
    ${tab === "hist" && html`<${HistTab} status=${status} presets=${presets} />`}
    ${tab === "stream" && html`<${StreamTab} />`}
    ${tab === "setup" && html`<${SetupTab} status=${status} onOpenSetup=${onOpenSetup} />`}`;
}
