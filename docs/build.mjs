// Builds the user manual from the user stories in stories/<lang>/*.md into site/ (static HTML, one
// page per chapter and language). `--strict` (CI) also fails on a missing screenshot.
//
// UI labels are never copied into a story: `{{S.key}}` is replaced by the SPA's own string from
// spa/src/i18n/<lang>.js and `{{C.key}}` by the captive portal's, so each language edition shows the
// labels the device shows and cannot drift when a label is reworded. An unknown key fails the build,
// like a missing translation fails the SPA build.
import { readFileSync, writeFileSync, mkdirSync, readdirSync, existsSync, rmSync, cpSync, statSync } from "node:fs";
import { join, dirname, relative } from "node:path";
import { fileURLToPath } from "node:url";
import { marked } from "marked";
import { de } from "../spa/src/i18n/de.js";
import { en } from "../spa/src/i18n/en.js";
import { fr } from "../spa/src/i18n/fr.js";
import { it } from "../spa/src/i18n/it.js";
import { checkLanguageTables } from "../spa/tools/i18n-check.mjs";
import { SITE, CHAPTERS } from "./site-strings.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const strict = process.argv.includes("--strict");
const LANGS = ["de", "en", "fr", "it"];   // de first: the reference, as in the SPA
const LANG_NAMES = { de: "Deutsch", en: "English", fr: "Français", it: "Italiano" };
const S = { de, en, fr, it };
const C = captiveTables();
const coverageJson = JSON.parse(readFileSync(join(here, "coverage.json"), "utf8"));
delete coverageJson._;
const COVERAGE = Object.keys(coverageJson);
const shotsJson = JSON.parse(readFileSync(join(here, "shots.json"), "utf8"));
const SHOTS = new Set([...shotsJson.runs.flatMap((r) => r.steps), ...Object.values(shotsJson.sequences).flat()]
  .filter((s) => s.shot).map((s) => s.shot));
const SECTIONS = ["Story", "Steps", "Acceptance", "Notes"];   // fixed English markers, localised on render
const REQUIRED_SECTIONS = SECTIONS.slice(0, 3);
const SHARED = ["chapter", "screen", "order", "mock", "hardware"];   // front matter every language must copy from de

const problems = [];
const missingShots = new Set();   // reported as one line: normal before the first `npm run shots`

// The captive portal is a framework-free page with its own flat table `var L = {de: {...}, ...}`;
// it is read from the page itself so the manual quotes what the phone shows.
function captiveTables() {
  const src = readFileSync(join(here, "..", "firmware", "components", "captive_portal", "captive.html"), "utf8");
  const m = src.match(/var L = (\{[\s\S]*?\n\});/);
  if (!m) throw new Error("captive.html: string table `var L = {...};` not found");
  return new Function(`return ${m[1]}`)();
}

// The UI parts a user can reach, read from the SPA's source: tabs (live/tabbar.js TABS), wizard
// steps (main.js STEPS) and collapsible cards (`Collapsible} id="..."`). Each must be listed under
// some coverage.json entry's `ui`, and that entry needs a story, so a card added to the app fails
// this build until the manual describes it. Parsed with regexes on purpose: the modules import
// Preact and the DOM, and the literals are simple.
function uiParts() {
  const src = join(here, "..", "spa", "src");
  const files = [];
  const walk = (d) => readdirSync(d).forEach((f) => {
    const p = join(d, f);
    if (statSync(p).isDirectory()) walk(p); else if (f.endsWith(".js") && !p.includes(`${join("src", "i18n")}`)) files.push(p);
  });
  walk(src);
  const parts = new Set();
  for (const f of files) {
    const text = readFileSync(f, "utf8");
    const uses = (text.match(/<\$\{Collapsible\}/g) || []).length;
    const ids = [...text.matchAll(/<\$\{Collapsible\}\s+id="([\w-]+)"/g)].map((m) => m[1]);
    if (uses !== ids.length) problems.push(`${relative(here, f)}: a Collapsible without a literal id="..." (coverage cannot see it)`);
    ids.forEach((id) => parts.add(`card:${id}`));
  }
  const tabs = readFileSync(join(src, "live", "tabbar.js"), "utf8").match(/TABS = \[([\s\S]*?)\n\];/);
  const steps = readFileSync(join(src, "main.js"), "utf8").match(/const STEPS = \[([^\]]*)\]/);
  if (!tabs || !steps) problems.push("coverage: TABS in live/tabbar.js or STEPS in main.js not found");
  else {
    [...tabs[1].matchAll(/^\s*\["([\w-]+)"/gm)].forEach((m) => parts.add(`tab:${m[1]}`));
    [...steps[1].matchAll(/"([\w-]+)"/g)].forEach((m) => parts.add(`step:${m[1]}`));
  }
  return parts;
}

