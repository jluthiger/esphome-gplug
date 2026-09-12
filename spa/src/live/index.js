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
import { initialTheme, saveTheme } from "../theme.js";
import { diagInfo, diagOf } from "../diag.js";

// Day-to-day home screen once the device is configured and on WiFi (see main.js routing).
// Session-only tab state, not reflected in the URL hash -- only #setup/#live are real routes
// (see main.js's comment on why), this is a local sub-view switch within "#live".
//
// /api/live + /api/ring are polled here once at 0.1 Hz and handed to whichever tab needs them,
// so the header's status pill and the tabs never disagree. Fast enough to feel live, slow enough
// not to matter on battery-backed phones or the device's own web server.
const POLL_MS = 10000;
// The stored 15-min history is only needed to fill the part of the last hour the RAM ring cannot
// cover, which matters after a restart and stops mattering an hour later. Re-read occasionally
// rather than on the live poll: it reads flash on the device.
const HIST_MS = 300000;
const TITLES = { live: "screenLive", hist: "screenHist", stream: "screenStream", setup: "screenSetup" };

export function Live({ status, presets }) {
  const [tab, setTab] = useState("live");
  const [live, setLive] = useState(null);
  const [ring, setRing] = useState(null);
  const [day, setDay] = useState(null);
  const [err, setErr] = useState(null);
  const [now, setNow] = useState(clock());
  const [theme, setTheme] = useState(initialTheme);

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

  useEffect(() => {
    let stop = false;
    async function readHistory() {
      try {
        const h = await api.history("day");
        if (!stop) setDay(h);
      } catch { /* the live view works without it; the charts just start where the ring does */ }
    }
    readHistory();
    const id = setInterval(readHistory, HIST_MS);
    return () => { stop = true; clearInterval(id); };
  }, []);

  useEffect(() => { document.getElementById("app").classList.add("live"); return () => document.getElementById("app").classList.remove("live"); }, []);

  function pickTheme(t) { saveTheme(t); setTheme(t); }

  const age = live?.age;
  // diag is the firmware's verdict (the same one that turns the LED red), so the pill and the LED
  // never disagree -- age alone stays null forever when no frame has ever arrived.
  const problem = diagInfo(diagOf(live));
  const pill = problem ? ["err", problem.pill]
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
    ${tab === "live" && html`<${LiveTab} live=${live} ring=${ring} day=${day} />`}
    ${tab === "hist" && html`<${HistTab} live=${live} ring=${ring} day=${day} status=${status} presets=${presets} />`}
    ${tab === "stream" && html`<${StreamTab} />`}
    ${tab === "setup" && html`<${SetupTab} status=${status} live=${live} presets=${presets} theme=${theme} onTheme=${pickTheme} />`}
    <${TabBar} tab=${tab} onChange=${setTab} />`;
}
