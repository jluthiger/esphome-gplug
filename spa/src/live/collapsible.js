import { useState } from "preact/hooks";
import { html } from "../h.js";

// A Setup card whose title row opens and closes it. The open/closed choice is per browser
// (localStorage, like language and theme), never on the device.
//
// A closed card does not render its body at all, so a body that fetches on mount (event log,
// memory) makes no request until it is opened and stops polling when it is closed again. State
// that must survive closing therefore belongs in the component that renders the Collapsible, not
// in its children -- the firmware card keeps its upload state that way.
//
// `locked` holds the card open with the toggle disabled: while a firmware upload runs, or while
// the meter key is invalid. It is not stored, so the user's own choice comes back afterwards.

const KEY = "gplug.setupOpen";

function load() {
  try { return JSON.parse(localStorage.getItem(KEY)) || {}; } catch { return {}; }
}

export function Collapsible({ id, title, summary, open: dflt = false, locked, children }) {
  // Only cards the user has toggled are stored, so a changed default reaches everyone else.
  const [open, setOpen] = useState(() => load()[id] ?? dflt);
  const shown = open || !!locked;
  const body = `coll-${id}`;

  function toggle() {
    setOpen(!open);
    try { localStorage.setItem(KEY, JSON.stringify({ ...load(), [id]: !open })); } catch { /* private mode, ignore */ }
  }

  return html`
    <div class="card">
      <button class="coll" aria-expanded=${shown} aria-controls=${shown ? body : undefined}
        disabled=${!!locked} onClick=${toggle}>
        <span class="lbl">${title}</span>
        ${!shown && summary && html`<span class="csum">${summary}</span>`}
        <i class="chev" aria-hidden="true"></i>
      </button>
      ${shown && html`<div id=${body}>${children}</div>`}
    </div>`;
}
