#!/bin/bash
# Fixed views measured (to-do 269): bash Tools/perf_views.sh <steps file> [editor|<packaged folder>] [extra flags]
# Full screen size, daytime; the step file says what (PerfViews.cpp). Summary + graphics memory: Saved/Logs/Views-Summary.txt
R="$(cd "$(dirname "$0")/.." && pwd)"; cd "$R"
F="$(cygpath -w "$(realpath "$1")")"; W="${2:-editor}"; shift; shift
N=$(powershell -NoProfile -Command "@(Get-CimInstance Win32_Process | Where-Object { \$_.Name -like 'AirportDynastyPro-Win64*' -or (\$_.Name -like 'UnrealEditor*') }).Count")
[ "${N//[^0-9]/}" != "0" ] && { echo "STOP: the game or editor is running"; exit 1; }
S="$(cygpath "$LOCALAPPDATA")/AirportDynastyPro/Saved"
for d in Saved/Config/WindowsEditor Saved/Config/Windows "$S/Config/Windows"; do [ -f "$d/GameUserSettings.ini" ] && cp "$d/GameUserSettings.ini" "$d/GameUserSettings.ini.bak-views"; done
( while true; do echo "$(date +%H:%M:%S) $(nvidia-smi --query-gpu=memory.used,utilization.gpu,clocks.gr --format=csv,noheader)"; sleep 1; done ) > Saved/Logs/Views-Vram.txt & SM=$!
if [ "$W" = editor ]; then
  # (his own game settings, so the run draws what he sees)
  [ -f "$S/Config/Windows/GameUserSettings.ini" ] && mkdir -p Saved/Config/WindowsEditor && cp "$S/Config/Windows/GameUserSettings.ini" Saved/Config/WindowsEditor/GameUserSettings.ini
  timeout 2400 "/c/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor.exe" "$(cygpath -w $PWD/AirportDynastyPro.uproject)" -game -fullscreen -ResX=3840 -ResY=1080 -ForceRes -Site=chablais -LocalTerrain -UIShot -UIChablais "-UIPerfViews=$F" "$@" -nosplash -log=Views.log > /dev/null 2>&1
  L=Saved/Logs/Views.log
else
  (cd "$W" && ADP_LOG=1 timeout 2400 ./AirportDynastyPro/Binaries/Win64/AirportDynastyPro-Win64-Shipping.exe -UIShot -UIChablais -fullscreen "-UIPerfViews=$F" "$@" > /dev/null 2>&1)
  L="$S/Logs/ADP.log"
fi
kill $SM 2>/dev/null
for d in Saved/Config/WindowsEditor Saved/Config/Windows "$S/Config/Windows"; do [ -f "$d/GameUserSettings.ini.bak-views" ] && mv -f "$d/GameUserSettings.ini.bak-views" "$d/GameUserSettings.ini"; done
grep "Views:" "$L" | sed 's/.*Views: //' > Saved/Logs/Views-Summary.txt
wc -l Saved/Logs/Views-Summary.txt; sort -k2 -n -t' ' Saved/Logs/Views-Vram.txt | tail -1; tail -2 Saved/Logs/Views-Summary.txt
