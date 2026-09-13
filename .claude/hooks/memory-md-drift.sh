#!/bin/sh
# PostToolUse hook (Bash): after a build of the real image, check that firmware/MEMORY.md still
# describes it. The file is the project's flash/RAM budget reference and went stale whenever a
# change was compiled but the numbers were not redone; tools/size_report.py makes redoing them a
# paste, and this makes forgetting it visible at the build that caused it.
input=$(cat)
cmd=$(printf '%s' "$input" | sed -n 's/.*"command": *"\([^"]*\)".*/\1/p' | head -1)

# Only builds of dev.yaml/gplug.yaml (base.yaml shares the build directory but is not the product).
case "$cmd" in
  *esphome*compile*dev.yaml*|*esphome*compile*gplug.yaml*|*esphome*run*dev.yaml*|*sizes.sh*) ;;
  *) exit 0 ;;
esac

out=$(python3 "${CLAUDE_PROJECT_DIR:-.}/firmware/tools/size_report.py" --check 2>&1) && exit 0
# Exit 2 hands the message to Claude; a missing build (exit 2 from the script) is not drift.
case "$out" in *"no build at"*) exit 0 ;; esac
printf '%s\n' "$out" >&2
exit 2
