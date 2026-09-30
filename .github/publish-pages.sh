#!/usr/bin/env bash
# Publishes to the gh-pages branch, which GitHub Pages serves (Settings -> Pages -> Deploy from a
# branch, gh-pages, / (root)). Two independent writers share the branch, so each touches only its
# own part of it:
#
#   publish-pages.sh install <dir>
#       release.yml, stable releases: the web installer at the site root (index.html, manifest.json,
#       the images). Devices fetch manifest.json and gplug-ota.bin from there for "Check for
#       updates", so these paths must never move. docs/ is left alone.
#   publish-pages.sh docs <site-dir> <edition> [latest]
#       docs.yml: one edition of the user manual into docs/<edition>/ (X.Y, one per minor release);
#       with `latest` also into docs/latest/. Rewrites docs/versions.json (the manual's version
#       picker) and docs/index.html. The installer files are left alone.
#
# One deploy used to replace the whole site (actions/deploy-pages); with the manual versioned per
# release that would mean rebuilding every old edition on every push. A branch keeps them.
# Runs in CI with GITHUB_TOKEN (contents: write); a push that loses a race is retried on top.
set -euo pipefail

mode="${1:?usage: publish-pages.sh install <dir> | docs <site-dir> <edition> [latest]}"
src="$(cd "${2:?source directory missing}" && pwd)"
edition="${3:-}"
latest="${4:-}"
[ "$mode" = docs ] && [ -z "$edition" ] && { echo "docs needs an edition" >&2; exit 2; }

work="$(mktemp -d)"
trap 'git worktree remove --force "$work" >/dev/null 2>&1 || true; rm -rf "$work"' EXIT
git config user.name "github-actions[bot]"
git config user.email "41898282+github-actions[bot]@users.noreply.github.com"

apply() {
  touch "$work/.nojekyll"   # serve files as they are, no Jekyll pass
  case "$mode" in
    install)
      # Copy over, never delete: the root also holds docs/.
      cp -R "$src"/. "$work"/
      msg="Publish installer ${GITHUB_REF_NAME:-local}"
      ;;
    docs)
      mkdir -p "$work/docs"
      rm -rf "$work/docs/$edition" && cp -R "$src" "$work/docs/$edition"
      if [ "$latest" = latest ]; then rm -rf "$work/docs/latest" && cp -R "$src" "$work/docs/latest"; fi
      node "$(dirname "$0")/../docs/versions.mjs" "$work/docs"
      msg="Publish manual ${edition}${latest:+ (latest)}"
      ;;
    *) echo "unknown mode $mode" >&2; exit 2 ;;
  esac
  git -C "$work" add -A
  git -C "$work" diff --cached --quiet && { echo "gh-pages: nothing changed"; return 0; }
  git -C "$work" commit -q -m "$msg"
  git -C "$work" push -q origin HEAD:gh-pages
}

for attempt in 1 2 3; do
  git worktree remove --force "$work" >/dev/null 2>&1 || true
  rm -rf "$work"
  if git fetch -q origin gh-pages 2>/dev/null; then
    git worktree add -q --detach "$work" FETCH_HEAD
  else
    # First publish: an empty branch without history of main.
    git worktree add -q --detach "$work"
    git -C "$work" checkout -q --orphan gh-pages-new
    git -C "$work" rm -rq --cached . >/dev/null 2>&1 || true
    find "$work" -mindepth 1 -maxdepth 1 ! -name .git -exec rm -rf {} +
  fi
  if apply; then echo "gh-pages: published ($mode${edition:+ $edition})"; exit 0; fi
  echo "gh-pages: push rejected, retrying ($attempt)" >&2
  sleep $((attempt * 5))
done
echo "gh-pages: giving up" >&2
exit 1
