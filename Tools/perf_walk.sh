#!/bin/bash
# The performance walk (owner, 2026-11-16: save the cycle so it can always be run again): the game walks the terminal
# by itself (-UIPerfWalk: Grand Hall, airside stairs, escalator, Floor 2, gate walkway, gate 1, its jetway, the service
# stair, the apron looking back at the terminal) and every slow frame goes to ADP-Perf.log with its cause and place.
#   bash Tools/perf_walk.sh            the newest packaged build in F:/ADP-Packaged (the real frames)
#   bash Tools/perf_walk.sh offscreen  the packaged build drawn off-screen (when Windows is locked: it slows a window down)
#   bash Tools/perf_walk.sh editor     the editor build (only to check the route; its frames aren't the game's)
#   ... coaches                        the coaches' own walk instead (to-do 146: car park, zebra crossing, along the coaches,
#                                      into one and down its aisle, the main hall; day then night) - with any of the above
#   ... 4k                             drawn off-screen at 3840x2160 (Full HD: offscreen)
#   ... screen                         his own screen in real full screen (-fullscreen)
# (his own resolution and window settings are saved before and put back after every run: -ResX overwrites them)
R=/c/Users/Simon/Documents/Airport-Dynasty-Pro-Unreal
# one copy of the game only (owner, 2026-11-18, to-do 121): his editor (the builder) may stay open and is never closed;
# any other running game - the installed game, or the editor running the game (-game) - stops the walk until it ends
N=$(powershell -NoProfile -Command "@(Get-CimInstance Win32_Process | Where-Object { \$_.Name -like 'AirportDynastyPro-Win64*' -or (\$_.Name -like 'UnrealEditor*' -and \$_.CommandLine -match ' -game') }).Count")
[ "${N//[^0-9]/}" != "0" ] && { echo "STOP: another copy of the game is running ($N) - one copy at a time; the editor itself may stay open"; exit 1; }
# (offscreen: drawn without a window - for runs while Windows is locked, which holds a window's frames back)
OFF=""; ROUTE="-UIPerfWalk"
for a in "$@"; do
  case "$a" in
    offscreen) OFF="-RenderOffscreen -ResX=1920 -ResY=1080 -windowed" ;;
    4k) OFF="-RenderOffscreen -ResX=3840 -ResY=2160 -windowed" ;;
    screen) OFF="-fullscreen" ;;
    coaches) ROUTE="-UICoachWalk" ;;
  esac
done
GUS="$LOCALAPPDATA/AirportDynastyPro/Saved/Config/Windows/GameUserSettings.ini"
[ -f "$GUS" ] && cp "$GUS" "$TEMP/GUS-perfwalk.ini"
trap '[ -f "$TEMP/GUS-perfwalk.ini" ] && cp "$TEMP/GUS-perfwalk.ini" "$GUS"' EXIT
if [ "$1" = "editor" ]; then
  L=$R/Saved/Logs
  "C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor.exe" "$(cd $R && pwd -W)/AirportDynastyPro.uproject" -game -UIShot $ROUTE -StallWatch -windowed -ResX=1920 -ResY=1080 -nosplash > /dev/null 2>&1
  STEPS=$L/AirportDynastyPro.log
else
  V=$(ls -d /f/ADP-Packaged/0.*/Full 2>/dev/null | sort -V | tail -1); [ -n "$V" ] || { echo "STOP: no packaged build"; exit 1; }
  echo "build: $V"
  L="$LOCALAPPDATA/AirportDynastyPro/Saved/Logs"
  (cd "$V" && ADP_LOG=1 ./AirportDynastyPro/Binaries/Win64/AirportDynastyPro-Win64-Shipping.exe -UIShot $ROUTE -StallWatch $OFF > /dev/null 2>&1)
  STEPS=$L/ADP.log
fi
echo "== the walk"; grep -E "Stress walk: " "$STEPS" | grep -v "way planned" | sed 's/.*Stress walk: //' | tr '\n' '|' | sed 's/|/ > /g'; echo
echo "== every 30 s"; grep "30 s:" "$L/ADP-Perf.log" | cut -c1-200
echo "== bad frames: $(grep -c 'BAD FRAME' "$L/ADP-Perf.log")"; grep "BAD FRAME" "$L/ADP-Perf.log" | sed 's/ - most likely / | /' | cut -c1-330
