#!/bin/bash
# One 4K picture: one.sh <shot id>. Takes its turn on the GPU (no game window, no other background Blender), renders,
# labels, commits and pushes. Never starts a second copy of itself.
cd /c/Users/Simon/Documents/Airport-Dynasty-Pro-Unreal || exit 1
D=Blueprints/Terminal/Changes/Detailed; OUTD=docs/Blueprints/Terminal-Changes/Detailed; id=$1
line=$(grep "^$id|" $D/captions.txt) || { echo "no caption for $id"; exit 1; }
name=$(echo "$line" | cut -d'|' -f2); cap=$(echo "$line" | cut -d'|' -f3); sub=$(echo "$line" | cut -d'|' -f4)
busy() { powershell -NoProfile -Command "@(Get-CimInstance Win32_Process | Where-Object { (\$_.Name -like 'UnrealEditor*' -and \$_.CommandLine -match ' -game') -or (\$_.Name -like 'AirportDynastyPro*') -or (\$_.Name -eq 'blender.exe' -and \$_.CommandLine -match ' -b ') }).Count" < /dev/null | tr -d '\r'; }
export TC_W=${TC_W:-3840} TC_H=${TC_H:-2160} TC_SAMPLES=${TC_SAMPLES:-128} TC_OUT=${TC_OUT:-F:/ADP-Export/tc_raw}
for k in $(seq 1 2000); do [ "$(busy)" = "0" ] && break; sleep 3; [ $k = 2000 ] && { echo "BUSY $id"; exit 2; }; done
"/c/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b F:/ADP-Export/tc_after.blend --python $D/render_shot.py -- $id > F:/ADP-Export/tc_f_$id.log 2>&1 < /dev/null
if grep -qE "out of GPU memory|Out of memory|Traceback" F:/ADP-Export/tc_f_$id.log; then echo "FAILED $id"; exit 3; fi
[ -n "$TC_TEST" ] && { echo "TESTED $name"; exit 0; }
python $D/annotate.py $TC_OUT $OUTD "$name" "$cap" "$sub" < /dev/null
git add "$OUTD/$name.png" && git commit -qm "Terminal changes, detailed renders: $name" -- "$OUTD/$name.png" < /dev/null
git push -q < /dev/null 2>&1 | grep -iE "error|reject" ; echo "DONE $name"
