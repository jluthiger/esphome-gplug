# gPlug ESPHome

ESPHome firmware for gPlug smart-meter adapters (ESP32-C3, 4 MB flash, ~400 kB SRAM, no PSRAM),
replacing Tasmota. Two halves that ship as one image:

- **Backend** `firmware/`: ESPHome external component `gplug_smi` (C++17, ESP-IDF). DSMR/P1,
  HDLC + DLMS/COSEM, AES-128-GCM, 15-min history on flash, HTTP API under `/api`. Plus a forked
  `captive_portal`. Configs: `gplug.yaml` (release, pulls components from GitHub), `dev.yaml`
  (same, local components) — always build `dev.yaml`.
- **Frontend** `spa/`: Preact 10 + htm (no JSX, no framework), esbuild → one gzipped HTML embedded in
  the firmware as `firmware/components/gplug_smi/spa.html.gz` (**committed**).

Read before non-trivial work: `intent/intent.md` (goal, constraints, decision log),
`firmware/README.md` (component, HTTP API, diag table), `spa/README.md` (screens, device API used),
`firmware/MEMORY.md` (flash/RAM budget). Phase: PoC.

## Commands

```
cd spa && npm run build          # bundle + i18n key check -> spa/dist + firmware/.../spa.html.gz
cd spa && npm run dev            # mock device on http://localhost:8080 (build first). MOCK_* env vars: see mock/server.mjs
cd spa && npm run presets        # regenerate presets.json from gplug/**/script.txt
firmware/test/run.sh [name...]   # host C++ tests (clang++, no IDF); capture-based tests skip without test/captures/
cd firmware && esphome config dev.yaml    # validate YAML
cd firmware && esphome compile dev.yaml   # full image (~2 min)
cd firmware && python3 tools/size_report.py [--check]   # flash/RAM tables for MEMORY.md; --check: is it current?
```

`/verify` runs the whole chain. Flashing a device (`esphome run/upload`) and tagging a release
are user-confirmed — never do either unprompted.

## Rules that bite

- **API contract has three copies**: firmware handler (`gplug_smi.cpp`), `spa/mock/server.mjs`,
  and the API tables in `firmware/README.md` + `spa/README.md`. Changing a route or JSON field means
  updating all of them in the same change. `/api-change` walks it.
- **Rebuild the SPA after any `spa/src` change** and commit `spa.html.gz`. CI fails the release on a
  stale bundle (compares decompressed HTML). A Stop hook here refuses to finish while it is stale.
- **Four languages** (`spa/src/i18n/{de,en,fr,it}.js`): every UI string exists in all four with the
  same type and arity; `de` is the reference. Text via `S.key` from `strings.js`, never hard-coded.
  Build fails on mismatch.
- **Mobile-first** UI (SPA and `captive.html`). Desktop rules only in `src/style.desktop.css`.
- **Budget**: SPA ≤ 64 kB gzipped; app image in a 1408 kB slot (see `firmware/MEMORY.md`). No
  unbounded heap on device: fixed-size buffers, max 48 OBIS entries. Report size deltas for
  firmware-visible changes.
- **`firmware/MEMORY.md` tracks the build.** Its tables come from `tools/size_report.py` and its
  `size-baseline` comment is what `--check` compares against. After a compile that moves the image
  > 1 kB or static RAM > 0.5 kB, update it in the same change (a hook flags this after every
  `esphome compile dev.yaml`).
- **Flash wear**: history appends every 15 min must last > 5 years; no new periodic flash/NVS writes
  without arithmetic.
- **Host-testable logic lives in header-only files** (`*_parser.h`, `*_decoder.h`, `*_store.h`,
  `*_log.h`, `protocol_sniff.h`, `ha_values.h`) with no ESPHome includes, tested in `firmware/test/test_*.cpp`.
  Keep new decode/storage logic there, add a test, and add it to `ALL` in `firmware/test/run.sh`.
  Editing one of those headers runs its tests automatically (PostToolUse hook).
- **Secrets**: `firmware/test/captures/` holds real frames and a device GUEK — gitignored, never
  commit, never print the key. The GUEK is never returned by the API.
- ESP-IDF HTTP shim: GET/POST only, JSON bodies read by the handler itself.
- Device facts in READMEs carry dates ("verified on gPlugK 2026-09-10"). Keep that style; say what
  was verified on hardware vs mock/host tests only.

## Style

- Comments explain *why* (constraint, measurement, failed alternative), in full sentences, like the
  existing code. Match that density.
- Commits: imperative sentence subject (no Conventional Commits prefix), body in prose explaining
  the reason and what was deliberately left out.
- Record design decisions with rationale in `intent/intent.md` decision log.
