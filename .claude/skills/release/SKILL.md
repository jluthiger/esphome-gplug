---
name: release
description: Cut a gPlug firmware release or release candidate - release commit, checks, tag, hardware test, back to -dev. Only when the user explicitly asks to release, tag or publish a version.
disable-model-invocation: true
---

# Release

The process is in `README.md` → "Versions and branches" and "Cutting a release"; this is the
checklist. Every tag push and every push to `main` below is outward-facing: **state exactly what
will be pushed and wait for the user's yes** before each one.

Ask for the version if not given. Minor vs patch: see the table in the README (`CHANGELOG.md`
*Unreleased* shows what changed; any settings/API/entity-key break or partition change → minor).

## 1. Preflight
- On `main`, clean tree, up to date with `origin/main`.
- `/verify` passes: SPA build committed, `firmware/test/run.sh`, `esphome compile dev.yaml`,
  `python3 firmware/tools/size_report.py --check`.
- `CHANGELOG.md` *Unreleased* describes everything user-visible since the last tag
  (`git log --oneline <last tag>..HEAD`); fill gaps, write *Upgrade notes* (USB reflash? settings and
  history kept? HA entity IDs changed?).

## 2. Release candidate `X.Y.Z-rc.N`
1. `firmware/gplug.yaml`: `version: "X.Y.Z-rc.N"`, external_components `ref: vX.Y.Z-rc.N`.
2. `CHANGELOG.md`: *Unreleased* → `## [X.Y.Z] – <today>`, new empty `## [Unreleased]` above.
3. `.github/release-check.sh X.Y.Z-rc.N` passes.
4. Commit "Release X.Y.Z-rc.N". Confirm, then `git tag vX.Y.Z-rc.N && git push origin main vX.Y.Z-rc.N`.
5. Watch the workflow (`gh run watch`), check the pre-release has both images and the notes.

## 3. Hardware test (with the user)
- OTA the rc's `gplug-X.Y.Z-rc.N.ota.bin` onto a gPlug that runs the **previous release** (flash that
  release first if needed): Wi-Fi, meter settings, `history.count`, event log survive; `diag` `ok`.
- Home Assistant entities present (API client check as in the HA verification); app on phone + desktop.
- Web installer only if a spare unit exists.
- Record results with date and variant; untested variants listed as not verified on hardware. Add them
  to the GitHub pre-release notes (`gh release edit`).
- A failure: fix on `main`, next `rc.N+1` (restore `-dev`/`ref: main` first only if the fix is not
  immediate).

## 4. Release `X.Y.Z`
1. New commit on top of the tested rc: `version: "X.Y.Z"`, `ref: vX.Y.Z`, changelog date if it changed.
   Nothing else in this commit.
2. `.github/release-check.sh X.Y.Z` passes.
3. Confirm, then `git tag vX.Y.Z && git push origin main vX.Y.Z`.
4. Workflow: release published as latest, installer page deployed, `stable` moved
   (`git ls-remote origin stable` equals the tag's commit).

## 5. Back to development
- `version: "<next minor>-dev"`, `ref: main`. Commit "Start X.Y+1.0 development", confirm, push.

Never move `stable` or delete tags by hand unless the user asks; a bad release is fixed by a new patch.
