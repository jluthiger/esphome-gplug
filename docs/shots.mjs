// Takes the manual's screenshots from the mock device (spa/mock/server.mjs) with Playwright, per
// language at phone width, into img/<lang>/<name>.png. Regenerated rather than committed: after a UI
// change `npm run shots` brings every language up to date, and CI does the same before publishing.
//
// shots.json lists runs. Each run starts a fresh mock (its `env` selects a scenario such as
// MOCK_DIAG=silent) and walks the UI like a user would; `{"shot": name}` saves the screen. Labels
// in actions are i18n keys ("S.next", "C.save"), resolved per language, so one script serves all four.
// Needs spa/dist (cd ../spa && npm run build).
import { spawn } from "node:child_process";
import { createServer } from "node:http";
import { readFileSync, mkdirSync, existsSync, copyFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { chromium } from "playwright";
import { de } from "../spa/src/i18n/de.js";
import { en } from "../spa/src/i18n/en.js";
import { fr } from "../spa/src/i18n/fr.js";
import { it } from "../spa/src/i18n/it.js";

const here = dirname(fileURLToPath(import.meta.url));
const spa = join(here, "..", "spa");
const LANGS = ["de", "en", "fr", "it"];
const S = { de, en, fr, it };
const captiveHtml = readFileSync(join(here, "..", "firmware", "components", "captive_portal", "captive.html"), "utf8");
const C = new Function(`return ${captiveHtml.match(/var L = (\{[\s\S]*?\n\});/)[1]}`)();
const { viewport, sequences, runs } = JSON.parse(readFileSync(join(here, "shots.json"), "utf8"));
// Optional arguments: run names to take, and --lang=xx to take one language only (while writing a run).
const args = process.argv.slice(2);
const only = args.filter((a) => !a.startsWith("--"));
const langs = (args.find((a) => a.startsWith("--lang="))?.slice(7).split(",")) || LANGS;

if (!existsSync(join(spa, "dist", "index.html"))) {
  console.error("spa/dist/index.html missing: cd ../spa && npm run build");
  process.exit(1);
}

const expand = (steps) => steps.flatMap((s) => {
  if (!s.use) return [s];
  if (!sequences[s.use]) throw new Error(`unknown sequence "${s.use}"`);
  return expand(sequences[s.use]);
});
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let nextPort = 18080;

// `args` fills a function key, e.g. S.fwRelInstall with ["0.7.0"] -> "0.7.0 installieren".
function label(lang, ref, args) {
  const m = /^(S|C)\.(\w+)$/.exec(ref);
  if (!m) return ref;
  let v = (m[1] === "S" ? S : C)[lang][m[2]];
  if (typeof v === "function" && args) v = v(...args);
  if (typeof v !== "string") throw new Error(`${ref}: not a text key in ${lang}`);
  return v;
}

async function startMock(env) {
  const port = nextPort++;
  const proc = spawn(process.execPath, ["mock/server.mjs"], { cwd: spa, env: { ...process.env, ...env, PORT: String(port) }, stdio: "ignore" });
  for (let i = 0; i < 50; i++) {
    try { if ((await fetch(`http://localhost:${port}/api/status`)).ok) return { port, stop: () => proc.kill() }; } catch { /* not up yet */ }
    await sleep(100);
  }
  proc.kill();
  throw new Error("mock did not start");
}

// The captive portal runs in the firmware, not in the mock; its page only needs /config.json (the
// scan) and a /wifisave that answers.
function startCaptive() {
  const port = nextPort++;
  const cfg = { name: "gplug-a1b2c3", mac: "A1:B2:C3:D4:E5:F6", aps: [{},
    { ssid: "Zuhause", rssi: -48, lock: true }, { ssid: "Zuhause-Gast", rssi: -63, lock: true }, { ssid: "Nachbar", rssi: -81, lock: true }] };
  const srv = createServer((req, res) => {
    if (req.url.startsWith("/config.json")) { res.writeHead(200, { "content-type": "application/json" }); res.end(JSON.stringify(cfg)); }
    else if (req.url.startsWith("/wifisave")) { res.writeHead(200); res.end("ok"); }
    else { res.writeHead(200, { "content-type": "text/html" }); res.end(captiveHtml); }
  }).listen(port);
  return { port, stop: () => srv.close() };
}

async function act(page, lang, step, dir) {
  const find = (ref) => page.getByText(label(lang, ref, step.args), { exact: step.exact !== false }).filter({ visible: true }).first();
  if (step.goto !== undefined) await page.goto(new URL(step.goto, page.url()).href);
  else if (step.reload) await page.reload();
  else if (step.api) {
    // Puts the mock into a state the walk would take long to reach, e.g. Wi-Fi connected.
    const [method, path] = step.api.split(" ");
    await fetch(new URL(path, page.url()), { method, body: JSON.stringify(step.body || {}), headers: { "content-type": "application/json" } });
  }
  else if (step.click) await find(step.click).click();
  else if (step.css) await page.locator(step.css).filter({ visible: true }).first().click();
  else if (step.fill) await page.locator(step.fill).filter({ visible: true }).first().fill(step.value);
  else if (step.waitFor) await find(step.waitFor).waitFor({ timeout: step.timeout || 20000 });
  else if (step.wait) await sleep(step.wait);
  else if (step.scroll) await find(step.scroll).scrollIntoViewIfNeeded();
  else if (step.top) await page.evaluate(() => window.scrollTo(0, 0));
  else if (step.shot) {
    await sleep(400);   // let transitions and the chart settle
    const path = join(dir, `${step.shot}.png`);
    // `card`: only the card whose title is that label (a Device-tab card, say), not the whole screen.
    if (step.card) {
      const card = page.locator(".card").filter({ has: page.getByText(label(lang, step.card), { exact: true }) }).last();
      await card.scrollIntoViewIfNeeded();
      await card.screenshot({ path });
    } else await page.screenshot({ path, fullPage: !!step.full });
  } else throw new Error(`unknown step ${JSON.stringify(step)}`);
}

const browser = await chromium.launch();
let count = 0;
try {
  for (const run of runs) {
    if (only.length && !only.includes(run.name)) continue;
    for (const lang of langs) {
      const server = run.captive ? startCaptive() : await startMock(run.env || {});
      const ctx = await browser.newContext({ viewport, deviceScaleFactor: 2, locale: `${lang}-CH`, colorScheme: run.theme || "light" });
      await ctx.addInitScript(([l, t, open]) => {
        try {
          if (!sessionStorage.getItem("shots.init")) {
            localStorage.setItem("gplug.lang", l);
            localStorage.setItem("gplug.theme", t);
            localStorage.setItem("gplug.setupOpen", JSON.stringify(open));
            sessionStorage.setItem("shots.init", "1");
          }
        } catch { /* captive page on another origin setup, ignore */ }
      }, [lang, run.theme || "light", run.open || {}]);
      const page = await ctx.newPage();
      const dir = join(here, "img", lang);
      mkdirSync(dir, { recursive: true });
      try {
        await page.goto(`http://localhost:${server.port}/${run.start || ""}`);
        for (const step of expand(run.steps)) {
          await act(page, lang, step, dir);
          if (step.shot) count++;
        }
      } catch (e) {
        throw new Error(`run "${run.name}" (${lang}): ${e.message}`);
      } finally {
        await ctx.close();
        server.stop();
      }
    }
    console.log(`shots              ${run.name}`);
  }
} finally {
  await browser.close();
}
console.log(`shots              ${count} images in docs/img/{${langs.join(",")}}`);

// The top-level README shows a few English screens. Those copies are committed (GitHub renders the
// README from the repository, and img/ is not), so a shots run refreshes them with the rest.
const README_SHOTS = ["live", "history", "wizard-meter"];
if (langs.includes("en")) {
  mkdirSync(join(here, "readme"), { recursive: true });
  for (const n of README_SHOTS) {
    const from = join(here, "img", "en", `${n}.png`);
    if (existsSync(from)) copyFileSync(from, join(here, "readme", `${n}.png`));
  }
}
