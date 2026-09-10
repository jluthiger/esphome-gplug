#!/bin/sh
# Compile each config and print flash/RAM usage from the linker map summary.
cd "$(dirname "$0")"
for cfg in base gplug; do
  echo "=== $cfg"
  esphome compile "$cfg.yaml" 2>&1 | grep -E 'RAM:|Flash:|error|Error' | tail -5
  bin=".esphome/build/gplug/.pioenvs/gplug/firmware.bin"
  [ -f "$bin" ] && { cp "$bin" "$cfg.bin"; ls -l "$cfg.bin" | awk '{printf "%s bytes = %.0f kB\n",$5,$5/1024}'; }
done
