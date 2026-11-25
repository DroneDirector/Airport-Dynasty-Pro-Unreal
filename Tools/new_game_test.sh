#!/bin/bash
# THE NEW GAME TEST (owner 2026-11-23: 0.18.1's new career hung at 92 % and the creator had the world and the airport's
# interface showing through): title -> New game -> where -> she/he -> looks -> pet -> partner -> airline -> difficulty ->
# loading -> the airport, walked by itself at his screen's size, a picture of every step; then Continue the same way
# (UI/NewGameTest.cpp: -UINewGame, -UIContinueGame). One visible game copy. His careers, saves, menus' storage and
# settings are put back exactly as they were. Exit code 0: both passed, no problems.
# Usage: bash Tools/new_game_test.sh                       (the editor's game)
#        bash Tools/new_game_test.sh --packaged <folder>   (a packaged game, e.g. F:/ADP-Packaged/0.18.2/Full)
#        bash Tools/new_game_test.sh --career [--from N]   (a new career, then the whole passenger and crew chain in it:
#          buy, hire, the timetable, two departures, the arrival back, the second departure - Private/ChainTest.cpp; about an
#          hour; a save point at the start of each stage in Saved/NewGameTest/chain, --from N goes on from there)
#        NG_SIZE="1920 1080" bash Tools/new_game_test.sh   (another size; NG_ONLY=new|continue: one of the two)
R="$(cd "$(dirname "$0")/.." && pwd)"; cd "$R"
read SW SH < <(powershell -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w "$R/Tools/screen_size.ps1")" | tr -d '\r')
[ -z "$SW" ] && { SW=3840; SH=1080; }
[ -n "$NG_SIZE" ] && read SW SH <<< "$NG_SIZE"
PACK=""; [ "$1" = "--packaged" ] && { PACK="$(cygpath -u "$2")"; shift 2; }
OUT="$R/Saved/NewGameTest"; mkdir -p "$OUT"
CAREER=0; FROM=1
[ "$1" = "--career" ] && { CAREER=1; shift; [ "$1" = "--from" ] && { FROM="$2"; shift 2; }; }
if [ -n "$PACK" ]; then S="$(cygpath -u "$LOCALAPPDATA")/AirportDynastyPro/Saved"; CFG=Windows
  tasklist | grep -qiE "AirportDynastyPro" && { echo "STOP: a packaged game is running"; exit 1; }
else S="$R/Saved"; CFG=WindowsEditor
  tasklist | grep -qi "UnrealEditor.exe" && powershell -NoProfile -Command "if (Get-CimInstance Win32_Process -Filter \"Name='UnrealEditor.exe'\" | Where-Object { \$_.CommandLine -match '-game' }) { exit 1 }" || { tasklist | grep -qi "UnrealEditor.exe" && { echo "STOP: a game copy is running"; exit 1; }; }
fi
# everything a new career writes, put aside
KEEP=$(mktemp -d "$OUT/keep-XXXXXX")
PARTS="Careers Saves Autosaves ADP webcache_6613 Config/Windows/GameUserSettings.ini Config/WindowsEditor/GameUserSettings.ini LastActivity.txt"
for p in $PARTS; do [ -e "$S/$p" ] && { mkdir -p "$KEEP/$(dirname $p)"; cp -rp "$S/$p" "$KEEP/$p" || { echo "STOP: $p couldn't be put aside"; rm -rf "$KEEP"; exit 1; }; }; done
# (2026-11-25: files still open right after the game's end kept the old folder there, and the copy landed INSIDE it - the menus
# had lost their save. So: waited until every helper is gone, the folder's removal checked, and if it can't be put back the copy stays.)
putback() { local OK=1 p i
  # (only the game's own page helpers, told by their command line - the Epic launcher has the same ones)
  for i in $(seq 1 30); do [ "$(powershell -NoProfile -Command "@(Get-CimInstance Win32_Process | Where-Object { \$_.Name -like 'AirportDynastyPro-Win64*' -or (\$_.Name -eq 'EpicWebHelper.exe' -and \$_.CommandLine -match 'AirportDynastyPro') }).Count" | tr -d '\r')" = 0 ] && break; sleep 1; done
  for p in $PARTS; do
    for i in $(seq 1 20); do rm -rf "$S/$p" 2>/dev/null; [ -e "$S/$p" ] || break; sleep 1; done
    [ -e "$S/$p" ] && { echo "PUT BACK FAILED: $S/$p is still open - the copy stays in $KEEP"; OK=0; continue; }
    [ -e "$KEEP/$p" ] && { mkdir -p "$S/$(dirname $p)"; cp -rpT "$KEEP/$p" "$S/$p" || { echo "PUT BACK FAILED: $p - the copy stays in $KEEP"; OK=0; }; }
  done; [ $OK = 1 ] && rm -rf "$KEEP"; }
trap putback EXIT
# a crash window of this test's own game copy (owner 2026-11-23: read, then closed at once - never left on his screen): its
# crash kept in Saved/NewGameTest/crashes, the window watching that PID closed; never any other window
gamepid() { powershell -NoProfile -Command "(Get-CimInstance Win32_Process -Filter \"Name='UnrealEditor.exe'\" | Where-Object { \$_.CommandLine -match '$1' } | Select-Object -First 1).ProcessId" | tr -d ''; }
closecrash() { [ -z "$1" ] && return; powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='CrashReportClientEditor.exe'\" | Where-Object { \$_.CommandLine -match '-MONITOR=$1(\D|\$)' } | ForEach-Object { Stop-Process -Id \$_.ProcessId -Force }" > /dev/null 2>&1
  for c in $(find "$S/Crashes" -maxdepth 1 -type d -newer "$OUT/stamp" -name "UECC*" 2>/dev/null); do mkdir -p "$OUT/crashes"; cp -rp "$c" "$OUT/crashes/"; done; }
