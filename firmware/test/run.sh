#!/bin/sh
# Compile and run the host-side test suite (no ESP-IDF, no device).
#
#   ./run.sh                 all tests
#   ./run.sh dsmr history    only these (names without the test_ prefix)
#
# replay, raw, structure and capturelist read real meter frames from captures/, which is gitignored
# (it holds a device key). A test whose own capture files are absent is reported as skipped rather
# than failed, so a fresh clone gets a green run on everything it can actually check -- and so does
# a machine that has some captures but not others, which is why each test names the file it needs
# instead of all four sharing a check for the directory.
cd "$(dirname "$0")" || exit 2
CXX=${CXX:-clang++}
needs_capture() {
  case $1 in
    replay) echo captures/gplugk_plaintext.hex ;;
    raw) echo captures/gplugk_raw1.hex ;;
    structure) echo captures/gplugm2_seg1.hex ;;
  esac
}
# capturelist is not listed here: it holds three independent captures and skips the ones whose files
# are missing by itself, so it still checks whatever this machine has.
ALL="dsmr aes dlms replay raw structure capturelist framelog history csv eventlog sniff ha heapmon version_cmp mqtt_template mqtt_status"
[ $# -gt 0 ] && set -- "$@" || set -- $ALL

fail=0
for t in "$@"; do
  skipped=
  for req in $(needs_capture "$t"); do
    [ -f "$req" ] || skipped="$req"
  done
  [ -z "$skipped" ] || { echo "skip  $t (no $skipped)"; continue; }
  if ! "$CXX" -std=c++17 -Wall -I../components/gplug_smi "test_$t.cpp" -o "test_$t" 2>"test_$t.err"; then
    echo "FAIL  $t (compile)"; cat "test_$t.err"; fail=1; rm -f "test_$t.err"; continue
  fi
  rm -f "test_$t.err"
  if out=$("./test_$t" 2>&1); then
    echo "ok    $t: $(printf '%s\n' "$out" | tail -1)"
  else
    echo "FAIL  $t"; printf '%s\n' "$out" | tail -20; fail=1
  fi
done
exit $fail
