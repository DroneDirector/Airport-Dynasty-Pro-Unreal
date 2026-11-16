#!/bin/bash
# The performance walk (owner, 2026-11-16: save the cycle so it can always be run again): the game walks the terminal
# by itself (-UIPerfWalk: Grand Hall, airside stairs, escalator, Floor 2, gate walkway, gate 1, its jetway, the service
# stair, the apron looking back at the terminal) and every slow frame goes to ADP-Perf.log with its cause and place.
#   bash Tools/perf_walk.sh            the newest packaged build in F:/ADP-Packaged (the real frames)
#   bash Tools/perf_walk.sh offscreen  the packaged build drawn off-screen (when Windows is locked: it slows a window down)
#   bash Tools/perf_walk.sh editor     the editor build (only to check the route; its frames aren't the game's)
R=/c/Users/Simon/Documents/Airport-Dynasty-Pro-Unreal
tasklist | grep -qiE "AirportDynastyPro-Win64|UnrealEditor.exe" && { echo "STOP: a game or editor is running (one copy at a time)"; exit 1; }
# (offscreen: drawn without a window - for runs while Windows is locked, which holds a window's frames back)
OFF=""; [ "$1" = "offscreen" -o "$2" = "offscreen" ] && OFF="-RenderOffscreen -ResX=1920 -ResY=1080 -windowed"
if [ "$1" = "editor" ]; then
  L=$R/Saved/Logs
  timeout 600 "C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor.exe" "$(cd $R && pwd -W)/AirportDynastyPro.uproject" -game -UIShot -UIPerfWalk -StallWatch -windowed -ResX=1920 -ResY=1080 -nosplash > /dev/null 2>&1
  STEPS=$L/AirportDynastyPro.log
else
  V=$(ls -d /f/ADP-Packaged/0.*/Full 2>/dev/null | sort -V | tail -1); [ -n "$V" ] || { echo "STOP: no packaged build"; exit 1; }
  echo "build: $V"
  L="$LOCALAPPDATA/AirportDynastyPro/Saved/Logs"
  (cd "$V" && ADP_LOG=1 timeout 600 ./AirportDynastyPro/Binaries/Win64/AirportDynastyPro-Win64-Shipping.exe -UIShot -UIPerfWalk -StallWatch $OFF > /dev/null 2>&1)
  STEPS=$L/ADP.log
fi
echo "== the walk"; grep -E "UIPerfWalk" "$STEPS" | sed 's/.*UIPerfWalk: //' | tr '\n' '|' | sed 's/|/ > /g'; echo
echo "== every 30 s"; grep "30 s:" "$L/ADP-Perf.log" | cut -c1-200
echo "== bad frames: $(grep -c 'BAD FRAME' "$L/ADP-Perf.log")"; grep "BAD FRAME" "$L/ADP-Perf.log" | sed 's/ - most likely / | /' | cut -c1-330
