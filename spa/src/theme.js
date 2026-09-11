// Light/dark theme: data-theme on <html> selects the token set in style.css. Stored per browser
// (localStorage); until the user picks one in Setup, follow the OS preference.
const KEY = "gplug.theme";

export function initialTheme() {
  try {
    const t = localStorage.getItem(KEY);
    if (t === "light" || t === "dark") return t;
  } catch { /* private mode, ignore */ }
  return window.matchMedia?.("(prefers-color-scheme: light)").matches ? "light" : "dark";
}

export function applyTheme(t) {
  document.documentElement.dataset.theme = t;
  document.querySelector('meta[name="color-scheme"]')?.setAttribute("content", t);
}

export function saveTheme(t) {
  applyTheme(t);
  try { localStorage.setItem(KEY, t); } catch { /* private mode, ignore */ }
}
