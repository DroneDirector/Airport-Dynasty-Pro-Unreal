#!/bin/bash
# The PC release, every step in the order that works (owner, 2026-11-14: hard-won fixes go into the scripts). Lessons:
#   - bump pc-version.json BEFORE packaging: the version is compiled into the exe (0.17.12 shipped saying 0.17.11)
#   - the web pages and Data are copied into Content first (the game reads Content/Web and Content/Data)
#   - patch cook based on 0.12.1 (-generatepatch); -iterate can't be used with patching (full cook, ~40-60 min)
#   - SMALL UPDATES (owner, 2026-11-16: to-do 50): the patch is compared with the previous release's own content packs,
#     copied into Releases/0.12.1/Windows first (the base release has no pakchunk*-Windows packs of its own, so before
#     this nothing was compared and every update resent all the content, ~1.4 GB); -addpatchlevel puts this release's
#     changes in a new numbered pack (pakchunkN-Windows_K_P), the earlier ones stay as they are in installed games
#   - the full game = the previous version's full game + this version's Windows on top; start-checked (it starts and runs without crashing) with ADP_LOG=1
#   - update zips from the two previous versions (Tools/MakeUpdate.py); the installer via Inno Setup
#   - GitHub takes files under 2 GiB: the installer is split (Setup.exe + Setup-N.bin, Installer/setup.iss DiskSpanning);
#     no full zip on GitHub; every file must be on the release before pc-version.json is pushed - this script pushes it
#     itself, only after checking (0.17.14's first try pushed the version with no release: the updater saw nothing to get)
#   bash Tools/release_pc.sh 0.17.13 0.17.12 0.17.11            (builds everything, stops before uploading)
#   bash Tools/release_pc.sh 0.17.13 0.17.12 0.17.11 --upload   (then uploads the GitHub release)
set -e
V=$1; A=$2; B=$3
R=/c/Users/Simon/Documents/Airport-Dynasty-Pro-Unreal; P=/f/ADP-Packaged
cd $R
grep -q "\"$V\"" pc-version.json || { echo "STOP: pc-version.json is not $V - bump it first"; exit 1; }
if [ "$4" != "--upload" ]; then
  if [ "$4" != "--after-package" ]; then   # (--after-package: the package in F:/ADP-Packaged/$V is done, carry on from the exe check)
  tasklist | grep -qi "UnrealEditor.exe" && { echo "STOP: an Unreal editor is running"; exit 1; }
  # the holographic map traced from the terminal as it stands (owner, 2026-11-19: every new part on the map, always):
  # the editor's own build first, then its game cuts every wall at walking height -> Data/holomap-walls.json
  echo "== the holographic map's walls traced from the terminal"
  "C:/Program Files/Epic Games/UE_5.7/Engine/Build/BatchFiles/Build.bat" AirportDynastyProEditor Win64 Development "-Project=$(cd $R && pwd -W)/AirportDynastyPro.uproject" -WaitMutex > /dev/null 2>&1 || { echo "STOP: the editor build failed"; exit 1; }
  BEFORE=$(stat -c %Y Data/holomap-walls.json 2>/dev/null || echo 0)
  timeout 600 "C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor.exe" "$(cd $R && pwd -W)/AirportDynastyPro.uproject" -game -UIShot -UIMapTrace -Site=chablais -LocalTerrain -windowed -ResX=1280 -ResY=720 > /dev/null 2>&1
  AFTER=$(stat -c %Y Data/holomap-walls.json 2>/dev/null || echo 0)
  [ "$AFTER" -gt "$BEFORE" ] || { echo "STOP: the map trace didn't write Data/holomap-walls.json"; exit 1; }
  grep -a "Map trace:" Saved/Logs/AirportDynastyPro.log | tail -1 | sed 's/.*Map trace: /   /'
  echo "== web pages and data into Content"
  rm -rf Content/Web && cp -r Airline-Manager-Pro-Babylon/www Content/Web && rm -rf Content/Data && cp -r Data Content/Data
  # the passengers' walking map ready-made from the terminal as it stands (owner, 2026-11-23: it was made again on every
  # start, a minute and more) - after the data's copy: it is made from exactly what is shipped, and carries that as its key
  echo "== the walking map baked"
  bash Tools/bake_paxnav.sh || { echo "STOP: the walking map wasn't baked"; exit 1; }
  echo "== every folder the code loads by name is always cooked"
  python - <<'PYEOF' || { echo "STOP: add the folders above to DirectoriesToAlwaysCook in Config/DefaultGame.ini"; exit 1; }
