#!/bin/bash
# The jetways into the game, every step (owner, 2026-11-17: "keep what you learned"): F:/ADP-Export/jetways.glb ->
# /Game/Babylon/Jetways (shared meshes jetways_mesh_<n>, Nanite on, materials made instance-safe, exact collision) and
# Data/babylon-jetways.json (the node tree the game places them from; copied to Content/Data for the packaged game).
# The flicker check (Tools/find_zfight.py -> Data/jetways-zfight.json) must have run first. Never with the editor open.
#   bash Tools/import_jetways.sh
set -e
R="$(cd "$(dirname "$0")/.." && pwd)"
ED="C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64"
GLB="F:/ADP-Export/jetways.glb"
UP="$(cygpath -w "$R/AirportDynastyPro.uproject")"
if tasklist 2>/dev/null | grep -qi "UnrealEditor.exe"; then echo "the editor is open - close it first"; exit 1; fi
[ -f "$R/Data/jetways-zfight.json" ] || { echo "run Tools/find_zfight.py $GLB Data/jetways-zfight.json first"; exit 1; }
# (LFS leaves committed .uasset files read-only: an import over them fails silently and keeps the old ones)
find "$R/Content/Babylon/Jetways" -type f -exec chmod u+w {} + 2>/dev/null || true
attrib -R "$(cygpath -w "$R/Content/Babylon/Jetways")\*" /S /D >/dev/null 2>&1 || true
before=$(stat -c %Y "$R/Content/Babylon/Jetways/jetways/StaticMeshes/jetways_mesh_0.uasset" 2>/dev/null || echo 0)
run() { "$ED/UnrealEditor-Cmd.exe" "$UP" -ExecutePythonScript="$1" -unattended -nullrhi -nosplash -NoLoadingScreen 2>&1 | grep -E "ADP_|Error|error:" | grep -v "LogPython: Error: Traceback" | tail -8; }
echo "== import"; run "$(cygpath -m "$R/Tools/import_babylon_model.py") $GLB Jetways --shared"
echo "== materials"; run "$(cygpath -m "$R/Tools/fix_babylon_materials.py") Jetways"
echo "== collision"; run "$(cygpath -m "$R/Tools/set_babylon_collision.py")"
after=$(stat -c %Y "$R/Content/Babylon/Jetways/jetways/StaticMeshes/jetways_mesh_0.uasset" 2>/dev/null || echo 0)
[ "$after" -gt "$before" ] || { echo "the import did not replace the meshes (still the old files)"; exit 1; }
echo "== node tree"; python -I "$R/Tools/glb_nodes.py" "$GLB" jetways && cp "$R/Data/babylon-jetways.json" "$R/Content/Data/babylon-jetways.json"
echo "== service door collision"; run "$(cygpath -m "$R/Tools/jetway_svc_collision.py")"   # (owner 2026-11-19: the doorway was blocked)
cp "$R/Data/jetways-zfight.json" "$R/Content/Data/jetways-zfight.json"
echo "== meshes: $(ls "$R/Content/Babylon/Jetways/jetways/StaticMeshes" | wc -l)"
echo "== done"
