import { useEffect, useState } from "preact/hooks";
import { WIDE } from "./layout.js";

// Whether the wide-screen layout applies. CSS alone cannot give a laptop a side rail or a frame
// list beside the open frame, so the few components that need a different structure there ask
// this; everything that only needs different sizes stays in style.wide.css. Called once, by the
// live screen, and handed down as a prop, so a resize re-renders from one place.
export function useWide() {
  const mq = typeof window !== "undefined" && window.matchMedia ? window.matchMedia(WIDE) : null;
  const [wide, setWide] = useState(!!mq?.matches);
  useEffect(() => {
    if (!mq) return;
    const on = () => setWide(mq.matches);
    // Safari before 14 (iOS 13 phones) has only the older addListener; this hook runs at every
    // width, so a missing addEventListener would take the phone's live screen down with it.
    if (mq.addEventListener) mq.addEventListener("change", on); else mq.addListener(on);
    on();
    return () => { if (mq.removeEventListener) mq.removeEventListener("change", on); else mq.removeListener(on); };
  }, []);
  return wide;
}
