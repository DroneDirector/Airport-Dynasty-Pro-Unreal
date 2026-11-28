#!/bin/bash
# Fixed views measured (to-do 269): bash Tools/perf_views.sh <steps file> [editor|<packaged folder>] [extra flags]
# Full screen size, daytime; the step file says what (PerfViews.cpp). Summary + graphics memory: Saved/Logs/Views-Summary.txt
#   --from <section tag or name word>: starts at that "# ===== <tag> <name>" section (the skipped ones' setup still run)
#   --resume: starts after the last section finished (Saved/Logs/Views-Progress.txt, written as each one ends)
# Always with -StallWatch: every game-thread hold of 60 ms or more named in ADP-Perf.log, short in the summary's end.
R="$(cd "$(dirname "$0")/.." && pwd)"; cd "$R"
F="$(cygpath -w "$(realpath "$1")")"; W="${2:-editor}"; shift; shift
X=(); while [ $# -gt 0 ]; do case "$1" in
  --from) X+=("-UIPerfViewsFrom=${2// /_}"); shift 2 ;;
  --resume) X+=(-UIPerfViewsResume); shift ;;
  *) X+=("$1"); shift ;;
esac; done
set -- "${X[@]}" -StallWatch
N=$(powershell -NoProfile -Command "@(Get-CimInstance Win32_Process | Where-Object { \$_.Name -like 'AirportDynastyPro-Win64*' -or (\$_.Name -like 'UnrealEditor*') }).Count")
[ "${N//[^0-9]/}" != "0" ] && { echo "STOP: the game or editor is running"; exit 1; }
S="$(cygpath "$LOCALAPPDATA")/AirportDynastyPro/Saved"
# (a fresh start forgets the old save point, so a later --resume never goes back to an older run)
case " $* " in *" -UIPerfViewsResume "*) ;; *) if [ "$W" = editor ]; then rm -f Saved/Logs/Views-Progress.txt; else rm -f "$S/Logs/Views-Progress.txt"; fi ;; esac
for d in Saved/Config/WindowsEditor Saved/Config/Windows "$S/Config/Windows"; do [ -f "$d/GameUserSettings.ini" ] && cp "$d/GameUserSettings.ini" "$d/GameUserSettings.ini.bak-views"; done
( while true; do echo "$(date +%H:%M:%S) $(nvidia-smi --query-gpu=memory.used,utilization.gpu,clocks.gr --format=csv,noheader)"; sleep 1; done ) > Saved/Logs/Views-Vram.txt & SM=$!
if [ "$W" = editor ]; then
  # (his own game settings, so the run draws what he sees)
  [ -f "$S/Config/Windows/GameUserSettings.ini" ] && mkdir -p Saved/Config/WindowsEditor && cp "$S/Config/Windows/GameUserSettings.ini" Saved/Config/WindowsEditor/GameUserSettings.ini
  timeout 3600 "/c/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor.exe" "$(cygpath -w $PWD/AirportDynastyPro.uproject)" -game -fullscreen -ResX=3840 -ResY=1080 -ForceRes -Site=chablais -LocalTerrain -UIShot -UIChablais "-UIPerfViews=$F" "$@" -nosplash -log=Views.log > /dev/null 2>&1
  L=Saved/Logs/Views.log; P=Saved/Logs/ADP-Perf.log; G=Saved/Logs/Views-Progress.txt
else
  (cd "$W" && ADP_LOG=1 timeout 3600 ./AirportDynastyPro/Binaries/Win64/AirportDynastyPro-Win64-Shipping.exe -UIShot -UIChablais -fullscreen "-UIPerfViews=$F" "$@" > /dev/null 2>&1)
  L="$S/Logs/ADP.log"; P="$S/Logs/ADP-Perf.log"; G="$S/Logs/Views-Progress.txt"
fi
kill $SM 2>/dev/null
for d in Saved/Config/WindowsEditor Saved/Config/Windows "$S/Config/Windows"; do [ -f "$d/GameUserSettings.ini.bak-views" ] && mv -f "$d/GameUserSettings.ini.bak-views" "$d/GameUserSettings.ini"; done
grep "Views:" "$L" | sed 's/.*Views: //' > Saved/Logs/Views-Summary.txt
{ echo "---- save point: $(head -2 "$G" 2>/dev/null | tr '\n' ' ')"
  echo "---- stalls (the game thread held 60 ms or more; first and longest look of each, the step it was in)"
  [ -f "$P" ] && awk -f Tools/views_stalls.awk "$P" | tail -300; } >> Saved/Logs/Views-Summary.txt
wc -l Saved/Logs/Views-Summary.txt; sort -k2 -n -t' ' Saved/Logs/Views-Vram.txt | tail -1; grep -c STALL Saved/Logs/Views-Summary.txt; tail -1 Saved/Logs/Views-Summary.txt