function checkTables(what, tables) {
  const ordered = Object.fromEntries(LANGS.map((l) => [l, tables[l] ?? {}]));
  for (const p of checkLanguageTables(ordered)) problems.push(`${what}: ${p}`);
}

// Front matter is a flat `key: value` block; nothing in a story needs more than that.
function parseStory(file) {
  const text = readFileSync(file, "utf8");
  file = relative(here, file);
  const m = text.match(/^---\n([\s\S]*?)\n---\n([\s\S]*)$/);
  if (!m) { problems.push(`${file}: no front matter`); return null; }
  const meta = {};
  for (const line of m[1].split("\n")) {
    const kv = line.match(/^(\w+):\s*(.*)$/);
    if (kv) meta[kv[1]] = kv[2].replace(/^"(.*)"$/, "$1").trim();
    else if (line.trim()) problems.push(`${file}: cannot read front matter line "${line}"`);
  }
  const sections = {};
  let cur = null;
  for (const line of m[2].split("\n")) {
    const h = line.match(/^## (\w+)\s*$/);
    if (h) {
      if (!SECTIONS.includes(h[1])) problems.push(`${file}: unknown section "## ${h[1]}" (use ${SECTIONS.join(", ")})`);
      cur = h[1]; sections[cur] = [];
    } else if (cur) sections[cur].push(line);
    else if (line.trim()) problems.push(`${file}: text before the first section`);
  }
  for (const s of REQUIRED_SECTIONS) if (!sections[s]) problems.push(`${file}: missing section "## ${s}"`);
  for (const k of ["id", "title", "chapter", "screen", "order", "mock"]) if (!meta[k]) problems.push(`${file}: front matter lacks "${k}"`);
  if (meta.mock && !/^\d{4}-\d{2}-\d{2}$/.test(meta.mock)) problems.push(`${file}: mock must be a date (YYYY-MM-DD)`);
  if (meta.chapter && !CHAPTERS.some(([id]) => id === meta.chapter)) problems.push(`${file}: unknown chapter "${meta.chapter}"`);
  meta.screens = (meta.screen || "").split(",").map((s) => s.trim()).filter(Boolean);
  for (const s of meta.screens) if (!COVERAGE.includes(s)) problems.push(`${file}: unknown screen "${s}" (see coverage.json)`);
  meta.order = Number(meta.order);
  const body = Object.fromEntries(Object.entries(sections).map(([k, v]) => [k, v.join("\n").trim()]));
  return { file, meta, body, refs: new Set() };
}

// `{{S.key}}`, `{{S.key(1, "x")}}`, `{{C.key}}`, `{{img:name|caption}}`.
const REF = /\{\{\s*(?:(S|C)\.(\w+)(?:\((.*?)\))?|img:([\w-]+)\|([^}]*))\s*\}\}/g;
const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" })[c]);

// Placeholders survive Markdown untouched and are swapped for HTML afterwards, so a label that
// happens to contain `*` or `_` is shown as text instead of turning into emphasis.
function renderMarkdown(story, lang, md) {
  const out = [];
  const src = md.replace(REF, (all, table, key, args, img, caption) => {
    const where = `${story.file}: ${all}`;
    let html;
    if (img) {
      story.refs.add(`img:${img}`);
      if (!SHOTS.has(img)) problems.push(`${where}: no screenshot "${img}" in shots.json`);
      const png = join("img", lang, `${img}.png`);
      if (!existsSync(join(here, png))) {
        if (strict) problems.push(`${where}: ${png} missing (npm run shots)`);
        else missingShots.add(png);
      }
      html = `<figure><img src="img/${img}.png" alt="${esc(caption.trim())}" loading="lazy"><figcaption>${esc(caption.trim())}</figcaption></figure>`;
    } else {
      story.refs.add(`${table}.${key}`);
      const t = (table === "S" ? S : C)[lang];
      if (!(key in t)) { problems.push(`${where}: unknown key`); return all; }
      let v = t[key];
      if (typeof v === "function") {
        let a;
        try { a = args === undefined ? [] : JSON.parse(`[${args}]`); } catch { problems.push(`${where}: arguments are not JSON`); return all; }
        if (a.length !== v.length) { problems.push(`${where}: takes ${v.length} arguments, got ${a.length}`); return all; }
        v = v(...a);
      } else if (args !== undefined) { problems.push(`${where}: is text, not a function`); return all; }
      html = `<span class="ui">${esc(v)}</span>`;
    }
    out.push(html);
    return `\u0000${out.length - 1}\u0000`;
  });
  return marked.parse(src).replace(/\u0000(\d+)\u0000/g, (_, i) => out[Number(i)]);
}