if [ $CAREER = 1 ]; then
  CH="$OUT/chain"; mkdir -p "$CH"; SW1=-UINewGame
  if [ "$FROM" -gt 1 ]; then
    [ -f "$CH/chaintest-$FROM.json" ] || { echo "STOP: no save point of stage $FROM"; exit 1; }
    mkdir -p "$S/Careers"; cp "$CH/chaintest-$FROM.json" "$S/Careers/career-chablais.json"
  else rm -f "$S/Careers/career-chablais.json"; fi
  read SW SH <<< "${NG_SIZE:-1600 900}"
  EX="/c/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor.exe"
  # (an own time limit and a kill of this copy only: the game ends by itself when the chain is through or stuck)
  ADP_LOG=1 timeout ${NG_LIMIT:-10800} "$EX" "$(cygpath -w $PWD/AirportDynastyPro.uproject)" -game $SW1 -ChainTest=$FROM -windowed -ResX=$SW -ResY=$SH -ForceRes -Site=chablais -LocalTerrain -nosplash -log=ChainTest.log "$@" > /dev/null 2>&1 &
  RUN=$!; touch "$OUT/stamp"; GP=""; for i in 1 2 3 4 5 6 7 8 9 10 11 12; do sleep 5; GP=$(gamepid 'log=ChainTest'); [ -n "$GP" ] && break; done
  wait $RUN; sleep 3; closecrash "$GP"
  cp "$S/Logs/ChainTest.log" "$CH/log-from-$FROM.txt" 2>/dev/null
  for f in "$S"/Careers/chaintest-*.json; do [ -f "$f" ] && cp "$f" "$CH/"; done
  echo "== career chain from stage $FROM:"
  grep -aE "CHAIN TEST: (started|stage|STAGE|RESULT|PROBLEM|save point|FAILED|ALL PASSED|buy|slot|hire|before|flying)|NEWGAME TEST: FAILED|left behind|no way from A" "$CH/log-from-$FROM.txt" | sed 's/^\[[^]]*\]\[[^]]*\]//; s/LogTemp: //; s/Display: //' | cut -c1-260
  grep -qa "CHAIN TEST: ALL PASSED" "$CH/log-from-$FROM.txt" && { echo "career chain: PASSED"; exit 0; } || { echo "career chain: FAILED - Saved/NewGameTest/chain/log-from-$FROM.txt"; exit 1; }
fi
BAD=0
for WHICH in new continue; do
  [ -n "$NG_ONLY" ] && [ "$NG_ONLY" != "$WHICH" ] && continue
  SW1=-UINewGame; [ $WHICH = continue ] && SW1=-UIContinueGame
  touch "$OUT/stamp"
  if [ -n "$PACK" ]; then
    ADP_LOG=1 timeout 660 "$PACK/AirportDynastyPro.exe" $SW1 -windowed -ResX=$SW -ResY=$SH -ForceRes -Site=chablais -LocalTerrain "$@" > /dev/null 2>&1
    taskkill //IM AirportDynastyPro-Win64-Shipping.exe //F > /dev/null 2>&1
    L="$S/Logs/ADP.log"
  else
    ADP_LOG=1 timeout 660 "/c/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor.exe" "$(cygpath -w $PWD/AirportDynastyPro.uproject)" -game $SW1 -windowed -ResX=$SW -ResY=$SH -ForceRes -Site=chablais -LocalTerrain -nosplash -log=NewGameTest.log "$@" > /dev/null 2>&1 &
    RUN=$!; GP=""; for i in 1 2 3 4 5 6 7 8 9 10 11 12; do sleep 5; GP=$(gamepid 'log=NewGameTest'); [ -n "$GP" ] && break; done
    wait $RUN; sleep 3; closecrash "$GP"
    L="$S/Logs/NewGameTest.log"
  fi
  rm -rf "$OUT/$WHICH"; mkdir -p "$OUT/$WHICH"
  find -L "$S/Screenshots" -newer "$OUT/stamp" \( -name "NewGame_*.png" -o -name "Continue_*.png" \) -exec mv {} "$OUT/$WHICH/" \;
  cp "$L" "$OUT/$WHICH/log.txt" 2>/dev/null
  echo "== $WHICH ($SW x $SH):"; grep -aE "NEWGAME TEST|New game loading: (everything|180)|Loading: (done|210)|navmesh ready|Uncaught" "$OUT/$WHICH/log.txt" | sed 's/^\[[^]]*\]\[[^]]*\]//; s/LogTemp: //' | grep -vE "picture|loading [0-9]+ s" | cut -c1-240
  grep -qa "NEWGAME TEST: PASSED" "$OUT/$WHICH/log.txt" || BAD=1
done
[ $BAD = 0 ] && echo "new game test: PASSED - pictures in Saved/NewGameTest" || echo "new game test: FAILED - see Saved/NewGameTest"
exit $BAD
