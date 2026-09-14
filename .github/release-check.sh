#!/bin/sh
# Checks that the tree is a release of <version>, the way the release workflow requires before it
# builds anything. Run it locally before tagging:
#
#   .github/release-check.sh 0.3.0          # release
#   .github/release-check.sh 0.3.0-rc.1     # pre-release
#
# A release is one commit that says what it is, in three places that must agree with the tag:
#   * firmware/gplug.yaml `version:`          -- what the app and the Device Builder show
#   * firmware/gplug.yaml external_components `ref: v<version>`
#                                              -- the component pinned to this very tag, so an adopted
#                                                 config following `stable` builds the gplug.yaml and
#                                                 the component of one commit, never a mix
#   * CHANGELOG.md `## [<base version>]`       -- the release notes; a pre-release uses the section of
#                                                 the version it leads up to (0.3.0-rc.1 -> [0.3.0])
set -eu
cd "$(dirname "$0")/.."

v=${1:?usage: release-check.sh <version>}
v=${v#v}
base=${v%%-*}
yaml=firmware/gplug.yaml
fail=0

case "$v" in
  *-dev*) echo "error: $v is a development version, not a release" >&2; exit 1 ;;
esac
echo "$v" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+(-rc\.[0-9]+)?$' \
  || { echo "error: $v is not X.Y.Z or X.Y.Z-rc.N" >&2; exit 1; }

have=$(sed -n 's/^  version: "\(.*\)".*/\1/p' "$yaml" | head -1)
[ "$have" = "$v" ] || { echo "error: $yaml has version \"$have\", tag says \"$v\"" >&2; fail=1; }

ref=$(awk '/^external_components:/{f=1} f&&/ ref: /{print $2; exit}' "$yaml")
[ "$ref" = "v$v" ] || { echo "error: $yaml external_components ref is \"$ref\", must be \"v$v\"" >&2; fail=1; }

grep -q "^## \[$base\]" CHANGELOG.md || { echo "error: CHANGELOG.md has no \"## [$base]\" section" >&2; fail=1; }

[ $fail -eq 0 ] && echo "release check ok: $v"
exit $fail