// ---- checks -------------------------------------------------------------------------------------

checkTables("SPA strings", S);
checkTables("captive portal strings", C);
checkTables("manual strings", SITE);

const stories = {};
for (const lang of LANGS) {
  const dir = join(here, "stories", lang);
  stories[lang] = {};
  for (const f of existsSync(dir) ? readdirSync(dir).filter((f) => f.endsWith(".md") && !f.startsWith("_")).sort() : []) {
    const s = parseStory(join(dir, f));
    if (!s) continue;
    if (s.meta.id !== f.replace(/\.md$/, "")) problems.push(`${s.file}: id "${s.meta.id}" does not match the file name`);
    stories[lang][s.meta.id] = s;
  }
}

const ids = Object.keys(stories.de);
if (!ids.length) problems.push("no stories in stories/de");
for (const lang of LANGS.slice(1)) {
  for (const id of ids) {
    const s = stories[lang][id];
    if (!s) { problems.push(`${lang}: story "${id}" missing (stories/${lang}/${id}.md)`); continue; }
    for (const k of SHARED) {
      if ((s.meta[k] ?? "") + "" !== (stories.de[id].meta[k] ?? "") + "") problems.push(`${s.file}: ${k} "${s.meta[k] ?? ""}" differs from de "${stories.de[id].meta[k] ?? ""}"`);
    }
    for (const sec of SECTIONS) if (!!s.body[sec] !== !!stories.de[id].body[sec]) problems.push(`${s.file}: section "${sec}" present in only one of de and ${lang}`);
  }
  for (const id of Object.keys(stories[lang])) if (!stories.de[id]) problems.push(`${lang}: story "${id}" has no German original`);
}
{
  const parts = uiParts();
  const listed = new Set(Object.values(coverageJson).flatMap((e) => e.ui || []));
  for (const p of parts) if (!listed.has(p)) problems.push(`coverage: spa/src has ${p}, but no coverage.json entry lists it under "ui" (add one, and a story for it)`);
  for (const p of listed) if (!parts.has(p)) problems.push(`coverage: coverage.json lists ${p}, which spa/src no longer has`);
}
for (const screen of COVERAGE) {
  if (!ids.some((id) => stories.de[id].meta.screens.includes(screen))) problems.push(`coverage: no story covers screen "${screen}"`);
}

// ---- render -------------------------------------------------------------------------------------

const out = join(here, "site");
rmSync(out, { recursive: true, force: true });
mkdirSync(join(out, "assets"), { recursive: true });
cpSync(join(here, "assets"), join(out, "assets"), { recursive: true });

const pageName = (ch) => `${ch}.html`;

function page(lang, file, title, main, chapterId) {
  const T = SITE[lang];
  const nav = CHAPTERS.map(([id, key]) => {
    const list = chapterStories(lang, id).map((s) => `<li><a href="${pageName(id)}#${s.meta.id}">${esc(s.meta.title)}</a></li>`).join("");
    return `<li${id === chapterId ? ' class="cur"' : ""}><a href="${pageName(id)}">${esc(T[key])}</a><ol>${list}</ol></li>`;
  }).join("");
  const langs = LANGS.map((l) => l === lang
    ? `<b lang="${l}">${LANG_NAMES[l]}</b>`
    : `<a lang="${l}" hreflang="${l}" class="lang" href="../${l}/${file}">${LANG_NAMES[l]}</a>`).join("");
  return `<!doctype html>
<html lang="${lang}" data-page="${file}">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="color-scheme" content="light dark">
<title>${esc(title)} · gPlug</title>
<link rel="stylesheet" href="../assets/docs.css">
<script src="../assets/docs.js" defer></script>
</head>
<body>
<header>
  <a class="brand" href="index.html">gPlug <span>${esc(T.manual)}</span></a>
  <nav class="switch" aria-label="${esc(T.language)}">${langs}</nav>
  <label class="ver" hidden>${esc(T.version)} <select></select></label>
</header>
<div class="wrap">
<nav class="toc" aria-label="${esc(T.contents)}"><details><summary>${esc(T.contents)}</summary><ol>${nav}</ol></details></nav>
<main>${main}</main>
</div>
</body>
</html>
`;
}

function chapterStories(lang, ch) {
  return ids.filter((id) => stories.de[id].meta.chapter === ch)
    .sort((a, b) => stories.de[a].meta.order - stories.de[b].meta.order || a.localeCompare(b))
    .map((id) => stories[lang][id]).filter(Boolean);
}

