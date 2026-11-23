#!/bin/bash
# THE NEW GAME TEST (owner 2026-11-23: 0.18.1's new career hung at 92 % and the creator had the world and the airport's
# interface showing through): title -> New game -> where -> she/he -> looks -> pet -> partner -> airline -> difficulty ->
# loading -> the airport, walked by itself at his screen's size, a picture of every step; then Continue the same way
# (UI/NewGameTest.cpp: -UINewGame, -UIContinueGame). One visible game copy. His careers, saves, menus' storage and
# settings are put back exactly as they were. Exit code 0: both passed, no problems.
# Usage: bash Tools/new_game_test.sh                       (the editor's game)
#        bash Tools/new_game_test.sh --packaged <folder>   (a packaged game, e.g. F:/ADP-Packaged/0.18.2/Full)
#        NG_SIZE="1920 1080" bash Tools/new_game_test.sh   (another size; NG_ONLY=new|continue: one of the two)
R="$(cd "$(dirname "$0")/.." && pwd)"; cd "$R"
read SW SH < <(powershell -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w "$R/Tools/screen_size.ps1")" | tr -d '\r')
[ -z "$SW" ] && { SW=3840; SH=1080; }
[ -n "$NG_SIZE" ] && read SW SH <<< "$NG_SIZE"
PACK=""; [ "$1" = "--packaged" ] && { PACK="$(cygpath -u "$2")"; shift 2; }
OUT="$R/Saved/NewGameTest"; mkdir -p "$OUT"
if [ -n "$PACK" ]; then S="$(cygpath -u "$LOCALAPPDATA")/AirportDynastyPro/Saved"; CFG=Windows
  tasklist | grep -qiE "AirportDynastyPro" && { echo "STOP: a packaged game is running"; exit 1; }
else S="$R/Saved"; CFG=WindowsEditor
  tasklist | grep -qi "UnrealEditor.exe" && powershell -NoProfile -Command "if (Get-CimInstance Win32_Process -Filter \"Name='UnrealEditor.exe'\" | Where-Object { \$_.CommandLine -match '-game' }) { exit 1 }" || { tasklist | grep -qi "UnrealEditor.exe" && { echo "STOP: a game copy is running"; exit 1; }; }
fi
# everything a new career writes, put aside
KEEP=$(mktemp -d "$OUT/keep-XXXXXX")
PARTS="Careers Saves Autosaves ADP webcache_6613 Config/Windows/GameUserSettings.ini Config/WindowsEditor/GameUserSettings.ini LastActivity.txt"
for p in $PARTS; do [ -e "$S/$p" ] && { mkdir -p "$KEEP/$(dirname $p)"; cp -rp "$S/$p" "$KEEP/$p" || { echo "STOP: $p couldn't be put aside"; rm -rf "$KEEP"; exit 1; }; }; done
putback() { for p in $PARTS; do rm -rf "$S/$p"; [ -e "$KEEP/$p" ] && { mkdir -p "$S/$(dirname $p)"; cp -rp "$KEEP/$p" "$S/$p"; }; done; rm -rf "$KEEP"; }
trap putback EXIT
BAD=0
for WHICH in new continue; do
  [ -n "$NG_ONLY" ] && [ "$NG_ONLY" != "$WHICH" ] && continue
  SW1=-UINewGame; [ $WHICH = continue ] && SW1=-UIContinueGame
  touch "$OUT/stamp"
  if [ -n "$PACK" ]; then
    ADP_LOG=1 timeout 660 "$PACK/AirportDynastyPro.exe" $SW1 -windowed -ResX=$SW -ResY=$SH -ForceRes "$@" > /dev/null 2>&1
    taskkill //IM AirportDynastyPro-Win64-Shipping.exe //F > /dev/null 2>&1
    L="$S/Logs/ADP.log"
  else
    ADP_LOG=1 timeout 660 "/c/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor.exe" "$(cygpath -w $PWD/AirportDynastyPro.uproject)" -game $SW1 -windowed -ResX=$SW -ResY=$SH -ForceRes -Site=chablais -LocalTerrain -nosplash -log=NewGameTest.log "$@" > /dev/null 2>&1
    powershell -NoProfile -Command "Get-Process CrashReportClientEditor -ErrorAction SilentlyContinue | Stop-Process -Force" > /dev/null 2>&1
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