import re, glob, sys
# (0.17.16 shipped without its escalator, road signs, street lights and more: the code loads them by name, nothing in a
#  level points at them, so the cook left them out, owner 2026-11-16. Folders already reached through a level are listed.)
SKIP = {'Characters', 'Maps'}
ini = open('Config/DefaultGame.ini', encoding='utf-8').read()
cooked = set(re.findall(r'DirectoriesToAlwaysCook=\(Path="/Game/([^"/]+)', ini))
used = set()
for f in glob.glob('Source/AirportDynastyPro/**/*.*', recursive=True):
    used |= set(re.findall(r'TEXT\("/Game/([A-Za-z0-9_]+)/', open(f, encoding='utf-8', errors='ignore').read()))
miss = sorted(used - cooked - SKIP)
if miss: print('not always cooked:', ', '.join(miss)); sys.exit(1)
print('all %d folders the code loads are cooked' % len(used - SKIP))
PYEOF
  echo "== frame-by-frame scans checked (a heavy one is a stutter)"
  python Tools/find_world_scans.py || { echo "STOP: a new scan over many objects - see above"; exit 1; }
  echo "== the previous release's content packs as the patch's base (only what changed since $A goes in the new pack)"
  REL=Releases/0.12.1/Windows
  cp -f $P/$A/Full/AirportDynastyPro/Content/Paks/pakchunk*-Windows_*_P.* $REL/ 2>/dev/null || true
  ls $REL | grep -c "_P\." | sed 's/^/  packs to compare with: /'
  echo "== cook and package (patch on 0.12.1 + the packs since)"
  rm -rf $P/$V
  "/c/Program Files/Epic Games/UE_5.7/Engine/Build/BatchFiles/RunUAT.bat" BuildCookRun -project="$(pwd -W)/AirportDynastyPro.uproject" -noP4 -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -generatepatch -addpatchlevel -basedonreleaseversion=0.12.1 -archive -archivedirectory="F:\\ADP-Packaged\\$V" -utf8output -unattended -IgnoreCookErrors > $P/uat-$V.log 2>&1 || { tail -20 $P/uat-$V.log; echo "STOP: packaging failed"; exit 1; }
  # (the cook goes on past errors only for one known engine check, "SerializedElementSize == ElementSize" in Array.h:
  #  a handled ensure the full cook logs once while it builds skinned meshes, every package still cooked, 2026-11-16;
  #  any other cook error still stops the release here)
  CK=$(ls -t "$APPDATA/Unreal Engine/AutomationTool/Logs/C+Program+Files+Epic+Games+UE_5.7"/Cook-*.txt | head -1)
  OTHER=$(sed -n '/Warning\/Error Summary/,$p' "$CK" | grep "Error:" | grep -vE "stack for UAT|Handled ensure|SerializedElementSize == ElementSize|Error: Stack:|\[Callstack\]|Error: *$" | head -5)
  [ -z "$OTHER" ] || { echo "$OTHER"; echo "STOP: cook errors"; exit 1; }
  grep -q "Cook failed" $P/uat-$V.log && echo "(cook: only the known engine check - packaging went on)"
  grep -q "BUILD SUCCESSFUL" $P/uat-$V.log || { tail -20 $P/uat-$V.log; echo "STOP: no BUILD SUCCESSFUL"; exit 1; }
  fi
  echo "== the version inside the exe"
  IN=$(python -c "
import re
b=open(r'F:/ADP-Packaged/$V/Windows/AirportDynastyPro/Binaries/Win64/AirportDynastyPro-Win64-Shipping.exe','rb').read()
print(' '.join(sorted(set(m.group(0).decode('utf-16-le') for m in re.finditer(rb'0\x00\.\x001\x00[0-9]\x00\.\x00[0-9]\x00(?:[0-9]\x00)?', b)))))")
  echo "inside: $IN"; echo "$IN" | grep -q "$V" || { echo "STOP: the exe doesn't carry $V"; exit 1; }
  echo "== the full game, and a start check"
  rm -rf $P/$V/Full; cp -r $P/$A/Full $P/$V/Full; cp -r $P/$V/Windows/. $P/$V/Full/   # (the earlier packs stay; this release's pack added)
  L="$LOCALAPPDATA/AirportDynastyPro/Saved/Logs"; rm -f "$L/ADP.log"
  (cd $P/$V/Full && ADP_LOG=1 timeout 90 ./AirportDynastyPro/Binaries/Win64/AirportDynastyPro-Win64-Shipping.exe -unattended -RenderOffscreen -windowed -ResX=640 -ResY=360 > /dev/null 2>&1) || true
  C=$(grep -c CRASH "$L/ADP.log" 2>/dev/null || true); echo "start check crashes: ${C:-none logged}"
  [ "$C" = "0" ] || [ -z "$C" ] || { grep -h CRASH -A5 "$L/ADP.log" | head -20; echo "STOP: it crashed"; exit 1; }
  # a new career from the title screen to the airport, and Continue, in the packaged game (owner, 2026-11-23: 0.18.1 was
  # released with a new game that never finished loading - the start check alone doesn't press anything)
  echo "== the new game test in the packaged game"
  bash Tools/new_game_test.sh --packaged $P/$V/Full || { echo "STOP: the new game test failed"; exit 1; }
  echo "== update zips"
  rm -f $P/AirportDynastyPro-Unreal-PC-Update-*-to-$V.zip
  python $R/Tools/MakeUpdate.py --old $P/$A/Full --new $P/$V/Full --from $A --to $V | tail -1
  python $R/Tools/MakeUpdate.py --old $P/$B/Full --new $P/$V/Full --from $B --to $V | tail -1
  echo "== installer"
  export ADP_VERSION=$V; export ADP_BUILD="F:\\ADP-Packaged\\$V\\Full"
  "$LOCALAPPDATA/Programs/Inno Setup 6/ISCC.exe" Installer/setup.iss > $P/iscc-$V.log 2>&1; tail -1 $P/iscc-$V.log
  ls -la Packaged/*$V* $P/*to-$V.zip
  echo "== built: run again with --upload to publish"
else
  echo "== GitHub release"
  rm -f Packaged/*$V*.zip.bad 2>/dev/null
  FILES="Packaged/AirportDynastyPro-Unreal-PC-Setup-$V.exe $(ls Packaged/AirportDynastyPro-Unreal-PC-Setup-$V-*.bin 2>/dev/null) $P/AirportDynastyPro-Unreal-PC-Update-$A-to-$V.zip $P/AirportDynastyPro-Unreal-PC-Update-$B-to-$V.zip"
  for F in $FILES; do S=$(stat -c %s "$F"); [ "$S" -lt 2147483648 ] || { echo "STOP: $F is over 2 GiB"; exit 1; }; done
  gh release create pc-unreal-v$V --prerelease --title "Airport Dynasty Pro (Unreal) $V" --notes-file release-notes-pc/$V.md $FILES 2>&1 | tail -2
  N=$(echo $FILES | wc -w); GOT=$(gh release view pc-unreal-v$V --json assets -q '.assets | length' 2>/dev/null || echo 0)
  gh release view pc-unreal-v$V --json assets -q '.assets[] | "\(.name) \(.size)"'
  [ "$GOT" = "$N" ] || { echo "STOP: $GOT of $N files on the release - pc-version.json NOT pushed"; exit 1; }
  echo "== pc-version.json (the updater sees $V now)"
  git pull -q --rebase --autostash && git add pc-version.json release-notes-pc/$V.md && { git diff --cached --quiet || git -c user.name=DroneDirector -c user.email=simon@dronedirector.fo commit -qm "PC $V"; } && git push -q   # (already committed: just pushed)
fi
