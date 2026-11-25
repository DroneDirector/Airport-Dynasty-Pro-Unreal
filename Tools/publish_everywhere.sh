#!/bin/bash
# Publishes a built release (Tools/release_pc.sh done, its checks passed): GitHub, then the game behind the owner's icons.
#   bash Tools/publish_everywhere.sh 0.19.0 0.18.2 0.18.1
cd "$(dirname "$0")/.." || exit 1
V=$1; A=$2; B=$3; P=/f/ADP-Packaged; G="/f/Airport Dynasty Pro Unreal"
[ -n "$B" ] || { echo "STOP: three versions needed"; exit 1; }
EXE=AirportDynastyPro/Binaries/Win64/AirportDynastyPro-Win64-Shipping.exe
[ -f "$P/$V/built-and-checked" ] || { echo "STOP: $V is not built and checked yet - nothing was published"; exit 1; }
tasklist | grep -qi "AirportDynastyPro" && { echo "STOP: close the game first, then start this again"; exit 1; }
DRAFT=$(gh release view pc-unreal-v$V --json isDraft -q .isDraft 2>/dev/null)
[ "$DRAFT" = "true" ] && { echo "== an upload that was cut off: started over"; gh release delete pc-unreal-v$V -y; DRAFT=; }
if [ "$DRAFT" = "false" ]; then
  echo "== $V is on GitHub already: its files are not sent again"
  git pull -q --rebase --autostash && git add pc-version.json && { git diff --cached --quiet || git -c user.name=DroneDirector -c user.email=simon@dronedirector.fo commit -qm "PC $V"; } && git push -q
else
  bash Tools/release_pc.sh $V $A $B --upload || exit 1
fi
git fetch -q; git show origin/main:pc-version.json | grep -q "\"$V\"" || { echo "STOP: pc-version.json on GitHub is not $V"; exit 1; }
echo "== this PC: the game behind the icons"
win() { cygpath -w "$1"; }
MSYS_NO_PATHCONV=1 robocopy "$(win $P/$V/Full)" "$(win "$G")" /E /R:2 /W:2 /NFL /NDL /NJH /NP /XD "$(win "$G/World Terrain")" > "$P/pc-copy-$V.log"; [ $? -lt 8 ] || { echo "STOP: the copy failed ($P/pc-copy-$V.log)"; exit 1; }
MSYS_NO_PATHCONV=1 robocopy "$(win $P/$V/Full/AirportDynastyPro/Content/Paks)" "$(win "$G/AirportDynastyPro/Content/Paks")" /MIR /R:2 /W:2 /NFL /NDL /NJH /NP >> "$P/pc-copy-$V.log"; [ $? -lt 8 ] || { echo "STOP: the packs' copy failed"; exit 1; }
cmp -s "$P/$V/Full/$EXE" "$G/$EXE" || { echo "STOP: the exe behind the icons isn't $V's"; exit 1; }
echo "== done: $V is on GitHub and on this PC ($G)"
