#!/bin/bash
# THE CINEMATIC TEST (owner 2026-11-21: "save this exact test ... call it the cinematic test"). One visible game copy at
# his screen size, daytime (13:00), with the tracking camera on everything working at once:
#   the pushback tug + crew (-UITugShot -SimTrafficShot -TugQuick -TugCheck), the ground vehicles - tractors, cart trains,
#   belt loader, arriving + departing bags (-GseTest), the departure belts from the check-in desks to the make-up
#   carousels (-DepBagTest), the passengers from the car park to the jetway and arrivals back to the claim belt and the
#   car park (-PaxFlowTest). The camera takes turns between the vehicles and a passenger; it ends by itself when all is
#   done (max 45 min). Saves/restores his GameUserSettings; closes a crash reporter left behind.
# Usage: bash Tools/cinematic_test.sh [extra flags, e.g. -ShotHour=22 for night]
R="$(cd "$(dirname "$0")/.." && pwd)"; cd "$R"
read SW SH < <(powershell -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w "$R/Tools/screen_size.ps1")" | tr -d '\r')
[ -z "$SW" ] && { SW=1920; SH=1080; }
for d in Saved/Config/WindowsEditor Saved/Config/Windows; do [ -f $d/GameUserSettings.ini ] && cp $d/GameUserSettings.ini $d/GameUserSettings.ini.bak-test; done
timeout 2700 "/c/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor.exe" "$(cygpath -w $PWD/AirportDynastyPro.uproject)" -game -windowed -ResX=$SW -ResY=$SH -ForceRes -Site=chablais -LocalTerrain -UIShot \
  -UIChablais -SimTrafficShot -UITugShot -ShotHour=13 -TugQuick -GseTest -DepBagTest -PaxFlowTest -TugCheck "$@" -nosplash -log=CinematicTest.log > /dev/null 2>&1
for d in Saved/Config/WindowsEditor Saved/Config/Windows; do [ -f $d/GameUserSettings.ini.bak-test ] && mv -f $d/GameUserSettings.ini.bak-test $d/GameUserSettings.ini; done
powershell -NoProfile -Command "Get-Process CrashReportClientEditor -ErrorAction SilentlyContinue | Stop-Process -Force" > /dev/null 2>&1
echo "cinematic test done ($SW x $SH) - log: Saved/Logs/CinematicTest.log"
