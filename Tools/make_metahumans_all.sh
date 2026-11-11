#!/bin/bash
# Builds the MetaHuman people one at a time, each in its own hidden editor session (Epic's MetaHuman build doesn't
# take a second person in the same session: the shared parts it saved the first time trip it up). Already built
# people are kept. Usage: Tools/make_metahumans_all.sh [first] [last]   (default 0 39)
R="$(cd "$(dirname "$0")/.." && pwd -W)"
ED="C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
FIRST=${1:-0}; LAST=${2:-39}
for i in $(seq $FIRST $LAST); do
  "$ED" "$R/AirportDynastyPro.uproject" -ExecutePythonScript="$R/Tools/make_metahumans.py build 1 $i" -unattended -nosplash -RenderOffscreen -dx12 > /dev/null 2>&1
  grep -E "ADP_MH: MH_|LogPython: Error" "$R/Saved/Logs/AirportDynastyPro.log" | tail -1 | sed 's/^.*ADP_MH: //'
done
echo "PEOPLE DONE"
