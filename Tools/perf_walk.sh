#!/bin/bash
# The performance walk (owner, 2026-11-16: save the cycle so it can always be run again): the game walks the terminal
# by itself (-UIPerfWalk: Grand Hall, airside stairs, escalator, Floor 2, gate walkway, gate 1, its jetway, the service
# stair, the apron looking back at the terminal) and every slow frame goes to ADP-Perf.log with its cause and place.
#   bash Tools/perf_walk.sh            the newest packaged build in F:/ADP-Packaged (the real frames)
#   bash Tools/perf_walk.sh offscreen  refused: every run is visible on the owner's screen (2026-11-27)
#   bash Tools/perf_walk.sh editor     the editor build (only to check the route; its frames aren't the game's)
#   ... coaches                        the coaches' own walk instead (to-do 146: car park, zebra crossing, along the coaches,
#                                      into one and down its aisle, the main hall; by day - 13:00 site time; XTRA=-WalkNight adds the night parts, only for a real reason) - with any of the above
#   ... both                           the terminal walk then the coaches walk, in one go
#   ... 4k                             full screen at 3840x2160, visible
#   ... screen                         his own screen in real full screen (-fullscreen)
# (his own resolution and window settings are saved before and put back after every run: -ResX overwrites them)
R=/c/Users/Simon/Documents/Airport-Dynasty-Pro-Unreal
# one copy of the game only (owner, 2026-11-18, to-do 121): his editor (the builder) may stay open and is never closed;
# any other running game - the installed game, or the editor running the game (-game) - stops the walk until it ends
N=$(powershell -NoProfile -Command "@(Get-CimInstance Win32_Process | Where-Object { \$_.Name -like 'AirportDynastyPro-Win64*' -or (\$_.Name -like 'UnrealEditor*' -and \$_.CommandLine -match ' -game') }).Count")
[ "${N//[^0-9]/}" != "0" ] && { echo "STOP: another copy of the game is running ($N) - one copy at a time; the editor itself may stay open"; exit 1; }
# every run is visible, full screen, on the owner's screen (owner 2026-11-27): no -RenderOffscreen
OFF=""; EXE=""; ROUTE="-UIPerfWalk"
for a in "$@"; do
  case "$a" in
    offscreen) echo "STOP: hidden runs are not allowed - the owner watches every test on his screen (2026-11-27); if Windows is locked, wait"; exit 1 ;;
    4k) OFF="-fullscreen -ResX=3840 -ResY=2160 -ForceRes " ; EXE="r.SetRes 3840x2160f" ;;   # (visible on his screen, owner 2026-11-27)
    screen) OFF="-fullscreen" ;;
    coaches) ROUTE="-UICoachWalk" ;;
    both) ROUTE="-UIFullWalk" ;;
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
  # (PKG=<folder> runs a test package instead, e.g. PKG=/f/ADP-Packaged/test-coaches/Windows - built without a release)
  V=${PKG:-$(ls -d /f/ADP-Packaged/0.*/Full 2>/dev/null | sort -V | tail -1)}; [ -n "$V" ] || { echo "STOP: no packaged build"; exit 1; }
  echo "build: $V"
  L="$LOCALAPPDATA/AirportDynastyPro/Saved/Logs"
  (cd "$V" && ADP_LOG=1 ./AirportDynastyPro/Binaries/Win64/AirportDynastyPro-Win64-Shipping.exe -UIShot $ROUTE -StallWatch $OFF $XTRA ${EXE:+"-ExecCmds=$EXE"} > /dev/null 2>&1)
  STEPS=$L/ADP.log
fi
grep -q "Stress walk: STOP - the ground is missing" "$STEPS" && { echo "STOP: the ground is missing (no terrain installed) - fix the ground first, then walk again"; exit 2; }
echo "== the walk"; grep -E "Stress walk: " "$STEPS" | grep -v "way planned" | sed 's/.*Stress walk: //' | tr '\n' '|' | sed 's/|/ > /g'; echo
echo "== every 30 s"; grep "30 s:" "$L/ADP-Perf.log" | cut -c1-200
echo "== bad frames: $(grep -c 'BAD FRAME' "$L/ADP-Perf.log")"; grep "BAD FRAME" "$L/ADP-Perf.log" | sed 's/ - most likely / | /' | cut -c1-330
