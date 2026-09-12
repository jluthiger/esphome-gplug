import { useEffect, useState } from "preact/hooks";

// Measured size of an element, for SVGs that draw in the size they are given.
//
// Both charts used a fixed 320 x h viewBox with preserveAspectRatio="none", which maps whatever
// width the element happens to have onto 320 units. On a phone that is nearly 1:1 and invisible;
// on the wider desktop column it stretches every stroke, bar and corner radius horizontally.
// Drawing into the measured pixel size keeps the mapping at 1:1 at any width, so one set of
// coordinates works for both and the desktop layer is free to change the chart's height.
export function useBox(ref, w0, h0) {
  const [box, setBox] = useState([w0, h0]);
  useEffect(() => {
    const el = ref.current;
    if (!el || typeof ResizeObserver === "undefined") return;
    // The observed size comes from CSS (width:100%, fixed height); the viewBox this feeds does not
    // affect layout, so there is no resize loop.
    const ro = new ResizeObserver(([e]) => {
      const { width, height } = e.contentRect;
      if (width > 0 && height > 0) setBox(([w, h]) =>
        Math.round(width) === w && Math.round(height) === h ? [w, h] : [Math.round(width), Math.round(height)]);
    });
    ro.observe(el);
    return () => ro.disconnect();
  }, [ref]);
  return box;
}
