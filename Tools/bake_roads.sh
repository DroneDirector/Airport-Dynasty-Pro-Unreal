#!/bin/bash
# Bakes the roads (Tools/bake_roads.py) and copies the list where the game reads it. Run after any change to Data/roads.json.
R="$(cd "$(dirname "$0")/.." && pwd -W)"
cd "$R" || exit 1
tasklist | grep -qi "UnrealEditor.exe" && { echo "STOP: an Unreal editor or game is running"; exit 1; }
cmd //c "attrib -R Content\Roads\Baked\*.uasset" >/dev/null 2>&1
timeout 1800 "C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$R/AirportDynastyPro.uproject" -ExecutePythonScript="$R/Tools/bake_roads.py" -unattended -nosplash > /dev/null 2>&1
grep -E "ADP_BAKE|Roads bake|LogPython: Error" Saved/Logs/AirportDynastyPro.log | cut -c40-220
# the baked roads drawn by Nanite (same triangles, cheaper shadows)
timeout 3600 "C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$R/AirportDynastyPro.uproject" -ExecutePythonScript="$R/Tools/nanite_baked_roads.py" -unattended -nosplash > /dev/null 2>&1
grep -E "ADP_NANITE|LogPython: Error" Saved/Logs/AirportDynastyPro.log | cut -c40-220
cp Data/roads-baked.json Content/Data/roads-baked.json 2>/dev/null && echo "roads-baked.json copied to Content/Data"
