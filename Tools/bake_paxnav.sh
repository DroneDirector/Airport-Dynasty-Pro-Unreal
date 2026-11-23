#!/bin/bash
# The walkers' navigation map made once, for the release (owner 2026-11-23: it was made again on every start, a minute and
# more): the editor's game builds the terminal, the roads and the town, generates the map (PaxNav.cpp) and writes it to the
# site's data folder (Data/Sites/<id>/paxnav.bin); the game loads it in a moment. Run by release_pc.sh;
# by hand after the terminal, the roads or the walker changed (without it the game makes it itself on its first start).
# Usage: bash Tools/bake_paxnav.sh [site ...]      (default: every site in Data/Sites - Chablais; Iceland is not baked, owner 2026-11-23)
R="$(cd "$(dirname "$0")/.." && pwd)"; cd "$R"
SITES="$*"; [ -z "$SITES" ] && SITES="$(ls Data/Sites)"
for d in Saved/Config/WindowsEditor Saved/Config/Windows; do [ -f $d/GameUserSettings.ini ] && cp $d/GameUserSettings.ini $d/GameUserSettings.ini.bak-bake; done
back() { for d in Saved/Config/WindowsEditor Saved/Config/Windows; do [ -f $d/GameUserSettings.ini.bak-bake ] && mv -f $d/GameUserSettings.ini.bak-bake $d/GameUserSettings.ini; done; }
trap back EXIT
BAD=0
for ID in $SITES; do
  F=Data/Sites/$ID/paxnav.bin; ARG="-Site=$ID"
  touch Saved/paxnav-stamp
  ADP_LOG=1 timeout 900 "/c/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor.exe" "$(cygpath -w $PWD/AirportDynastyPro.uproject)" -game -PaxNavBake -LateWorldNow $ARG -LocalTerrain -windowed -ResX=1280 -ResY=720 -nosplash -log=PaxNavBake.log > /dev/null 2>&1
  [ $F -nt Saved/paxnav-stamp ] || { echo "STOP: $F wasn't written"; exit 1; }
  # (the game reads Content/Data where it is there: the same map in both)
  [ -d Content/Data ] && cp -f $F Content/${F}
  grep -a "PaxNav bake:" Saved/Logs/PaxNavBake.log | tail -1 | sed "s/.*PaxNav bake: /   /"
  # (every leg a passenger walks, car park to gate and back out, and the ways that must stay shut: a failed one stops the release)
  grep -a "PaxNav leg" Saved/Logs/PaxNavBake.log | sed "s/.*PaxNav leg /   /"
  grep -a "PaxNav legs:" Saved/Logs/PaxNavBake.log | tail -1 | sed "s/.*PaxNav legs: /   /"
  grep -qa "PaxNav legs: ALL REACHED" Saved/Logs/PaxNavBake.log || { echo "FAILED: the $ID walking map has legs passengers can't walk (the FAILED lines above)"; BAD=1; }
done
rm -f Saved/paxnav-stamp
exit $BAD
