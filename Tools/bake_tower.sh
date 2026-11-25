#!/bin/bash
# Bakes the control tower's shells (Tools/bake_tower.py), then checks in a second run that they load. Run after any change
# to the shells in TowerADC.cpp. Takes the lock; no window.   bash Tools/bake_tower.sh [check]   (check: only the check)
R="$(cd "$(dirname "$0")/.." && pwd -W)"; cd "$R" || exit 1
L=/f/ADP-Export/gpu.lock
mkdir $L 2>/dev/null || { echo "LOCK BUSY - not baked"; exit 3; }
trap 'rmdir $L 2>/dev/null' EXIT
tasklist | grep -qi "UnrealEditor" && { echo "STOP: an Unreal editor or game is running"; exit 3; }
chmod u+w Content/Tower/Baked/*.uasset 2>/dev/null
run() {
  LOG="$R/Saved/Logs/bake_tower_$1.log"
  timeout 200 "C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$R/AirportDynastyPro.uproject" -ExecutePythonScript="$R/Tools/bake_tower.py" -unattended -nullrhi -nosplash -nosound -abslog="$LOG" > /dev/null 2>&1
  echo "-- $1 (exit $?)"
  grep -aE "ADP_BAKE|Tower bake|Tower:|LogPython: Error|SM_ADC|Fatal|Assertion" "$LOG" | sed 's/^\[[^]]*\]\[[^]]*\]//' | cut -c1-330 | head -70
}
[ "$1" = check ] || run bake
export ADP_TOWER_CHECK=1
run check
ls -l --time-style=full-iso Content/Tower/Baked Data/tower-baked.json | cut -c1-120
