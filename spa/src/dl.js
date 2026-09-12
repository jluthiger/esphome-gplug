// Getting text out of the app: to the clipboard, or to a file.
//
// The clipboard part exists because navigator.clipboard is only defined in a *secure context*.
// The gPlug serves plain HTTP on the local network, so on a real device that API is simply absent
// and the modern one-liner silently does nothing -- while localhost, where this was developed, is
// a secure context by definition and hid the problem. Hence the execCommand fallback, and hence
// copyText() reporting whether anything actually happened instead of assuming it did.

export async function copyText(text) {
  // Available over HTTPS and on localhost; the nicer path when it exists.
  if (navigator.clipboard?.writeText) {
    try {
      await navigator.clipboard.writeText(text);
      return true;
    } catch { /* permission denied or not allowed in this context: fall through */ }
  }
  // The old way, still the only way on a plain-HTTP origin. The textarea has to be in the document
  // and visible enough to be selectable, so it is parked off-screen rather than hidden; readOnly
  // keeps the on-screen keyboard away on mobile, and setSelectionRange is what iOS needs.
  try {
    const ta = document.createElement("textarea");
    ta.value = text;
    ta.readOnly = true;
    ta.style.cssText = "position:fixed;top:0;left:-9999px;opacity:0";
    document.body.appendChild(ta);
    ta.select();
    ta.setSelectionRange(0, text.length);
    const ok = document.execCommand("copy");
    document.body.removeChild(ta);
    return ok;
  } catch {
    return false;
  }
}

// A download has no secure-context requirement, so this is the path that always works -- and the
// one that actually suits sending a log or a capture to support.
export function download(name, text) {
  const a = document.createElement("a");
  a.href = URL.createObjectURL(new Blob([text], { type: "text/plain" }));
  a.download = name;
  a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 2000);
}
