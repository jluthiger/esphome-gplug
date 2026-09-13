#!/bin/sh
# PostToolUse hook (Edit|Write): after a change to a host-testable decoder/store header or a test,
# run the matching host test so a broken decoder surfaces at the edit, not at the next compile.
# Headers without a direct test, and gplug_smi.cpp (needs ESP-IDF), are left to /verify.
f=$(cat | sed -n 's/.*"file_path": *"\([^"]*\)".*/\1/p' | head -1)
[ -n "$f" ] || exit 0

case "$(basename "$f")" in
  dsmr_parser.h|test_dsmr.cpp)            tests="dsmr" ;;
  test_aes.cpp)                           tests="aes" ;;
  aes_gcm.h)                              tests="aes dlms raw" ;;
  dlms_decoder.h)                         tests="dlms replay raw structure capturelist framelog sniff" ;;
  test_dlms.cpp|test_replay.cpp|test_raw.cpp|test_structure.cpp|test_capturelist.cpp)
                                          t=$(basename "$f" .cpp); tests=${t#test_} ;;
  frame_log.h|test_framelog.cpp)          tests="framelog" ;;
  history_store.h|test_history.cpp)       tests="history csv" ;;
  history_csv.h|test_csv.cpp)             tests="csv" ;;
  event_log.h|test_eventlog.cpp)          tests="eventlog" ;;
  protocol_sniff.h|test_sniff.cpp)        tests="sniff" ;;
  *) exit 0 ;;
esac

out=$("${CLAUDE_PROJECT_DIR:-.}/firmware/test/run.sh" $tests 2>&1) && exit 0
echo "Host tests failed after editing $(basename "$f"):" >&2
printf '%s\n' "$out" | tail -40 >&2
exit 2
