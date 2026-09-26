// Bundles src/ into dist/index.html (single file, JS+CSS inlined) and dist/index.html.gz
// for the mock dev server, plus the copy embedded by the ESPHome firmware.
import { build } from "esbuild";
import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { gzipSync } from "node:zlib";
import { de } from "./src/i18n/de.js";
import { en } from "./src/i18n/en.js";
import { fr } from "./src/i18n/fr.js";
import { it } from "./src/i18n/it.js";
import { iconPng } from "./tools/icon.mjs";
import { WIDE } from "./src/layout.js";
import { compile, render } from "./src/live/mqtt-template.js";

const watch = process.argv.includes("--watch");

// A translation that silently lacks a key would ship a German word inside a French UI (the proxy
// in i18n/index.js falls back to German rather than showing "undefined"), and a key whose value is
// a function in one language but a string in another throws at render time. Both are cheap to
// catch here and expensive to notice on a device, so the build refuses them.
function checkLanguageTables(tables) {
  const [refName, ref] = Object.entries(tables)[0];
  const refKeys = Object.keys(ref);
  const problems = [];
  for (const [name, t] of Object.entries(tables).slice(1)) {
    for (const k of refKeys) {
      if (!(k in t)) problems.push(`${name}: missing key "${k}"`);
      else if (typeof t[k] !== typeof ref[k]) problems.push(`${name}: "${k}" is ${typeof t[k]}, ${refName} has ${typeof ref[k]}`);
      else if (typeof t[k] === "function" && t[k].length !== ref[k].length) problems.push(`${name}: "${k}" takes ${t[k].length} arguments, ${refName} passes ${ref[k].length}`);
    }
    for (const k of Object.keys(t)) if (!(k in ref)) problems.push(`${name}: unknown key "${k}" (not in ${refName})`);
  }
  if (problems.length) {
    console.error("Language tables do not match:\n  " + problems.join("\n  "));
    process.exit(1);
  }
  console.log(`languages          ${Object.keys(tables).join(", ")} · ${refKeys.length} strings each`);
}
checkLanguageTables({ de, en, fr, it });

// The MQTT card previews templates with a JS port of the firmware's renderer. A preview that
// disagrees with what the device publishes misleads more than none, so the port has to pass the
// same vectors as test/test_mqtt_template.cpp; the fixture is the one described in the file.
function checkMqttVectors() {
  const f = {
    names: ["Ei", "Eo", "Pi", "Po", "SMid", "U1"], obis: ["1.8.0", "2.8.0", "1.7.0", "2.7.0", "96.1.0", "32.7.0"],
    units: ["kWh", "kWh", "kW", "kW", "", "V"], prec: [3, 3, 3, 3, 0, 1],
    string: [false, false, false, false, true, false], device: "gplug-a1b2c3", mac: "a1b2c3d4e5f6",
  };
  const s = { v: [1234.5, 0.25, 1.5, 0, 0, 0], have: [true, true, true, true, false, false], smid: '1234"5\\6', epoch: 1790000000 };
  const unesc = (x) => x.replace(/\\([tn])/g, (_, c) => (c === "t" ? "\t" : "\n"));
  const problems = [];
  let rows = 0;
  readFileSync("../firmware/test/mqtt_template_vectors.tsv", "utf8").split("\n").forEach((line, n) => {
    if (!line || line.startsWith("#")) return;
    const col = line.split("\t");
    if (col.length !== 4) { problems.push(`line ${n + 1}: ${col.length} columns`); return; }
    const each = col[0] === "each", topic = col[1] === "topic";
    const c = compile(unesc(col[2]), each, topic, f);
    const got = c.ok ? "ok:" + render(c, topic, f, s, each ? 0 : -1) : `err:${c.code}:${c.pos}`;
    if (got !== unesc(col[3])) problems.push(`line ${n + 1}: ${col[2]}\n    want ${col[3]}\n    got  ${got}`);
    rows++;
  });
  if (problems.length) {
    console.error("MQTT template port disagrees with the firmware vectors:\n  " + problems.join("\n  "));
    process.exit(1);
  }
  console.log(`mqtt vectors       ${rows} rows match the firmware`);
}
checkMqttVectors();

const result = await build({
  entryPoints: ["src/main.js"],
  bundle: true,
  minify: true,
  format: "iife",
  target: ["es2018"],
  write: false,
  define: { "process.env.NODE_ENV": '"production"' },
  legalComments: "none",
});
const js = result.outputFiles[0].text;
const squish = (f) => readFileSync(f, "utf8").replace(/\s+/g, " ").trim();
const css = squish("src/style.css");
// Kept in its own file rather than as @media blocks at the bottom of style.css: the mobile sheet
// stays the base cascade and the desktop rules stay reviewable (and removable) as one layer.
// index.html scopes them with <style media>, so they still travel in the same single document.
const cssDesktop = squish("src/style.desktop.css");
// The wide layer's query is the same constant useWide() matches, so the CSS and the components
// that branch on it switch at the same pixel.
const cssWide = squish("src/style.wide.css");
const html = readFileSync("src/index.html", "utf8")
  .replace("/*CSS*/", () => css)
  .replace("/*CSS-DESKTOP*/", () => cssDesktop)
  .replace("/*WIDE-Q*/", () => WIDE)
  .replace("/*CSS-WIDE*/", () => cssWide)
  .replace("/*JS*/", () => js.replace(/<\/script/gi, "<\\/script"));

mkdirSync("dist", { recursive: true });
writeFileSync("dist/index.html", html);
const gz = gzipSync(Buffer.from(html), { level: 9 });
writeFileSync("dist/index.html.gz", gz);
// The copy the firmware actually embeds (committed; see firmware/components/gplug_smi/__init__.py).
const BUNDLED = "../firmware/components/gplug_smi/spa.html.gz";
writeFileSync(BUNDLED, gz);
// Home-screen shortcut assets, served by the firmware as /manifest.webmanifest and /icon.png.
const manifest = Buffer.from(JSON.stringify(JSON.parse(readFileSync("src/manifest.webmanifest", "utf8"))));
const icon = iconPng(512);
writeFileSync("dist/manifest.webmanifest", manifest);
writeFileSync("dist/icon.png", icon);
writeFileSync("../firmware/components/gplug_smi/manifest.webmanifest", manifest);
writeFileSync("../firmware/components/gplug_smi/icon.png", icon);
console.log(`icon.png            ${(icon.length / 1024).toFixed(1)} kB, manifest ${manifest.length} B`);
console.log(`css                ${(css.length / 1024).toFixed(1)} kB mobile + ${(cssDesktop.length / 1024).toFixed(1)} kB desktop + ${(cssWide.length / 1024).toFixed(1)} kB wide`);
console.log(`dist/index.html     ${(html.length / 1024).toFixed(1)} kB`);
console.log(`dist/index.html.gz  ${(gz.length / 1024).toFixed(1)} kB  -> ${BUNDLED}`);
