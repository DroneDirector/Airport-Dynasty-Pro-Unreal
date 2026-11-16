#!/bin/bash
# Names the places in the performance log's STALL entries (the stall watch, PerfLog.cpp) for a packaged build, whose
# program carries no names: each "+0x..." offset looked up in the build's own symbol file (.pdb) with llvm-symbolizer.
#   bash Tools/stall_names.sh [build folder]   (default: the newest F:/ADP-Packaged/<version>/Full)
B="${1:-$(ls -d /f/ADP-Packaged/0.*/Full 2>/dev/null | sort -V | tail -1)}"
EXE="$B/AirportDynastyPro/Binaries/Win64/AirportDynastyPro-Win64-Shipping.exe"
LOG="$LOCALAPPDATA/AirportDynastyPro/Saved/Logs/ADP-Perf.log"
[ -f "$EXE" ] || { echo "no game program in $B"; exit 1; }
grep -A24 "STALL" "$LOG" | grep -E "STALL|\+0x" | while IFS= read -r L; do
  if [[ "$L" =~ \+0x([0-9a-f]+) ]]; then
    echo "    $(llvm-symbolizer --obj="$EXE" --relative-address "0x${BASH_REMATCH[1]}" 2>/dev/null | head -2 | tr '\n' ' ')"
  else echo "$L"; fi
done
