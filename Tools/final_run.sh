#!/bin/bash
# The one performance and picture run (Tools/views/final.txt) in a packaged build, and its summary.
#   bash Tools/final_run.sh [build folder]      summary: Saved/NewGameTest/final-summary.txt
cd /c/Users/Simon/Documents/Airport-Dynasty-Pro-Unreal || exit 2
PK="${1:-/f/ADP-Packaged/perf2/Windows}"; SUM=Saved/NewGameTest/final-summary.txt
[ -f "$PK/AirportDynastyPro.exe" ] || { echo "no build in $PK - not started"; exit 3; }
[ -e /f/ADP-Export/gpu.lock ] && { echo "gpu.lock is taken - not started"; exit 5; }
tasklist | grep -qi "AirportDynastyPro-Win64\|UnrealEditor" && { echo "a game or editor is running - not started"; exit 4; }
LA="$(cygpath -u "$LOCALAPPDATA")/AirportDynastyPro/Saved"; I="$LA/Config/Windows/Engine.ini"
mkdir -p Saved/NewGameTest "$LA/Config/Windows"
[ -f "$I" ] && cp "$I" "$I.before-run"
# the streaming pools this build is measured with; what was there is put back whatever happens
back() { if [ -f "$I.before-run" ]; then mv -f "$I.before-run" "$I"; else rm -f "$I"; fi; }
trap back EXIT
printf '[/Script/Engine.RendererSettings]\nr.Nanite.Streaming.StreamingPoolSize=160\n\n[/Script/Engine.VirtualTexturePoolConfig]\nDefaultSizeInMegabyte=8\n' > "$I"
touch Saved/NewGameTest/final-start
NG_LIMIT=2700 NG_CAREER=Saved/NewGameTest/chain/chaintest-3.json NG_ONLY=continue NG_SIZE="3840 1080" bash Tools/new_game_test.sh --packaged "$PK" -ChainTest=3 -RemoteStands -FeetCheck -StallWatch=30 -BoardDump "-UIPerfViews=$(cygpath -m "$PWD")/Tools/views/final.txt" > Saved/NewGameTest/final-run.out 2>&1
L=Saved/NewGameTest/continue/log.txt; PL="$LA/Logs/ADP-Perf.log"
[ "$L" -nt Saved/NewGameTest/final-start ] || { echo "the run left no new log (see Saved/NewGameTest/final-run.out)"; exit 6; }
texts() { sed -E 's/^\[[^]]*\](\[[^]]*\])?//; s/[0-9]+(\.[0-9]+)?/N/g' | cut -c1-180 | sort | uniq -c | sort -rn | head -40; }
{
  echo "== run"; tail -4 Saved/NewGameTest/final-run.out | cut -c1-200
  if grep -aq "Views: done" "$L"; then echo "the script ran to its end"; else echo "THE SCRIPT DID NOT REACH ITS END"; fi
  echo "== measures and steps"; grep -a "Views:" "$L" | grep -av "Views: lives\|subject missing\|Views: subject \|left out" | cut -c1-330
  echo "== lives"; grep -a "Views: lives" "$L" | cut -c1-600
  echo "== subjects found"; grep -a "Views: subject " "$L" | grep -av "subject missing" | cut -c1-160
  echo "== subjects missing, left out, did not come"; grep -a "Views:.*\(subject missing\|left out\|did not come\|not written\|not understood\|nobody walks\)" "$L" | cut -c1-200
  echo "== pictures"; find "$LA/Screenshots/run" -name "*.png" -newer Saved/NewGameTest/final-start 2>/dev/null | wc -l
  echo "== errors by text"; grep -a "Error\|ERROR" "$L" | texts
  echo "== warnings by text"; grep -a "Warning\|WARN" "$L" | texts
  for K in "Roads:" "Glass rain" "Boards" "Tower" "Feet:"; do echo "== $K"; grep -a "$K" "$L" | grep -av "Views: lives" | tail -12 | cut -c1-260; done
  echo "== bad frames"; grep -ac "BAD FRAME" "$PL"; grep -a "BAD FRAME" "$PL" | head -60 | cut -c1-300
  echo "== stalls"; grep -ac "STALL" "$PL"; timeout 200 bash Tools/stall_names.sh "$PK" 2>/dev/null | head -60 | cut -c1-200
} > "$SUM" 2>&1
echo "summary: $SUM ($(wc -l < "$SUM") lines)"
