import { useEffect, useState } from "preact/hooks";
import { html } from "../h.js";
import { S } from "../strings.js";
import { api } from "../api.js";
import { clock } from "../fmt.js";
import { TabBar } from "./tabbar.js";
import { LiveTab } from "./live-tab.js";
import { HistTab } from "./hist-tab.js";
import { StreamTab } from "./stream-tab.js";
import { SetupTab } from "./setup-tab.js";

// Day-to-day home screen once the device is configured and on WiFi (see main.js routing).
// Session-only tab state, not reflected in the URL hash -- only #setup/#live are real routes
// (see main.js's comment on why), this is a local sub-view switch within "#live".
//
// /api/live + /api/ring are polled here once at 0.1 Hz and handed to whichever tab needs them,
// so the header's status pill and the tabs never disagree. Fast enough to feel live, slow enough
// not to matter on battery-backed phones or the device's own web server.
const POLL_MS = 10000;
const DIM_KEY = "gplug.dim";
const TITLES = { live: "screenLive", hist: "screenHist", stream: "screenStream", setup: "screenSetup" };

export function Live({ status, presets }) {
  const [tab, setTab] = useState("live");
  const [live, setLive] = useState(null);
  const [ring, setRing] = useState(null);
  const [err, setErr] = useState(null);
  const [now, setNow] = useState(clock());
  const [dim, setDim] = useState(() => {
    try { return localStorage.getItem(DIM_KEY) === "1"; } catch { return false; }
  });

  useEffect(() => {
    let stop = false;
    async function tick() {
      try {
        const [l, r] = await Promise.all([api.live(), api.ring()]);
        if (stop) return;
        setLive(l); setRing(r); setErr(null);
      } catch (e) { if (!stop) setErr(String(e.message || e)); }
      setNow(clock());
    }
    tick();
    const id = setInterval(tick, POLL_MS);
    return () => { stop = true; clearInterval(id); };
  }, []);

  useEffect(() => { document.getElementById("app").classList.add("live"); return () => document.getElementById("app").classList.remove("live"); }, []);

  function toggleDim() {
    const next = !dim;
    setDim(next);
    try { localStorage.setItem(DIM_KEY, next ? "1" : "0"); } catch { /* private mode, ignore */ }
  }

  const age = live?.age;
  // no_data is the firmware's verdict (same one that turns the LED red), so the pill and the LED
  // never disagree -- age alone stays null forever when no frame has ever arrived.
  const pill = live?.key_invalid ? ["err", S.keyWrong]
    : live?.no_data ? ["err", S.noData]
    : age == null ? ["warn", S.waitingData]
    : ["ok", S.dataOk];
  const wifi = status?.wifi?.connected ? `${S.wlan} · ${status.wifi.rssi} dBm` : `${S.wlan} · ${S.offline}`;
  const sub = [live?.smid, status?.hardware?.variant].filter(Boolean).join(" · ");

  return html`
    <div class="statusbar"><span>${now}</span><span><i class="dot"></i>${wifi}</span></div>
    <div class="screenhead">
      <div><div class="title">${S[TITLES[tab]]}</div><div class="sub">${sub || " "}</div></div>
      <span class="badge ${pill[0]}">${pill[1]}</span>
    </div>
    ${err && html`<div class="err">${err}</div>`}
    ${tab === "live" && html`<${LiveTab} live=${live} ring=${ring} />`}
    ${tab === "hist" && html`<${HistTab} live=${live} ring=${ring} status=${status} presets=${presets} />`}
    ${tab === "stream" && html`<${StreamTab} />`}
    ${tab === "setup" && html`<${SetupTab} status=${status} live=${live} presets=${presets} dim=${dim} onToggleDim=${toggleDim} />`}
    <${TabBar} tab=${tab} onChange=${setTab} />
    ${dim && html`<div class="dimlayer"></div>`}`;
}
