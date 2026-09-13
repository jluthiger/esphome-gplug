#!/bin/sh
# Stop hook: refuse to finish while SPA sources changed after the committed bundle was built.
# The firmware embeds firmware/components/gplug_smi/spa.html.gz, and the release workflow fails
# when that file does not match `npm run build` -- so a forgotten rebuild ships old UI or breaks CI.
input=$(cat)
# Already continuing because of this hook once: don't loop.
printf '%s' "$input" | grep -q '"stop_hook_active": *true' && exit 0

cd "${CLAUDE_PROJECT_DIR:-.}" || exit 0
BUNDLE=firmware/components/gplug_smi/spa.html.gz
[ -f "$BUNDLE" ] || exit 0

# Only files that are modified or new in the working tree count; checkout mtimes mean nothing.
changed=$(git status --porcelain -- spa/src spa/build.mjs spa/tools firmware/components/gplug_smi/presets.json 2>/dev/null \
  | awk '{print $NF}')
[ -n "$changed" ] || exit 0

stale=""
for f in $changed; do
  [ -f "$f" ] && [ "$f" -nt "$BUNDLE" ] && stale="$stale $f"
done
[ -n "$stale" ] || exit 0

echo "SPA sources changed after $BUNDLE was built:$stale" >&2
echo "Run 'cd spa && npm run build' (also checks the four language tables) before finishing." >&2
exit 2
