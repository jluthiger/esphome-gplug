import { useEffect, useState } from "preact/hooks";
import { html } from "../h.js";
import { S } from "../strings.js";
import { api } from "../api.js";
import { dur } from "../fmt.js";
import { copyText, download } from "../dl.js";

// The device's persistent event log (/api/log, event_log.h). The device stores codes, not
// sentences: 12 bytes per record, and the words are put together here so they come out in the
// user's language and can be reworded without touching firmware.
//
// Fetched once when the card is opened, never polled: these are rare events, and the Setup tab is
// somewhere a user goes deliberately.

const EV = { 1: "evBoot", 2: "evOta", 3: "evWifiUp", 4: "evWifiLost", 5: "evMeterLost",
             6: "evMeterOk", 7: "evConfig", 8: "evStorage", 9: "evButton" };
// esp_reset_reason_t as event_log.h renumbers it; the three watchdog flavours read the same to a
// user, so they share a string.
const RR = { 1: "rrPoweron", 2: "rrExt", 3: "rrSw", 4: "rrPanic", 5: "rrWdt", 6: "rrWdt", 7: "rrWdt",
             9: "rrBrownout" };
const CFG = { 1: "cfgHardware", 2: "cfgMeter", 3: "cfgWifi" };
const ST = { 1: "stWrite", 2: "stUnavail" };
// EV_METER_LOST detail, the diag verdict that replaced "ok" -- the same wording the diagnosis card
// uses, so the log and the card agree.
const WHY = { 1: "dgKey", 2: "dgNoMatch", 3: "dgProtocol", 4: "dgGarbled", 5: "dgSilent" };

function detailOf(e) {
  if (e.code === 1) return S[RR[e.detail] || "rrUnknown"];
  if (e.code === 5) return S[WHY[e.detail]] || null;
  if (e.code === 7) return S[CFG[e.detail]] || null;
  if (e.code === 8) return S[ST[e.detail]] || null;
  if (e.code === 3 && e.value) return `${-e.value} dBm`;
  return null;
}

// A record carries a wall clock only if the device knew one when it was written; everything
// before the first clock sync of the very first boot has uptime and nothing else.
function when(e) {
  if (e.t) return new Date(e.t * 1000).toLocaleString(undefined, { dateStyle: "short", timeStyle: "short" });
  return S.logUptime(dur(e.up));
}

function asText(events) {
  return events.map((e) => {
    const d = detailOf(e);
    return [when(e), S[EV[e.code]] || S.evUnknown, d, e.repeat ? S.logRepeat(e.repeat + 1) : null]
      .filter(Boolean).join(" · ");
  }).join("\n");
}

export function LogCard() {
  const [events, setEvents] = useState(null);
  const [err, setErr] = useState(null);
  const [copied, setCopied] = useState(null);   // null | "ok" | "fail"

  useEffect(() => {
    let stop = false;
    api.log()
      .then((r) => { if (!stop) setEvents(r.events || []); })
      .catch((e) => { if (!stop) setErr(String(e.message || e)); });
    return () => { stop = true; };
  }, []);

  // navigator.clipboard does not exist on a plain-HTTP origin, which is exactly how the device is
  // reached, so copyText() falls back and reports whether it worked. When even that fails the card
  // points at the download, which has no such restriction.
  async function copy() {
    const ok = await copyText(asText(newestFirst));
    setCopied(ok ? "ok" : "fail");
    setTimeout(() => setCopied(null), 3000);
  }

  // The device appends, so the newest is last; support reads the other way round.
  const newestFirst = events ? [...events].reverse() : [];

  return html`
    <div class="card">
      <div class="lbl">${S.logTitle}</div>
      ${err && html`<div class="err">${err}</div>`}
      ${!events && !err && html`<p class="hint"><span class="spin"></span> ${S.loading}</p>`}
      ${events && !events.length && html`<p class="hint">${S.logEmpty}</p>`}
      ${newestFirst.map((e) => {
        const d = detailOf(e);
        return html`
          <div class="ev">
            <div class="ev-t">${when(e)}</div>
            <div class="ev-w">
              <span class="ev-n ${e.code === 1 && e.detail === 4 ? "orange" : ""}">${S[EV[e.code]] || S.evUnknown}</span>
              ${d && html`<span class="ev-d">${d}</span>`}
              ${e.repeat > 0 && html`<span class="ev-r">${S.logRepeat(e.repeat + 1)}</span>`}
            </div>
          </div>`;
      })}
      ${events && events.length > 0 && html`
        <p class="hint" style="margin:10px 0 0">${S.logHint}</p>
        <div class="row" style="margin-top:10px">
          <button onClick=${copy}>${copied === "ok" ? S.copied : copied === "fail" ? S.copyFailedShort : S.logCopy}</button>
          <button class="primary" onClick=${() => download("gplug-log.txt", asText(newestFirst))}>${S.logDownload}</button>
        </div>
        ${copied === "fail" && html`<p class="hint" style="margin:8px 0 0">${S.copyFailed}</p>`}`}
    </div>`;
}
