#!/bin/sh
# Compile and run the host-side test suite (no ESP-IDF, no device).
#
#   ./run.sh                 all tests
#   ./run.sh dsmr history    only these (names without the test_ prefix)
#
# replay, raw, structure and capturelist read real meter frames from captures/, which is gitignored
# (it holds a device key). Without that directory they are reported as skipped rather than failed,
# so a fresh clone gets a green run on everything it can actually check.
cd "$(dirname "$0")" || exit 2
CXX=${CXX:-clang++}
NEEDS_CAPTURES="replay raw structure capturelist"
ALL="dsmr aes dlms replay raw structure capturelist framelog history csv eventlog sniff ha"
[ $# -gt 0 ] && set -- "$@" || set -- $ALL

fail=0
for t in "$@"; do
  case " $NEEDS_CAPTURES " in
    *" $t "*) [ -d captures ] || { echo "skip  $t (no captures/)"; continue; } ;;
  esac
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
