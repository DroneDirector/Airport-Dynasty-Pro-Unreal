#!/bin/bash
# One pet from the owner's Meshy model to its moves, every step in order (each step starts from the one before, so a
# change anywhere is re-run from here):  bash Art/Pets/build_pet.sh <Dogs|Cats> <Name> <ear-tip height m> <faces>
#   make_pet.py   facing forward, real size, reduced, new UVs, colour + surface detail baked from the full model
#   rig_pet.py    the skeleton from pet_skeletons.json (with the opening mouth where it has one), the skin's weights
#   anim_pet.py   the moves, all in place; speeds to <Name>_moves.json
set -e
cd "$(dirname "$0")"
B="/f/Tools/Blender-4.2/blender.exe"
for s in "make_pet.py -- $1 $2 $3 $4" "rig_pet.py -- $1 $2" "anim_pet.py -- $1 $2"; do
  echo "== $s"
  "$B" -b --python $s 2>&1 | grep -E "^(PET|RIG|MOUTH|MOVE|MOVES)|Error|Traceback|line [0-9]" || true
done
