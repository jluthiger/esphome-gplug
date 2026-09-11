import { html } from "../h.js";
import { S } from "../strings.js";

export function Welcome({ status, onNext }) {
  return html`
    <h2>${S.welcome}</h2>
    <p>${S.welcomeText}</p>
    ${status && html`
      <div class="card kv">
        <b>${S.version}</b><span class="num">${status.version || "?"}</span>
        <b>Gerät</b><span class="num">${status.hostname || "?"}</span>
      </div>`}
    <div class="nav"><button class="primary" onClick=${onNext}>${S.next}</button></div>`;
}
