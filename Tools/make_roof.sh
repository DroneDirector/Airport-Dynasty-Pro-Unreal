#!/bin/sh
# The owner's zinc roof on the terminal's and the car rental's roof tops, end to end (Tools/set_roof.py says how).
R="$(cd "$(dirname "$0")/.." && pwd)"; ED="/c/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64"
UE() { timeout 900 "$ED/UnrealEditor-Cmd.exe" "$R/AirportDynastyPro.uproject" -ExecutePythonScript="$R/Tools/set_roof.py $1" -unattended -nullrhi -nosplash > /dev/null 2>&1; grep "ADP_ROOF" "$R/Saved/Logs/AirportDynastyPro.log" | cut -c40-; }
UE dump
python "$R/Tools/roof_uvs_arc.py"
"/c/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python "$R/Tools/roof_unwrap.py" 2>&1 | grep ROOF
UE apply