function renderStory(s, lang) {
  const T = SITE[lang];
  const heading = { Story: T.story, Steps: T.steps, Acceptance: T.accept, Notes: T.notes };
  const secs = SECTIONS.filter((k) => s.body[k]).map((k) =>
    `<section class="${k.toLowerCase()}"><h3>${esc(heading[k])}</h3>${renderMarkdown(s, lang, s.body[k])}</section>`).join("");
  const date = new Date(`${s.meta.mock}T12:00:00Z`).toLocaleDateString(`${lang}-CH`, { day: "numeric", month: "long", year: "numeric" });
  const verified = T.verifiedMock(date) + (s.meta.hardware ? " " + T.verifiedHw(esc(s.meta.hardware)) : "");
  return `<article id="${s.meta.id}"><h2><a href="#${s.meta.id}">${esc(s.meta.title)}</a></h2>${secs}<p class="verified">${verified}</p></article>`;
}

for (const lang of LANGS) {
  const T = SITE[lang];
  const dir = join(out, lang);
  mkdirSync(dir, { recursive: true });
  const imgs = join(here, "img", lang);
  if (existsSync(imgs)) cpSync(imgs, join(dir, "img"), { recursive: true });

  const intro = join(here, "stories", lang, "_intro.md");
  if (!existsSync(intro)) problems.push(`${lang}: stories/${lang}/_intro.md missing`);
  const introHtml = existsSync(intro)
    ? renderMarkdown({ file: relative(here, intro), refs: new Set() }, lang, readFileSync(intro, "utf8")) : "";
  const index = CHAPTERS.map(([id, key]) =>
    `<h2><a href="${pageName(id)}">${esc(T[key])}</a></h2><ol>${chapterStories(lang, id).map((s) =>
      `<li><a href="${pageName(id)}#${s.meta.id}">${esc(s.meta.title)}</a></li>`).join("")}</ol>`).join("");
  writeFileSync(join(dir, "index.html"), page(lang, "index.html", T.manual, `${introHtml}${index}`, null));

  CHAPTERS.forEach(([id, key], i) => {
    const body = chapterStories(lang, id).map((s) => renderStory(s, lang)).join("");
    const prev = CHAPTERS[i - 1], next = CHAPTERS[i + 1];
    const pn = `<nav class="pn">${prev ? `<a href="${pageName(prev[0])}">← ${esc(T[prev[1]])}</a>` : "<span></span>"}${next ? `<a href="${pageName(next[0])}">${esc(T[next[1]])} →</a>` : ""}</nav>`;
    writeFileSync(join(dir, pageName(id)), page(lang, pageName(id), T[key], `<h1>${esc(T[key])}</h1>${body}${pn}`, id));
  });
}

// The edition's entry point sends the reader to their browser's language, German otherwise.
writeFileSync(join(out, "index.html"), `<!doctype html>
<html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>gPlug</title>
<script>
var l = "de", t = navigator.languages || [navigator.language || ""];
for (var i = 0; i < t.length; i++) { var c = String(t[i]).slice(0, 2).toLowerCase(); if (${JSON.stringify(LANGS)}.indexOf(c) >= 0) { l = c; break; } }
location.replace(l + "/index.html" + location.hash);
</script></head>
<body>${LANGS.map((l) => `<p><a href="${l}/index.html">${SITE[l].manual} – ${LANG_NAMES[l]}</a></p>`).join("")}</body></html>
`);

// A translation that drops a step usually drops the label it names; comparing the referenced keys
// and screenshots with the German story catches that without judging the prose.
for (const lang of LANGS.slice(1)) {
  for (const id of ids) {
    const s = stories[lang][id];
    if (!s) continue;
    const ref = stories.de[id].refs;
    const lost = [...ref].filter((r) => !s.refs.has(r)), extra = [...s.refs].filter((r) => !ref.has(r));
    if (lost.length) problems.push(`${s.file}: does not use ${lost.join(", ")} (the de story does)`);
    if (extra.length) problems.push(`${s.file}: uses ${extra.join(", ")} (the de story does not)`);
  }
}

if (missingShots.size) console.warn(`warning: ${missingShots.size} screenshots missing, e.g. ${[...missingShots][0]} (npm run shots)`);
if (problems.length) {
  console.error(`User manual has ${problems.length} problem(s):\n  ` + problems.join("\n  "));
  process.exit(1);
}
console.log(`manual             ${LANGS.join(", ")} · ${ids.length} stories each · ${COVERAGE.length} screens covered -> docs/site`);
