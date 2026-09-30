# METER user manual

The manual for METER, the firmware on gPlug adapters, generated from user stories: one story per task a user performs, in
German, English, French and Italian. Published on GitHub Pages under
<https://jluthiger.github.io/esphome-gplug/docs/>, one edition per major or minor release (`X.Y/`, and
`latest/`), built by `.github/workflows/docs.yml` on a `vX.Y.0` tag. Nothing builds it in CI before
that, so run `npm run build` locally whenever `spa/src`, `docs/` or `captive.html` change (`/verify`
does).

```
cd spa && npm run build        # the mock serves spa/dist, so build it first
cd docs && npm ci
npm run shots                  # screenshots from the mock, all languages -> img/ (~12 min; gitignored)
npm run shots -- device --lang=de   # only some runs / one language while writing
npm run build                  # stories -> site/; fails on any problem listed below
npm run build -- --strict      # as CI: a missing screenshot is an error too
npm run serve                  # http://localhost:8090/
```

## Layout

| Path | What |
|---|---|
| `stories/<lang>/<id>.md` | One story. `de` is the reference; every other language has the same files |
| `stories/<lang>/_intro.md` | Text of the manual's start page |
| `site-strings.mjs` | The manual's own frame text (headings, navigation, chapter names) per language |
| `coverage.json` | Every screen and card of the UI; each needs at least one story whose `screen` names it. Its `ui` lists tie entries to tabs, wizard steps and cards found in `spa/src` |
| `shots.json` | Screenshot runs: a mock scenario (`MOCK_*` env) and the taps that reach a screen |
| `build.mjs`, `shots.mjs`, `serve.mjs` | Build, screenshots, local preview |
| `readme/` | Three English screenshots for the top-level `README.md`, committed (GitHub renders the README from the repository); `npm run shots` refreshes them |
| `versions.mjs` | Edition index on the gh-pages branch (called by `.github/publish-pages.sh`) |

## A story

```markdown
---
id: daily-csv                 # = file name
title: Den Lastgang als CSV exportieren
chapter: daily                # setup | daily | device | trouble
screen: history-csv           # one or more ids from coverage.json, comma-separated
order: 30                     # position in the chapter
mock: 2026-09-30              # date the steps were walked on the mock
hardware: gPlugK 2026-09-25   # optional: what was verified on a real device, and when
---
## Story
Als … möchte ich …, damit …

## Steps
1. Tippen Sie auf {{S.tabHist}} und öffnen Sie die Karte {{S.csvTitle}}.
   {{img:history-csv|Lastgang exportieren}}

## Acceptance
- …

## Notes
- optional
```

The section markers stay English in every language (the build writes the localised headings).
`chapter`, `screen`, `order`, `mock` and `hardware` must be the same in every language.

**UI labels are never typed.** `{{S.key}}` is the SPA's string from `spa/src/i18n/<lang>.js`,
`{{S.key("arg", 2)}}` fills a function key (JSON arguments), `{{C.key}}` is the captive portal's
(`firmware/components/captive_portal/captive.html`). So each edition shows exactly what the device
shows, and a reworded label updates the manual. `{{img:name|caption}}` inserts a screenshot taken by
a `shots.json` run.

## What the build refuses

- a language table (SPA, captive portal, `site-strings.mjs`) that does not match German
- an unknown key, a function key with the wrong number of arguments, arguments on a text key
- a story missing in a language, one without a German original, a shared front-matter field that
  differs from German
- a translation that references other keys or screenshots than its German story (a dropped step
  usually drops its label)
- a screen in `coverage.json` without a story, an unknown screen or chapter
- a tab (`TABS`), wizard step (`STEPS`) or collapsible card (`Collapsible id="..."`) in `spa/src` that
  no `coverage.json` entry lists under `ui`, or a `ui` entry the code no longer has
- an image name no `shots.json` run produces; with `--strict`, a missing image

## Keeping it current

Label text follows the SPA by itself, and screenshots are retaken for every edition. What no check
can see is a flow that changed while its labels stayed. That is reviewed once per minor release,
against the *Added* and *Changed* lines of the changelog (`/release`, preflight).

Renaming or removing an SPA string therefore also means `cd docs && npm run build`. CI builds the
manual only for a major or minor release tag, so a break found there blocks that release's edition.
