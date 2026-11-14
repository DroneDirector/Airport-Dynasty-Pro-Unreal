#!/bin/sh
# The land along the roads as Nanite meshes, end to end (owner, 2026-11-14). Run after Tools/make_roads.py (which
# remakes the band's data) whenever roads change: the game shapes the land under the roads and writes it (-DumpLand),
# Tools/land_nanite.py makes the glb, Tools/set_land_corridor.py imports it with its own ground material.
R="$(cd "$(dirname "$0")/.." && pwd)"; ED="/c/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64"
rm -rf "$R/Content/Land/Corridor"   # (the game shapes and writes the land only while no Nanite set is there)
timeout 900 "$ED/UnrealEditor.exe" "$R/AirportDynastyPro.uproject" -game -DumpLand -LocalTerrain -windowed -ResX=640 -ResY=360 -log=DumpLand.log > /dev/null 2>&1
grep "Land corridor" "$R/Saved/Logs/DumpLand.log" | cut -c30-
python "$R/Tools/land_nanite.py"
timeout 1800 "$ED/UnrealEditor-Cmd.exe" "$R/AirportDynastyPro.uproject" -ExecutePythonScript="$R/Tools/set_land_corridor.py" -unattended -nullrhi -nosplash > /dev/null 2>&1
grep "ADP_LAND\|LogPython: Error" "$R/Saved/Logs/AirportDynastyPro.log" | cut -c30- | tail -5
