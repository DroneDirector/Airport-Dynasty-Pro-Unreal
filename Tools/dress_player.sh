#!/bin/bash
# A garment from Blender onto the player, end to end (2026-11-13): into Unreal as a resizable outfit
# (Tools/make_outfit.py), the player built wearing it (Tools/make_metahumans.py outfit), and the wardrobe test's
# pictures (-UIWardrobe) of the player in it. Usage: Tools/dress_player.sh <Outfit> [fabrics]
R="$(cd "$(dirname "$0")/.." && pwd -W)"
ED="C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64"
LOG="$R/Saved/Logs/AirportDynastyPro.log"
O=$1
if [ "$2" = "fabrics" ]; then
  timeout 600 "$ED/UnrealEditor-Cmd.exe" "$R/AirportDynastyPro.uproject" -ExecutePythonScript="$R/Tools/set_fabrics.py" -unattended -nullrhi -nosplash > /dev/null 2>&1
  grep -c "ADP_FABRIC instance" "$LOG" | sed 's/^/fabrics: /'
fi
timeout 600 "$ED/UnrealEditor-Cmd.exe" "$R/AirportDynastyPro.uproject" -ExecutePythonScript="$R/Tools/make_outfit.py $O resize" -unattended -RenderOffscreen -nosplash > /dev/null 2>&1
grep -E "ADP_OUTFIT (DONE|failed)" "$LOG" | sed 's/^.*ADP_OUTFIT //'
timeout 1500 "$ED/UnrealEditor-Cmd.exe" "$R/AirportDynastyPro.uproject" -ExecutePythonScript="$R/Tools/make_metahumans.py outfit $O" -unattended -nosplash -RenderOffscreen -dx12 > /dev/null 2>&1
grep -E "ADP_MH: MH_900_player_$O (made|outfit failed)" "$LOG" | sed 's/^.*ADP_MH: //'
INI="$R/Saved/Config/WindowsEditor/GameUserSettings.ini"
sed -i "s/^Outfit=.*/Outfit=MH_900_player_$O/" "$INI"
rm -f "$R"/Saved/Screenshots/WindowsEditor/Wardrobe*.png
timeout 300 "$ED/UnrealEditor.exe" "$R/AirportDynastyPro.uproject" -game -windowed -ResX=1920 -ResY=1080 -ForceRes -RenderOffscreen -unattended -UIShot -UIWardrobe > /dev/null 2>&1
ls "$R"/Saved/Screenshots/WindowsEditor/ | grep -c Wardrobe | sed 's/^/pictures: /'
