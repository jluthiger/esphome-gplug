import { useState } from "preact/hooks";
import { html } from "./h.js";

// Which sample of a chart the user is pointing at -- mouse hover, a finger dragged across it, or
// the arrow keys -- shared by all three charts so they behave the same.
//
// The charts draw into their measured size at 1:1 (box.js), but the first render still uses the
// fallback size, so the position is taken as a fraction of the element's on-screen width rather
// than from offsetX in viewBox units.
//
// The selection is kept as a distance from the newest sample, not an index from the left: the Live
// chart grows while the ring fills and the heap chart while the device's trend fills, and counting
// from the right keeps the guide where the user put it while samples are added at that end. The
// readout then says what is under the guide now, which is what a pointer resting there means.
//
// `bars` switches the mapping from "nearest point" (a line with n points spanning the width) to
// "the slot under the pointer" (n bars of equal width).
export function useScrub(n, bars) {
  const [st, setSt] = useState(null);   // { back, key } or null when nothing is selected
  const set = (back, key) => setSt((s) => (s && s.back === back && s.key === key ? s : { back, key }));
  const hide = () => setSt(null);
  const clamp = (i) => Math.max(0, Math.min(n - 1, i));

  const at = (e) => {
    const r = e.currentTarget.getBoundingClientRect();
    const f = r.width ? (e.clientX - r.left) / r.width : 1;
    set(n - 1 - clamp(bars ? Math.floor(f * n) : Math.round(f * (n - 1))), false);
  };

  const i = st && n > 0 ? clamp(n - 1 - st.back) : null;
  const props = {
    tabindex: 0,
    onPointerDown: (e) => {
      // Capture keeps a finger that drifts off the chart's edge scrubbing; touch-action: pan-y in
      // the CSS leaves vertical drags to the page, which then cancels the pointer.
      if (e.pointerType !== "mouse") e.currentTarget.setPointerCapture?.(e.pointerId);
      at(e);
    },
    onPointerMove: (e) => { if (e.pointerType === "mouse" || e.buttons) at(e); },
    // Lifting the finger ends the reading; a mouse keeps it until it leaves the chart.
    onPointerUp: (e) => { if (e.pointerType !== "mouse") hide(); },
    onPointerCancel: hide,
    onPointerLeave: (e) => { if (e.pointerType === "mouse") hide(); },
    // A keyboard reading ends with focus, a mouse reading with pointerleave.
    onBlur: () => { if (st?.key) hide(); },
    onKeyDown: (e) => {
      if (!n) return;
      const cur = i ?? n - 1;
      const big = Math.max(1, Math.round(n / 12));
      const to = {
        ArrowLeft: cur - (e.shiftKey ? big : 1), ArrowRight: cur + (e.shiftKey ? big : 1),
        PageUp: cur - big, PageDown: cur + big, Home: 0, End: n - 1,
      }[e.key];
      if (e.key === "Escape" && st) { hide(); e.preventDefault(); return; }
      if (to === undefined) return;
      e.preventDefault();
      set(n - 1 - clamp(to), true);
    },
  };
  return { i, props };
}

// The line above a chart that carries the reading. It keeps its height when empty, so the chart
// does not move under the finger that starts a scrub. The region is always polite: switching
// aria-live on in the same render as the text changes is not announced reliably, and "polite"
// already lets a screen reader drop the intermediate readings of a fast run of key presses.
export function Readout({ text }) {
  return html`<div class="readout" aria-live="polite">${text || ""}</div>`;
}
