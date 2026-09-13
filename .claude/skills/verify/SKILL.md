---
name: verify
description: Full local check of the gPlug project before commit or release — SPA build with i18n check, stale-bundle check, host C++ tests, ESPHome config/compile and size report. Use after changes to spa/ or firmware/, or when asked to verify, test or check the build.
---

# Verify

Run in order, stop at the first failure and report it with output. Skip stages the change cannot
affect only when the user asked for a quick check; say which were skipped.

1. **SPA build** (always if `spa/` or `presets.json` changed):
   `cd spa && npm run build`. Note the printed gzip size; flag if > 64 kB.
2. **Bundle freshness** (what CI checks):
   ```
   git diff --quiet -- firmware/components/gplug_smi/spa.html.gz || echo "bundle changed: commit it"
   ```
   If `spa/src` changed but the bundle did not, the build did not pick the change up — investigate.
3. **Host tests**: `firmware/test/run.sh`. Report skipped capture tests explicitly; they cover the
   real-meter DLMS paths and only run where `firmware/test/captures/` exists.
4. **ESPHome config**: `cd firmware && esphome config dev.yaml`.
5. **Compile + size** (if `firmware/components/`, `*.yaml` or the bundle changed):
   `cd firmware && esphome compile dev.yaml`, then read flash/RAM from the output. Compare with the
   latest numbers in `firmware/MEMORY.md`; report the delta in kB and slot %.
6. **Mock smoke test** (if SPA behaviour changed): `cd spa && npm run dev` in background, fetch the
   touched `/api/*` routes and `/`, or use the `run` skill to drive the page. Stop the server after.

Summarise as a short table: stage, result, notable numbers. Never claim device behaviour from
mock or host results — say "not verified on hardware".
